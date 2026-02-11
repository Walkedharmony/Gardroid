#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <functional>
#include <filesystem>

class PfsPacker {
public:

    using ProgressCallback = std::function<void(const std::string&, int, int)>;

    static bool pack(const std::string& sourceFolder, const std::string& outputFile, ProgressCallback onProgress = nullptr);

private:
    struct FileEntry {
        std::string fullPath;
        std::string relativePath;
        std::streampos position;
        uint32_t offset;
        uint32_t length;
    };


    static void encrypt(std::vector<uint8_t>& input, const std::vector<uint8_t>& key);


    static std::string normalizePath(std::string path);
};