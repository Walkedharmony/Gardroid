#include "Xp3Packer.h"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <zlib.h>
#include <codecvt>
#include <locale>
#include <algorithm>

namespace fs = std::filesystem;


void write_u8(std::vector<char>& buf, uint8_t v) { buf.push_back(static_cast<char>(v)); }
void write_u32(std::vector<char>& buf, uint32_t v) {
    buf.push_back((char)(v & 0xFF));
    buf.push_back((char)((v >> 8) & 0xFF));
    buf.push_back((char)((v >> 16) & 0xFF));
    buf.push_back((char)((v >> 24) & 0xFF));
}
void write_u64(std::vector<char>& buf, uint64_t v) {
    write_u32(buf, (uint32_t)(v & 0xFFFFFFFF));
    write_u32(buf, (uint32_t)(v >> 32));
}
void write_bytes(std::vector<char>& buf, const void* data, size_t size) {
    const char* ptr = static_cast<const char*>(data);
    buf.insert(buf.end(), ptr, ptr + size);
}


bool Xp3Packer::compress_zlib(const std::vector<char>& input, std::vector<char>& output) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    // Level 9 = Best Compression
    if (deflateInit(&strm, 9) != Z_OK) return false;


    size_t max_out = deflateBound(&strm, input.size());
    output.resize(max_out);

    strm.avail_in = input.size();
    strm.next_in = (Bytef*)input.data();
    strm.avail_out = output.size();
    strm.next_out = (Bytef*)output.data();

    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    if (ret != Z_STREAM_END) return false;


    output.resize(strm.total_out);
    return true;
}

std::vector<char16_t> Xp3Packer::to_utf16(const std::string& str) {

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    try {
        std::u16string u16 = converter.from_bytes(str);
        return std::vector<char16_t>(u16.begin(), u16.end());
    } catch (...) {
        return {};
    }
}

bool Xp3Packer::pack(const std::string& sourceFolder, const std::string& outputFile,
                     std::function<void(const std::string&, int, int)> onProgress) {

    FILE* fp = fopen(outputFile.c_str(), "wb");
    if (!fp) return false;


    const uint8_t HEADER_MAGIC[] = { 0x58, 0x50, 0x33, 0x0D, 0x0A, 0x20, 0x0A, 0x1A, 0x8B, 0x67, 0x01 };
    fwrite(HEADER_MAGIC, 1, sizeof(HEADER_MAGIC), fp);


    uint64_t index_offset_placeholder = 0;
    fwrite(&index_offset_placeholder, 1, 8, fp);


    std::vector<char> index_buffer;

    write_u8(index_buffer, 0);
    write_u64(index_buffer, 0);


    std::string baseDir = sourceFolder;
    if (baseDir.back() == '/') baseDir.pop_back();
    int totalFiles = 0;
    for (const auto& entry : fs::recursive_directory_iterator(baseDir)) {
        if (entry.is_regular_file()) totalFiles++;
    }
    int currentFileIndex = 0;
    for (const auto& entry : fs::recursive_directory_iterator(baseDir)) {
        if (!entry.is_regular_file()) continue;

        currentFileIndex++;
        std::string filePath = entry.path().string();
        std::string relativePath = filePath.substr(baseDir.length());
        if (!relativePath.empty() && (relativePath[0] == '/' || relativePath[0] == '\\')) {
            relativePath = relativePath.substr(1);
        }

        if (onProgress) {
            onProgress(relativePath, currentFileIndex, totalFiles);
        }

        std::ifstream inFile(filePath, std::ios::binary | std::ios::ate);
        if (!inFile) continue;
        uint64_t originalSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);
        std::vector<char> fileData(originalSize);
        inFile.read(fileData.data(), originalSize);

        bool compressed = true;
        if (filePath.length() > 4 && filePath.substr(filePath.length() - 4) == ".mpg") compressed = false;

        std::vector<char> dataToWrite;
        if (compressed) {
            if (!compress_zlib(fileData, dataToWrite)) {

                compressed = false;
                dataToWrite = fileData;
            }
        } else {
            dataToWrite = fileData;
        }

        uint64_t compressedSize = dataToWrite.size();

        uint64_t currentOffset = ftell(fp);
        fwrite(dataToWrite.data(), 1, compressedSize, fp);

        // CHUNK: File (0x656C6946)
        std::vector<char> fileChunk;

        // -- SUBCHUNK: Info (0x6F666E69)
        std::vector<char> infoChunk;
        write_u32(infoChunk, 0); // Flags
        write_u64(infoChunk, originalSize);
        write_u64(infoChunk, compressedSize);

        auto nameUtf16 = to_utf16(relativePath);
        write_u32(infoChunk, (uint32_t)nameUtf16.size());

        infoChunk.pop_back(); infoChunk.pop_back();

        // Write Name (UTF-16LE Bytes)
        write_bytes(infoChunk, nameUtf16.data(), nameUtf16.size() * 2);


        write_u32(fileChunk, 0x6F666E69); // "info"
        write_u64(fileChunk, infoChunk.size());
        write_bytes(fileChunk, infoChunk.data(), infoChunk.size());


        std::vector<char> segmChunk;
        write_u32(segmChunk, compressed ? 1 : 0); // Flag (1=compressed)
        write_u64(segmChunk, currentOffset);      // Offset
        write_u64(segmChunk, originalSize);
        write_u64(segmChunk, compressedSize);


        write_u32(fileChunk, 0x6D676573); // "segm"
        write_u64(fileChunk, segmChunk.size());
        write_bytes(fileChunk, segmChunk.data(), segmChunk.size());


        std::vector<char> adlrChunk;
        write_u32(adlrChunk, 0);

        write_u32(fileChunk, 0x726C6461); // "adlr"
        write_u64(fileChunk, adlrChunk.size());
        write_bytes(fileChunk, adlrChunk.data(), adlrChunk.size());


        write_u32(index_buffer, 0x656C6946); // "File"
        write_u64(index_buffer, fileChunk.size());
        write_bytes(index_buffer, fileChunk.data(), fileChunk.size());
    }

    uint64_t totalIndexSize = index_buffer.size() - 9;
    memcpy(index_buffer.data() + 1, &totalIndexSize, 8);


    uint64_t indexOffsetPosition = ftell(fp);
    fwrite(index_buffer.data(), 1, index_buffer.size(), fp);


    fseek(fp, 11, SEEK_SET);
    fwrite(&indexOffsetPosition, 1, 8, fp);

    fclose(fp);
    return true;
}