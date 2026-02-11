#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstring> // Untuk memcpy

class TlgDecoder {
public:
    static bool decode(const std::vector<char>& input, std::vector<uint32_t>& outputPixels, uint32_t& width, uint32_t& height);

private:
    using uint8 = uint8_t;
    using int32 = int32_t;
    using uint32 = uint32_t;

    static int32 readInt32(const uint8_t*& src) {
        int32 val;
        memcpy(&val, src, 4);
        src += 4;
        return val;
    }

    static int decompressLZSS(std::vector<uint8>& outbuf, const std::vector<uint8>& inbuf, int inbuf_size, std::vector<uint8>& text, int initial_r);

    static void composeColors3To4(std::vector<uint8>& outp, int outp_index, int upper,
                                  const std::vector<std::vector<uint8>>& buf, int bufpos, int width);

    static void composeColors4To4(std::vector<uint8>& outp, int outp_index, int upper,
                                  const std::vector<std::vector<uint8>>& buf, int bufpos, int width);
};