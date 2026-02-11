#include "TlgDecoder.h"
#include <cstring>
#include <algorithm>

int TlgDecoder::decompressLZSS(std::vector<uint8>& outbuf, const std::vector<uint8>& inbuf, int inbuf_size, std::vector<uint8>& text, int initialr)
{
    int r = initialr;
    uint32 flags = 0;
    int o = 0;
    int i = 0;
    while (i < inbuf_size) {
        if (((flags >>= 1) & 256) == 0) {
            if (i >= inbuf_size) break;
            flags = (uint32)(inbuf[i++] | 0xff00);
        }
        if (flags & 1) {
            if (i + 1 >= inbuf_size) break;
            int mpos = inbuf[i] | ((inbuf[i + 1] & 0xf) << 8);
            int mlen = (inbuf[i + 1] & 0xf0) >> 4;
            i += 2;
            mlen += 3;
            if (mlen == 18) {
                if (i >= inbuf_size) break;
                mlen += inbuf[i++];
            }
            while (mlen--) {
                if (o >= outbuf.size()) break;
                uint8 c = text[mpos++];
                outbuf[o++] = c;
                text[r++] = c;
                mpos &= (4096 - 1);
                r &= (4096 - 1);
            }
        }
        else {
            if (i >= inbuf_size) break;
            uint8 c = inbuf[i++];
            if (o < outbuf.size()) outbuf[o++] = c;
            text[r++] = c;
            r &= (4096 - 1);
        }
    }
    return r;
}

void TlgDecoder::composeColors3To4(std::vector<uint8>& outp, int outp_index, int upper,
                                   const std::vector<std::vector<uint8>>& buf, int bufpos, int width)
{
    uint8 pc0 = 0, pc1 = 0, pc2 = 0;
    for (int x = 0; x < width; x++) {
        uint8 c0 = buf[0][bufpos + x];
        uint8 c1 = buf[1][bufpos + x];
        uint8 c2 = buf[2][bufpos + x];
        c0 += c1; c2 += c1;

        // Output B, G, R, A (Little Endian int: 0xAARRGGBB)
        outp[outp_index++] = (uint8)(((pc0 += c0) + outp[upper + 0]) & 0xff); // B
        outp[outp_index++] = (uint8)(((pc1 += c1) + outp[upper + 1]) & 0xff); // G
        outp[outp_index++] = (uint8)(((pc2 += c2) + outp[upper + 2]) & 0xff); // R
        outp[outp_index++] = 0xff; // A
        upper += 4;
    }
}

void TlgDecoder::composeColors4To4(std::vector<uint8>& outp, int outp_index, int upper,
                                   const std::vector<std::vector<uint8>>& buf, int bufpos, int width)
{
    uint8 pc0 = 0, pc1 = 0, pc2 = 0, pc3 = 0;
    for (int x = 0; x < width; x++) {
        uint8 c0 = buf[0][bufpos + x];
        uint8 c1 = buf[1][bufpos + x];
        uint8 c2 = buf[2][bufpos + x];
        uint8 c3 = buf[3][bufpos + x];
        c0 += c1; c2 += c1;

        outp[outp_index++] = (uint8)(((pc0 += c0) + outp[upper + 0]) & 0xff); // B
        outp[outp_index++] = (uint8)(((pc1 += c1) + outp[upper + 1]) & 0xff); // G
        outp[outp_index++] = (uint8)(((pc2 += c2) + outp[upper + 2]) & 0xff); // R
        outp[outp_index++] = (uint8)(((pc3 += c3) + outp[upper + 3]) & 0xff); // A
        upper += 4;
    }
}



bool TlgDecoder::decode(const std::vector<char>& input, std::vector<uint32_t>& outputPixels, uint32_t& width, uint32_t& height) {
    if (input.size() < 15) return false;

    const uint8_t* src = (const uint8_t*)input.data();
    const uint8_t* srcEnd = src + input.size();

    int dataOffset = 0;
    if (memcmp(src, "TLG0.0", 6) == 0) {
        dataOffset = 15;
    }

    if (dataOffset + 11 > input.size()) return false;

    const uint8_t* cursor = src + dataOffset;

    if (memcmp(cursor, "TLG5.0", 6) != 0) {
        return false;
    }

    cursor += 11;

    int colors = *cursor++;
    width = readInt32(cursor);
    height = readInt32(cursor);
    int32 blockHeight = readInt32(cursor);

    if (colors != 3 && colors != 4) return false;

    int blockCount = ((height - 1) / blockHeight) + 1;

    cursor += blockCount * 4;

    int stride = width * 4;

    std::vector<uint8> imageBitsBytes(height * stride, 0);


    std::vector<uint8> text(4096, 0);
    std::vector<uint8> inBuf(blockHeight * width + 100);
    std::vector<std::vector<uint8>> outBuf(4);
    for (int i = 0; i < 4; i++) outBuf[i].resize(blockHeight * width + 100);

    int z = 0;
    int prevLine = -1;


    for (int y_blk = 0; y_blk < (int)height; y_blk += blockHeight)
    {

        for (int c = 0; c < colors; c++)
        {
            uint8 mark = *cursor++;
            int32 size = readInt32(cursor);

            if (mark == 0) {
                if (cursor + size > srcEnd) return false;
                memcpy(inBuf.data(), cursor, size);
                cursor += size;

                z = decompressLZSS(outBuf[c], inBuf, size, text, z);
            }
            else {
                if (cursor + size > srcEnd) return false;
                memcpy(outBuf[c].data(), cursor, size);
                cursor += size;
            }
        }


        int y_lim = y_blk + blockHeight;
        if (y_lim > (int)height) y_lim = height;

        int outbuf_pos = 0;

        for (int y = y_blk; y < y_lim; y++)
        {
            int current = y * stride;
            int current_org = current;

            if (prevLine >= 0) {
                if (colors == 3)
                    composeColors3To4(imageBitsBytes, current, prevLine, outBuf, outbuf_pos, width);
                else
                    composeColors4To4(imageBitsBytes, current, prevLine, outBuf, outbuf_pos, width);
            }
            else {

                if (colors == 3) {
                    int pr = 0, pg = 0, pb = 0;
                    for (int x = 0; x < (int)width; x++) {
                        int b = outBuf[0][outbuf_pos + x];
                        int g = outBuf[1][outbuf_pos + x];
                        int r = outBuf[2][outbuf_pos + x];
                        b += g; r += g;


                        imageBitsBytes[current++] = (uint8)(pb += b);
                        imageBitsBytes[current++] = (uint8)(pg += g);
                        imageBitsBytes[current++] = (uint8)(pr += r);
                        imageBitsBytes[current++] = 0xff;
                    }
                }
                else {
                    int pr = 0, pg = 0, pb = 0, pa = 0;
                    for (int x = 0; x < (int)width; x++) {
                        int b = outBuf[0][outbuf_pos + x];
                        int g = outBuf[1][outbuf_pos + x];
                        int r = outBuf[2][outbuf_pos + x];
                        int a = outBuf[3][outbuf_pos + x];
                        b += g; r += g;

                        imageBitsBytes[current++] = (uint8)(pb += b);
                        imageBitsBytes[current++] = (uint8)(pg += g);
                        imageBitsBytes[current++] = (uint8)(pr += r);
                        imageBitsBytes[current++] = (uint8)(pa += a);
                    }
                }
            }
            outbuf_pos += width;
            prevLine = current_org;
        }
    }

    outputPixels.resize(width * height);
    memcpy(outputPixels.data(), imageBitsBytes.data(), imageBitsBytes.size());

    return true;
}