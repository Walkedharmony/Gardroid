#pragma once

#include <vector>
#include <cstdint>
#include <string>

class BgiImageDecoder {
public:

    static bool decode(const std::vector<char>& input, std::vector<uint32_t>& outputPixels, uint32_t& width, uint32_t& height);

private:
    static uint32_t readU32(const uint8_t* ptr) {
        return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    }
};