#include "KirikiriKS.h"
#include <stdexcept>
#include <zlib.h>

std::vector<uint8_t> KirikiriDescrambler::Descramble(std::vector<uint8_t>& data) {
    if (data.size() < 5) return data;
    if (data[0] != 0xFE || data[1] != 0xFE) {
        return data;
    }
    if (data[3] != 0xFF || data[4] != 0xFE) {
        throw std::runtime_error("Scrambled Kirikiri file is missing BOM.");
    }

    uint8_t mode = data[2];

    switch (mode) {
        case 0: return DescrambleMode0(data);
        case 1: return DescrambleMode1(data);
        case 2: return DecompressMode2(data);
        default:
            throw std::runtime_error("Unsupported scrambling mode.");
    }
}

std::vector<uint8_t> KirikiriDescrambler::DescrambleMode0(std::vector<uint8_t>& data) {
    for (size_t i = 5; i < data.size(); i += 2) {
        if (i + 1 < data.size()) {
            if (data[i + 1] == 0 && data[i] < 0x20) continue;
            data[i + 1] ^= (data[i] & 0xFE);
            data[i] ^= 1;
        }
    }
    return std::vector<uint8_t>(data.begin() + 3, data.end());
}

std::vector<uint8_t> KirikiriDescrambler::DescrambleMode1(std::vector<uint8_t>& data) {
    for (size_t i = 5; i < data.size(); i += 2) {
        if (i + 1 < data.size()) {
            uint16_t c = data[i] | (data[i + 1] << 8);
            c = ((c & 0xAAAA) >> 1) | ((c & 0x5555) << 1);
            data[i] = (uint8_t)(c & 0xFF);
            data[i + 1] = (uint8_t)(c >> 8);
        }
    }
    return std::vector<uint8_t>(data.begin() + 3, data.end());
}

std::vector<uint8_t> KirikiriDescrambler::DecompressMode2(std::vector<uint8_t>& data) {
    size_t offset = 5;
    auto readInt64 = [&](size_t pos) -> int64_t {
        int64_t val = 0;
        for (int j = 0; j < 8; j++) val |= ((int64_t)data[pos + j] << (j * 8));
        return val;
    };


    int64_t compressedLength = readInt64(offset);
    offset += 8;
    int64_t uncompressedLength = readInt64(offset);
    offset += 8;

    offset += 2;


    std::vector<uint8_t> uncompressedData(2 + uncompressedLength);
    uncompressedData[0] = 0xFF;
    uncompressedData[1] = 0xFE;

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = data.size() - offset;
    strm.next_in = data.data() + offset;
    strm.avail_out = uncompressedLength;
    strm.next_out = uncompressedData.data() + 2;
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib.");
    }

    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END && ret != Z_OK) {
        throw std::runtime_error("Zlib decompression failed.");
    }

    return uncompressedData;
}