#include "ArcParser.h"
#include <algorithm>
#include <cstring>

ArcParser::ArcParser(FILE* file) : m_file(file), m_base_offset(0) {}

ArcParser::~ArcParser() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

uint32_t ArcParser::readU32() {
    uint32_t val;
    if (fread(&val, 1, 4, m_file) != 4) return 0;
    return val;
}

bool ArcParser::isScriptFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.size() >= 4 && (lower.substr(lower.size() - 4) == ".ws2" ||
                                 (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".json"));
}

bool ArcParser::parse() {
    if (!m_file) return false;

    fseek(m_file, 0, SEEK_END);
    uint32_t fileSize = ftell(m_file);
    fseek(m_file, 0, SEEK_SET);

    uint32_t count = readU32();
    if (count == 0 || count > 200000) return false;

    uint32_t index_size = readU32();
    m_base_offset = 8 + index_size;

    if (index_size > m_base_offset || m_base_offset >= fileSize) return false;

    for (uint32_t i = 0; i < count; ++i) {
        if (ftell(m_file) >= m_base_offset) return false;

        uint32_t size = readU32();
        uint32_t offset = readU32();

        std::string name = "";
        while (true) {
            if (ftell(m_file) >= m_base_offset) return false;
            uint16_t c;
            if (fread(&c, 1, 2, m_file) != 2) return false;
            if (c == 0) break;

            if (c < 0x80) name += (char)c;
            else if (c < 0x800) {
                name += (char)(0xC0 | (c >> 6));
                name += (char)(0x80 | (c & 0x3F));
            } else {
                name += (char)(0xE0 | (c >> 12));
                name += (char)(0x80 | ((c >> 6) & 0x3F));
                name += (char)(0x80 | (c & 0x3F));
            }
        }

        if (name.empty()) return false;

        ArcEntry entry;
        entry.name = name;
        entry.offset = offset;
        entry.size = size;
        m_file_list.push_back(entry);
    }

    return ftell(m_file) == m_base_offset;
}

bool ArcParser::extractToBuffer(const std::string& targetName, std::vector<char>& outputBuffer) {
    const ArcEntry* targetEntry = nullptr;

    for (const auto& entry : m_file_list) {
        std::string dbName = entry.name;
        std::replace(dbName.begin(), dbName.end(), '\\', '/');

        std::string searchName = targetName;
        std::replace(searchName.begin(), searchName.end(), '\\', '/');

        if (dbName == searchName) {
            targetEntry = &entry;
            break;
        }
    }

    if (!targetEntry) return false;

    fseek(m_file, m_base_offset + targetEntry->offset, SEEK_SET);
    outputBuffer.resize(targetEntry->size);
    if (fread(outputBuffer.data(), 1, targetEntry->size, m_file) != targetEntry->size) return false;

    if (isScriptFile(targetEntry->name)) {
        for (size_t i = 0; i < outputBuffer.size(); ++i) {
            uint8_t val = static_cast<uint8_t>(outputBuffer[i]);
            outputBuffer[i] = static_cast<char>(rotByteR(val, 2));
        }
    }

    return true;
}

bool ArcParser::extractFile(const std::string& targetName, const std::string& outputPath) {
    std::vector<char> buffer;
    if (!extractToBuffer(targetName, buffer)) return false;

    FILE* out = fopen(outputPath.c_str(), "wb");
    if (!out) return false;
    fwrite(buffer.data(), 1, buffer.size(), out);
    fclose(out);
    return true;
}