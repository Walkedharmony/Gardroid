#pragma once

#include <vector>
#include <cstdint>

class KirikiriDescrambler {
public:
    static std::vector<uint8_t> Descramble(std::vector<uint8_t>& data);

private:
    static std::vector<uint8_t> DescrambleMode0(std::vector<uint8_t>& data);
    static std::vector<uint8_t> DescrambleMode1(std::vector<uint8_t>& data);
    static std::vector<uint8_t> DecompressMode2(std::vector<uint8_t>& data);
};