#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <functional>

class Xp3Packer {
public:

    static bool pack(const std::string& sourceFolder, const std::string& outputFile,
                     std::function<void(const std::string&, int, int)> onProgress);

private:

    static bool compress_zlib(const std::vector<char>& input, std::vector<char>& output);


    static std::vector<char16_t> to_utf16(const std::string& str);
};