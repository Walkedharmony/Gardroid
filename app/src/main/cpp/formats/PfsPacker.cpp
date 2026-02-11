#include "PfsPacker.h"
#include "../utils/SHA-1.h"
#include <iostream>
#include <algorithm>
#include <vector>

namespace fs = std::filesystem;

std::string PfsPacker::normalizePath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    return path;
}

void PfsPacker::encrypt(std::vector<uint8_t>& input, const std::vector<uint8_t>& key) {
    if (key.empty()) return;
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] ^= key[i % key.size()];
    }
}

bool PfsPacker::pack(const std::string& sourceFolder, const std::string& outputFile, ProgressCallback onProgress) {
    std::vector<FileEntry> entries;

    try {
        if (!fs::exists(sourceFolder)) return false;

        for (const auto& entry : fs::recursive_directory_iterator(sourceFolder)) {
            if (entry.is_regular_file()) {
                FileEntry record;
                record.fullPath = entry.path().string();

                std::string rel = fs::relative(entry.path(), sourceFolder).string();

                record.relativePath = normalizePath(rel);

                record.position = 0;
                record.offset = 0;
                record.length = 0;

                entries.push_back(record);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
        return false;
    }

    if (entries.empty()) return false;

    std::ofstream writer(outputFile, std::ios::binary);
    if (!writer) return false;

    uint32_t zero = 0;

    writer.write("pf8", 3);

    writer.write(reinterpret_cast<const char*>(&zero), 4);

    auto posIndexStart = writer.tellp();
    uint32_t count = static_cast<uint32_t>(entries.size());
    writer.write(reinterpret_cast<const char*>(&count), 4);

    for (auto& entry : entries) {

        uint32_t pathLen = static_cast<uint32_t>(entry.relativePath.length());
        writer.write(reinterpret_cast<const char*>(&pathLen), 4);

        writer.write(entry.relativePath.c_str(), pathLen);

        entry.position = writer.tellp();

        writer.write(reinterpret_cast<const char*>(&zero), 4); // Unknown
        writer.write(reinterpret_cast<const char*>(&zero), 4); // Offset
        writer.write(reinterpret_cast<const char*>(&zero), 4); // Length
    }

    auto posOffsetTable = writer.tellp();

    uint32_t tableCount = count + 1;
    writer.write(reinterpret_cast<const char*>(&tableCount), 4);

    for (const auto& entry : entries) {
        uint32_t relPos = static_cast<uint32_t>(entry.position - posIndexStart);
        writer.write(reinterpret_cast<const char*>(&relPos), 4); // Position
        writer.write(reinterpret_cast<const char*>(&zero), 4);   // Padding
    }

    writer.write(reinterpret_cast<const char*>(&zero), 4); // End marker
    writer.write(reinterpret_cast<const char*>(&zero), 4); // Padding

    uint32_t relTablePos = static_cast<uint32_t>(posOffsetTable - posIndexStart);
    writer.write(reinterpret_cast<const char*>(&relTablePos), 4);

    auto posIndexEnd = writer.tellp();

    uint32_t currentDataOffset = static_cast<uint32_t>(posIndexEnd);

    for (auto& entry : entries) {
        uintmax_t fSize = fs::file_size(entry.fullPath);

        entry.offset = currentDataOffset;
        entry.length = static_cast<uint32_t>(fSize);

        currentDataOffset += entry.length;
    }

    for (const auto& entry : entries) {

        writer.seekp(entry.position + std::streamoff(4));

        writer.write(reinterpret_cast<const char*>(&entry.offset), 4);
        writer.write(reinterpret_cast<const char*>(&entry.length), 4);
    }

    writer.flush();

    size_t indexSize = static_cast<size_t>(posIndexEnd - posIndexStart);
    std::vector<uint8_t> indexData(indexSize);

    std::ifstream reader(outputFile, std::ios::binary);
    reader.seekg(posIndexStart);
    reader.read(reinterpret_cast<char*>(indexData.data()), indexSize);
    reader.close();

    std::vector<uint8_t> key = SHA1_Simple::Calculate(indexData);

    writer.seekp(posIndexEnd);

    int processedCount = 0;
    int totalFiles = entries.size();

    for (const auto& entry : entries) {
        if (onProgress) onProgress(entry.relativePath, ++processedCount, totalFiles);

        std::ifstream fileIn(entry.fullPath, std::ios::binary);
        if (!fileIn) continue;

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(fileIn)),
                                    std::istreambuf_iterator<char>());

        encrypt(buffer, key);

        writer.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }



    writer.seekp(3);
    uint32_t finalIndexSize = static_cast<uint32_t>(indexSize);
    writer.write(reinterpret_cast<const char*>(&finalIndexSize), 4);

    writer.close();
    return true;
}