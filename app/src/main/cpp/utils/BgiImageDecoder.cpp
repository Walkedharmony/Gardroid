#include "BgiImageDecoder.h"
#include <cstring>
#include <algorithm>
#include <vector>

namespace {
    class MsbBitStream {
        const uint8_t* m_data;
        size_t m_size, m_pos;
        uint32_t m_cache;
        int m_bitsCached;
    public:
        MsbBitStream(const uint8_t* data, size_t size) : m_data(data), m_size(size), m_pos(0), m_cache(0), m_bitsCached(0) {}
        int GetNextBit() {
            if (m_bitsCached == 0) {
                if (m_pos >= m_size) return -1;
                m_cache = m_data[m_pos++];
                m_bitsCached = 8;
            }
            int bit = (m_cache >> (m_bitsCached - 1)) & 1;
            m_bitsCached--;
            return bit;
        }
        int GetBits(int count) {
            int result = 0;
            while (count > 0) {
                if (m_bitsCached == 0) {
                    if (m_pos >= m_size) return result << count;
                    m_cache = m_data[m_pos++];
                    m_bitsCached = 8;
                }
                int take = (count < m_bitsCached) ? count : m_bitsCached;
                int shift = m_bitsCached - take;
                result = (result << take) | ((m_cache >> shift) & ((1 << take) - 1));
                m_bitsCached -= take;
                count -= take;
            }
            return result;
        }
        int GetCacheSize() const { return m_bitsCached; }
    };

    struct HuffmanNode {
        bool Valid, IsParent;
        uint32_t Weight;
        int LeftChildIndex, RightChildIndex;
    };

    class HuffmanTree {
        std::vector<HuffmanNode> m_nodes;
    public:
        HuffmanTree(const std::vector<uint32_t>& weights, bool v2 = false) {
            uint32_t root_weight = 0;
            for (uint32_t w : weights) {
                m_nodes.push_back({ w != 0, false, w, -1, -1 });
                root_weight += w;
            }

            int child[2];
            while (true) {
                uint32_t weight = 0;
                for (int i = 0; i < 2; i++) {
                    uint32_t min_weight = 0xFFFFFFFF;
                    child[i] = -1;
                    int n = 0;
                    if (v2) {
                        for (; n < m_nodes.size(); ++n) {
                            if (m_nodes[n].Valid) {
                                min_weight = m_nodes[n].Weight;
                                child[i] = n++;
                                break;
                            }
                        }
                        n = std::max(n, i + 1);
                    }
                    for (; n < m_nodes.size(); ++n) {
                        if (m_nodes[n].Valid && m_nodes[n].Weight < min_weight) {
                            min_weight = m_nodes[n].Weight;
                            child[i] = n;
                        }
                    }
                    if (-1 == child[i]) continue;
                    m_nodes[child[i]].Valid = false;
                    weight += m_nodes[child[i]].Weight;
                }
                m_nodes.push_back({ true, true, weight, child[0], child[1] });
                if (weight >= root_weight) break;
            }
        }

        int DecodeToken(MsbBitStream& input) {
            if (m_nodes.empty()) return -1;
            int idx = (int)m_nodes.size() - 1;
            do {
                int bit = input.GetNextBit();
                if (bit == -1) return -1;
                idx = (bit == 0) ? m_nodes[idx].LeftChildIndex : m_nodes[idx].RightChildIndex;
            } while (m_nodes[idx].IsParent);
            return idx;
        }
    };

    struct CbgMetaData {
        int Width, Height, BPP, EncLength;
        uint32_t Key;
        uint8_t CheckSum, CheckXor;
        int Version;
    };

    class CbgDecoder {
        std::vector<uint8_t> m_input;
        CbgMetaData m_info;
        uint32_t m_key;
        float m_DCT[2][64];

        static const float DCT_Table[64];
        static const uint8_t block_fill_order[64];

        int ReadInteger(const uint8_t*& ptr, const uint8_t* end) {
            int v = 0, code_length = 0;
            uint8_t code;
            do {
                if (ptr >= end) return -1;
                code = *ptr++;
                if (code_length >= 32) return -1;
                v |= (code & 0x7f) << code_length;
                code_length += 7;
            } while (code & 0x80);
            return v;
        }

        std::vector<uint32_t> ReadWeightTable(const uint8_t*& ptr, const uint8_t* end, int length) {
            std::vector<uint32_t> w(length);
            for (int i = 0; i < length; ++i) w[i] = ReadInteger(ptr, end);
            return w;
        }

        void DecodeDCT(int channel, const std::vector<int16_t>& data, int src_offset, float tmp[8][8], short ycbcr[64][3]) {
            int d = (channel > 0) ? 1 : 0;
            for (int i = 0; i < 8; ++i) {
                float v1 = data[src_offset + i] * m_DCT[d][i];
                float v2 = data[src_offset + 8 + i] * m_DCT[d][8 + i];
                float v3 = data[src_offset + 16 + i] * m_DCT[d][16 + i];
                float v4 = data[src_offset + 24 + i] * m_DCT[d][24 + i];
                float v5 = data[src_offset + 32 + i] * m_DCT[d][32 + i];
                float v6 = data[src_offset + 40 + i] * m_DCT[d][40 + i];
                float v7 = data[src_offset + 48 + i] * m_DCT[d][48 + i];
                float v8 = data[src_offset + 56 + i] * m_DCT[d][56 + i];

                float v10 = v1 + v5;
                float v11 = v1 - v5;
                float v12 = v3 + v7;
                float v13 = (v3 - v7) * 1.414f - v12;
                v1 = v10 + v12; v7 = v10 - v12;
                v3 = v11 + v13; v5 = v11 - v13;
                float v14 = v2 + v8;
                float v15 = v2 - v8;
                float v16 = v6 + v4;
                float v17 = v6 - v4;
                v8 = v14 + v16;
                v11 = (v14 - v16) * 1.414f;
                float v9 = (v17 + v15) * 1.847f;
                v10 = 1.082f * v15 - v9;
                v13 = -2.613f * v17 + v9;
                v6 = v13 - v8; v4 = v11 - v6; v2 = v10 + v4;

                tmp[0][i] = v1 + v8; tmp[1][i] = v3 + v6;
                tmp[2][i] = v5 + v4; tmp[3][i] = v7 - v2;
                tmp[4][i] = v7 + v2; tmp[5][i] = v5 - v4;
                tmp[6][i] = v3 - v6; tmp[7][i] = v1 - v8;
            }

            int dst_idx = 0;
            for (int i = 0; i < 8; ++i) {
                float v10 = tmp[i][0] + tmp[i][4], v11 = tmp[i][0] - tmp[i][4];
                float v12 = tmp[i][2] + tmp[i][6], v13 = tmp[i][2] - tmp[i][6];
                float v14 = tmp[i][1] + tmp[i][7], v15 = tmp[i][1] - tmp[i][7];
                float v16 = tmp[i][5] + tmp[i][3], v17 = tmp[i][5] - tmp[i][3];

                v13 = 1.414f * v13 - v12;
                float v1 = v10 + v12, v7 = v10 - v12, v3 = v11 + v13, v5 = v11 - v13;
                float v8 = v14 + v16; v11 = (v14 - v16) * 1.414f;
                float v9 = (v17 + v15) * 1.847f;
                v10 = v9 - v15 * 1.082f; v13 = v9 - v17 * 2.613f;
                float v6 = v13 - v8, v4 = v11 - v6, v2 = v10 - v4;

                auto FtoS = [](float f) -> short {
                    int a = 0x80 + ((int)f >> 3);
                    if (a <= 0) return 0; if (a < 0x180 && a > 0xFF) return 0xFF; return (a <= 0xFF) ? a : 0;
                };

                ycbcr[dst_idx++][channel] = FtoS(v1 + v8); ycbcr[dst_idx++][channel] = FtoS(v3 + v6);
                ycbcr[dst_idx++][channel] = FtoS(v5 + v4); ycbcr[dst_idx++][channel] = FtoS(v7 + v2);
                ycbcr[dst_idx++][channel] = FtoS(v7 - v2); ycbcr[dst_idx++][channel] = FtoS(v5 - v4);
                ycbcr[dst_idx++][channel] = FtoS(v3 - v6); ycbcr[dst_idx++][channel] = FtoS(v1 - v8);
            }
        }

    public:
        CbgDecoder(const std::vector<uint8_t>& fileData) : m_input(fileData) {}

        bool ParseHeader() {
            if (m_input.size() < 0x30 || memcmp(m_input.data(), "CompressedBG___", 15) != 0) return false;
            const uint8_t* h = m_input.data();
            m_info.Width = h[0x10] | (h[0x11] << 8);
            m_info.Height = h[0x12] | (h[0x13] << 8);
            m_info.BPP = h[0x14] | (h[0x15] << 8);
            m_info.Key = h[0x24] | (h[0x25] << 8) | (h[0x26] << 16) | (h[0x27] << 24);
            m_info.EncLength = h[0x28] | (h[0x29] << 8) | (h[0x2A] << 16) | (h[0x2B] << 24);
            m_info.CheckSum = h[0x2C];
            m_info.CheckXor = h[0x2D];
            m_info.Version = h[0x2E] | (h[0x2F] << 8);
            m_key = m_info.Key;
            return true;
        }

        std::vector<uint8_t> Unpack() {
            if (m_info.Version < 2 || m_input.size() < 0x30 + m_info.EncLength) return {};

            std::vector<uint8_t> encData(m_input.begin() + 0x30, m_input.begin() + 0x30 + m_info.EncLength);
            uint8_t sum = 0, xor_v = 0;

            for (size_t i = 0; i < encData.size(); ++i) {
                m_key = m_key * 0x015A4E35 + 1;
                encData[i] -= (uint8_t)(m_key >> 16);
                sum += encData[i];
                xor_v ^= encData[i];
            }

            int limit = std::min((int)encData.size(), 128);
            for (int i = 0; i < limit; ++i) m_DCT[i >> 6][i & 0x3F] = encData[i] * DCT_Table[i & 0x3F];

            const uint8_t* base_offset_ptr = m_input.data() + 0x30 + m_info.EncLength;
            const uint8_t* end = m_input.data() + m_input.size();
            const uint8_t* ptr = base_offset_ptr;

            HuffmanTree tree1(ReadWeightTable(ptr, end, 0x10), true);
            HuffmanTree tree2(ReadWeightTable(ptr, end, 0xB0), true);

            int alignedWidth = (m_info.Width + 7) & -8;
            int alignedHeight = (m_info.Height + 7) & -8;
            int y_blocks = alignedHeight / 8;

            std::vector<int> offsets(y_blocks + 1);
            for (int i = 0; i <= y_blocks; ++i) {
                if (ptr + 4 > end) return {};
                offsets[i] = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
                ptr += 4;
            }

            std::vector<uint8_t> output(alignedWidth * alignedHeight * 4, 0x00);
            float tmp[8][8];
            short ycbcr[64][3];
            int dst_offset = 0;

            for (int i = 0; i < y_blocks; ++i) {
                int pad = ((alignedWidth >> 3) + 7) >> 3;
                const uint8_t* bptr = base_offset_ptr + offsets[i] + pad;
                const uint8_t* bend = base_offset_ptr + offsets[i + 1];

                if (bptr >= bend || bend > end) continue;

                int blockSize = ReadInteger(bptr, bend);
                if (blockSize <= 0 || blockSize > alignedWidth * 100) continue;

                MsbBitStream blockBits(bptr, bend - bptr);
                std::vector<int16_t> color_data(blockSize, 0);
                int acc = 0;

                for (int k = 0; k < blockSize; k += 64) {
                    int count = tree1.DecodeToken(blockBits);
                    if (count < 0) break;
                    if (count != 0) {
                        int v = blockBits.GetBits(count);
                        if (count > 0 && ((v >> (count - 1)) == 0)) {
                            uint32_t mask = 0xFFFFFFFF << count;
                            v = (int)(mask | (uint32_t)v) + 1;
                        }
                        acc += v;
                    }
                    color_data[k] = acc;
                }

                if ((blockBits.GetCacheSize() & 7) != 0) blockBits.GetBits(blockBits.GetCacheSize() & 7);

                for (int k = 0; k < blockSize; k += 64) {
                    int index = 1;
                    while (index < 64) {
                        int code = tree2.DecodeToken(blockBits);
                        if (code <= 0) break;
                        if (code == 0xF) { index += 16; continue; }
                        index += (code & 0xF);
                        if (index >= 64) break;

                        int bits = code >> 4;
                        int v = blockBits.GetBits(bits);
                        if (bits != 0 && ((v >> (bits - 1)) == 0)) {
                            uint32_t mask = 0xFFFFFFFF << bits;
                            v = (int)(mask | (uint32_t)v) + 1;
                        }

                        if (k + block_fill_order[index] < blockSize) {
                            color_data[k + block_fill_order[index]] = v;
                        }
                        index++;
                    }
                }

                int blocks_per_row = alignedWidth / 8;
                for (int b = 0; b < blocks_per_row; ++b) {
                    int current_dst = dst_offset + b * 32;
                    int base_src = b * 64;

                    if (base_src + alignedWidth * 16 + 64 > blockSize) break;

                    DecodeDCT(0, color_data, base_src, tmp, ycbcr);
                    DecodeDCT(1, color_data, base_src + alignedWidth * 8, tmp, ycbcr);
                    DecodeDCT(2, color_data, base_src + alignedWidth * 16, tmp, ycbcr);

                    for (int j = 0; j < 64; ++j) {
                        float cy = ycbcr[j][0], cb = ycbcr[j][1], cr = ycbcr[j][2];
                        float r = cy + 1.402f * cr - 178.956f;
                        float g = cy - 0.34414f * cb - 0.71414f * cr + 135.95984f;
                        float b_val = cy + 1.772f * cb - 226.316f;

                        auto Clamp = [](float f) -> uint8_t { return f < 0 ? 0 : (f > 255 ? 255 : (uint8_t)f); };
                        int p = ((j >> 3) * alignedWidth + (j & 7)) * 4;

                        output[current_dst + p] = Clamp(b_val);
                        output[current_dst + p + 1] = Clamp(g);
                        output[current_dst + p + 2] = Clamp(r);
                        output[current_dst + p + 3] = 0xFF;
                    }
                }
                dst_offset += alignedWidth * 32;
            }

            if (m_info.BPP == 32 && offsets[y_blocks] > 0) {
                const uint8_t* aptr = base_offset_ptr + offsets[y_blocks];
                if (aptr + 4 <= end) {
                    int magic = aptr[0] | (aptr[1] << 8) | (aptr[2] << 16) | (aptr[3] << 24);
                    if (magic == 1) {
                        aptr += 4;
                        int dst = 3, ctl = 1 << 1;
                        while (dst < output.size() && aptr < end) {
                            ctl >>= 1;
                            if (ctl == 1) ctl = (*aptr++) | 0x100;

                            if (ctl & 1) {
                                if (aptr + 2 > end) break;
                                uint16_t v = aptr[0] | (aptr[1] << 8); aptr += 2;
                                int x = v & 0x3F; if (x > 0x1F) x |= ~0x3F;
                                int y = (v >> 6) & 7; if (y != 0) y |= ~0x07;
                                int count = ((v >> 9) & 0x7F) + 3;

                                int src = dst + (x + y * alignedWidth) * 4;
                                if (src < 0 || src >= dst) break;
                                for (int i = 0; i < count && dst < output.size(); ++i) {
                                    output[dst] = output[src];
                                    src += 4; dst += 4;
                                }
                            }
                            else {
                                if (dst < output.size()) {
                                    output[dst] = *aptr++;
                                    dst += 4;
                                }
                            }
                        }
                    }
                }
            }

            std::vector<uint8_t> final_output(m_info.Width * m_info.Height * 4);
            for (int y = 0; y < m_info.Height; ++y) {
                memcpy(final_output.data() + y * m_info.Width * 4,
                       output.data() + y * alignedWidth * 4,
                       m_info.Width * 4);
            }

            return final_output;
        }

        int GetWidth() { return m_info.Width; }
        int GetHeight() { return m_info.Height; }
    };

    const float CbgDecoder::DCT_Table[64] = {
            1.00000000f, 1.38703990f, 1.30656302f, 1.17587554f, 1.00000000f, 0.78569496f, 0.54119611f, 0.27589938f,
            1.38703990f, 1.92387950f, 1.81225491f, 1.63098633f, 1.38703990f, 1.08979023f, 0.75066054f, 0.38268343f,
            1.30656302f, 1.81225491f, 1.70710683f, 1.53635550f, 1.30656302f, 1.02655995f, 0.70710677f, 0.36047992f,
            1.17587554f, 1.63098633f, 1.53635550f, 1.38268340f, 1.17587554f, 0.92387950f, 0.63637930f, 0.32442334f,
            1.00000000f, 1.38703990f, 1.30656302f, 1.17587554f, 1.00000000f, 0.78569496f, 0.54119611f, 0.27589938f,
            0.78569496f, 1.08979023f, 1.02655995f, 0.92387950f, 0.78569496f, 0.61731654f, 0.42521504f, 0.21677275f,
            0.54119611f, 0.75066054f, 0.70710677f, 0.63637930f, 0.54119611f, 0.42521504f, 0.29289323f, 0.14931567f,
            0.27589938f, 0.38268343f, 0.36047992f, 0.32442334f, 0.27589938f, 0.21677275f, 0.14931567f, 0.07612047f,
    };

    const uint8_t CbgDecoder::block_fill_order[64] = {
            0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
            12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
            35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
            58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
    };
} // Akhir namespace

bool BgiImageDecoder::decode(const std::vector<char>& input, std::vector<uint32_t>& outputPixels, uint32_t& width, uint32_t& height) {
    if (input.size() < 0x20) return false;
    const uint8_t* data = (const uint8_t*)input.data();

    if (memcmp(data, "CompressedBG___", 15) == 0) {
        std::vector<uint8_t> unsigned_input(input.begin(), input.end());
        CbgDecoder decoder(unsigned_input);

        if (!decoder.ParseHeader()) return false;

        auto pixels = decoder.Unpack();
        if (pixels.empty()) return false;

        width = decoder.GetWidth();
        height = decoder.GetHeight();

        outputPixels.resize(width * height);

        for (size_t i = 0; i < width * height; ++i) {
            uint8_t b = pixels[i * 4 + 0];
            uint8_t g = pixels[i * 4 + 1];
            uint8_t r = pixels[i * 4 + 2];
            uint8_t a = pixels[i * 4 + 3];
            outputPixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
        return true;
    }

    else if (memcmp(data, "CompressedBG", 12) == 0) {
        uint32_t headerSize = readU32(data + 0x0C);
        if (headerSize < 0x20) headerSize = 0x20;

        width = readU32(data + 0x10);
        height = readU32(data + 0x14);
        int bpp = readU32(data + 0x18);
        int isScrambled = readU32(data + 0x1C);

        if (width == 0 || height == 0) return false;
        if (bpp != 24 && bpp != 32) return false;

        int bytesPerPixel = bpp / 8;
        if (input.size() < headerSize) return false;

        outputPixels.resize(width * height);

        const uint8_t* src = data + headerSize;
        const uint8_t* srcEnd = data + input.size();

        if (isScrambled != 0) {
            std::vector<uint8_t> rawImage(width * height * bytesPerPixel);
            uint8_t* dstBase = rawImage.data();

            for (int channel = 0; channel < bytesPerPixel; channel++) {
                uint8_t accumulator = 0;
                for (uint32_t y = 0; y < height; y++) {
                    bool forward = (y % 2 == 0);

                    if (forward) {
                        for (uint32_t x = 0; x < width; x++) {
                            if (src >= srcEnd) break;
                            accumulator += *src++;
                            dstBase[(y * width + x) * bytesPerPixel + channel] = accumulator;
                        }
                    } else {
                        for (int x = (int)width - 1; x >= 0; x--) {
                            if (src >= srcEnd) break;
                            accumulator += *src++;
                            dstBase[(y * width + x) * bytesPerPixel + channel] = accumulator;
                        }
                    }
                }
            }

            for (size_t i = 0; i < outputPixels.size(); i++) {
                uint8_t b = rawImage[i * bytesPerPixel + 0];
                uint8_t g = rawImage[i * bytesPerPixel + 1];
                uint8_t r = rawImage[i * bytesPerPixel + 2];
                uint8_t a = (bytesPerPixel == 4) ? rawImage[i * bytesPerPixel + 3] : 0xFF;
                outputPixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        } else {
            for (size_t i = 0; i < outputPixels.size(); i++) {
                if (src + bytesPerPixel > srcEnd) break;
                uint8_t b = *src++;
                uint8_t g = *src++;
                uint8_t r = *src++;
                uint8_t a = (bytesPerPixel == 4) ? *src++ : 0xFF;
                outputPixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
        return true;
    }

    return false;
}