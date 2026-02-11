#ifndef SHA1_HPP
#define SHA1_HPP

#include <vector>
#include <string>
#include <cstdint>

class SHA1_Simple {
public:

    static std::vector<uint8_t> Calculate(const std::vector<uint8_t>& data) {
        uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

        uint64_t mlen = data.size();
        uint64_t bitlen = mlen * 8;


        std::vector<uint8_t> padded = data;

        padded.push_back(0x80);

        while ((padded.size() * 8) % 512 != 448) {
            padded.push_back(0);
        }


        for (int i = 7; i >= 0; --i) {
            padded.push_back((bitlen >> (i * 8)) & 0xFF);
        }


        for (size_t i = 0; i < padded.size(); i += 64) {
            uint32_t w[80];


            for (int j = 0; j < 16; ++j) {
                w[j] = (padded[i + j * 4] << 24) |
                       (padded[i + j * 4 + 1] << 16) |
                       (padded[i + j * 4 + 2] << 8) |
                       (padded[i + j * 4 + 3]);
            }


            for (int j = 16; j < 80; ++j) {
                w[j] = rotl(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
            }


            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

            for (int j = 0; j < 80; ++j) {
                uint32_t f, k;
                if (j < 20) {
                    f = (b & c) | (~b & d);
                    k = 0x5A827999;
                } else if (j < 40) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1;
                } else if (j < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDC;
                } else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6;
                }

                uint32_t temp = rotl(a, 5) + f + e + k + w[j];
                e = d;
                d = c;
                c = rotl(b, 30);
                b = a;
                a = temp;
            }


            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
        }

        std::vector<uint8_t> digest(20);
        for (int i = 0; i < 5; ++i) {
            digest[i * 4]     = (h[i] >> 24) & 0xFF;
            digest[i * 4 + 1] = (h[i] >> 16) & 0xFF;
            digest[i * 4 + 2] = (h[i] >> 8) & 0xFF;
            digest[i * 4 + 3] = h[i] & 0xFF;
        }

        return digest;
    }

private:
    static uint32_t rotl(uint32_t v, uint32_t bits) {
        return (v << bits) | (v >> (32 - bits));
    }
};

#endif