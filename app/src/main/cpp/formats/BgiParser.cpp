#include "BgiParser.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <vector>

BgiParser::BgiParser(FILE* file) : m_file(file) {}

BgiParser::~BgiParser() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

bool BgiParser::parse() {
    if (!m_file) return false;

    char signature[13];
    fseek(m_file, 0, SEEK_SET);
    if (fread(signature, 1, 12, m_file) != 12) return false;
    signature[12] = '\0';

    int version = 0;

    if (memcmp(signature, "PackFile    ", 12) == 0) {
        version = 1;
    }
    else if (memcmp(signature, "BURIKO ARC20", 12) == 0) {
        version = 2;
    }
    else {
        return false;
    }


    uint32_t count = readU32();


    if (count == 0 || count > 200000) return false;


    if (version == 1) {

        uint32_t index_size = 0x20 * count;
        uint32_t base_offset = 0x10 + index_size;

        fseek(m_file, 0x10, SEEK_SET); // Pindah ke awal index

        for (uint32_t i = 0; i < count; ++i) {
            char nameBuf[17];
            if (fread(nameBuf, 1, 16, m_file) != 16) break;
            nameBuf[16] = '\0';

            uint32_t relOffset = readU32();
            uint32_t size = readU32();
            fseek(m_file, 8, SEEK_CUR); // Skip padding

            BgiEntry entry;
            entry.name = std::string(nameBuf);
            entry.offset = base_offset + relOffset;
            entry.size = size;
            m_file_list.push_back(entry);
        }
    }
    else if (version == 2) {

        uint32_t index_size = 0x80 * count;

        uint32_t base_offset = 0x10 + index_size;

        fseek(m_file, 0x10, SEEK_SET);

        std::vector<char> nameBuffer(96);

        for (uint32_t i = 0; i < count; ++i) {

            if (fread(nameBuffer.data(), 1, 0x60, m_file) != 0x60) break;


            nameBuffer[95] = '\0';
            std::string fileName = std::string(nameBuffer.data());


            uint32_t relOffset = readU32();


            uint32_t size = readU32();

            fseek(m_file, 24, SEEK_CUR);

            BgiEntry entry;
            entry.name = fileName;
            entry.offset = base_offset + relOffset;
            entry.size = size;
            m_file_list.push_back(entry);
        }
    }

    return true;
}


bool BgiParser::extractFile(const std::string& targetName, const std::string& outputPath) {
    const BgiEntry* targetEntry = nullptr;

    for (const auto& entry : m_file_list) {
        if (entry.name == targetName) {
            targetEntry = &entry;
            break;
        }
    }

    if (!targetEntry) return false;

    fseek(m_file, targetEntry->offset, SEEK_SET);
    std::vector<char> buffer(targetEntry->size);
    if (fread(buffer.data(), 1, targetEntry->size, m_file) != targetEntry->size) return false;

    FILE* out = fopen(outputPath.c_str(), "wb");
    if (!out) return false;
    fwrite(buffer.data(), 1, buffer.size(), out);
    fclose(out);

    return true;
}

bool BgiParser::extractToBuffer(const std::string& targetName, std::vector<char>& outputBuffer) {
    const BgiEntry* targetEntry = nullptr;

    for (const auto& entry : m_file_list) {
        if (entry.name == targetName) {
            targetEntry = &entry;
            break;
        }
    }

    if (!targetEntry) return false;

    fseek(m_file, targetEntry->offset, SEEK_SET);
    outputBuffer.resize(targetEntry->size);
    if (fread(outputBuffer.data(), 1, targetEntry->size, m_file) != targetEntry->size) return false;

    return true;
}