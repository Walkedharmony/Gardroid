#include "PfsParser.h"
#include "../utils/SHA-1.h"
#include <cstring>
#include <algorithm>
#include <iostream>

PfsParser::PfsParser(FILE* file) : m_file(file), m_version(0) {}

PfsParser::~PfsParser() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}


uint8_t PfsParser::readU8() {
    uint8_t val;
    fread(&val, 1, 1, m_file);
    return val;
}

uint32_t PfsParser::readU32() {
    uint32_t val;
    if (fread(&val, 1, 4, m_file) != 4) return 0;
    return val;
}

int32_t PfsParser::readS32() {
    int32_t val;
    if (fread(&val, 1, 4, m_file) != 4) return 0;
    return val;
}

bool PfsParser::parse() {
    if (!m_file) return false;

    fseek(m_file, 0, SEEK_SET);
    char sig[3];
    if (fread(sig, 1, 2, m_file) != 2) return false;

    if (sig[0] != 'p' || sig[1] != 'f') return false;

    uint8_t verChar = readU8();
    m_version = verChar - '0';

    switch (m_version) {
        case 8:
        case 6:
        case 9:
        case 5:
        case 4:
            return parse_v8(m_version);
        case 2:
            return parse_v2();
        default:

            return false;
    }
}

bool PfsParser::parse_v8(int version) {
    // Offset 3: uint index_size
    fseek(m_file, 3, SEEK_SET);
    uint32_t index_size = readU32();

    std::vector<uint8_t> index_data(index_size);
    fseek(m_file, 7, SEEK_SET);
    if (fread(index_data.data(), 1, index_size, m_file) != index_size) return false;

    if (version == 4 || version == 5 || version == 8 || version == 9) {

        m_key = SHA1_Simple::Calculate(index_data);
    } else {
        m_key.clear();
    }

    size_t pos = 0;

    if (index_data.size() < 4) return false;
    int32_t count = 0;
    memcpy(&count, index_data.data() + pos, 4);
    pos += 4;

    for (int i = 0; i < count; ++i) {
        if (pos + 4 > index_data.size()) break;

        int32_t name_len = 0;
        memcpy(&name_len, index_data.data() + pos, 4);
        pos += 4;

        if (pos + name_len > index_data.size()) break;

        std::string name(reinterpret_cast<char*>(index_data.data() + pos), name_len);

        pos += name_len;
        pos += 4;

        if (pos + 8 > index_data.size()) break;


        uint32_t offset = 0;
        uint32_t size = 0;
        memcpy(&offset, index_data.data() + pos, 4);
        memcpy(&size, index_data.data() + pos + 4, 4);
        pos += 8;

        PfsEntry entry;
        entry.name = name;
        entry.offset = offset;
        entry.size = size;
        m_file_list.push_back(entry);
    }

    return !m_file_list.empty();
}

bool PfsParser::parse_v2() {

    fseek(m_file, 0xB, SEEK_SET);
    int32_t count = readS32();


    fseek(m_file, 3, SEEK_SET);
    uint32_t index_size = readU32();

    std::vector<uint8_t> index_data(index_size);
    fseek(m_file, 7, SEEK_SET);
    if (fread(index_data.data(), 1, index_size, m_file) != index_size) return false;

    m_key.clear();

    size_t pos = 8;

    for (int i = 0; i < count; ++i) {
        if (pos + 4 > index_data.size()) break;

        int32_t name_len = 0;
        memcpy(&name_len, index_data.data() + pos, 4);

        pos += 4;
        if (pos + name_len > index_data.size()) break;

        std::string name(reinterpret_cast<char*>(index_data.data() + pos), name_len);

        pos += name_len;
        pos += 12;

        if (pos + 8 > index_data.size()) break;

        uint32_t offset = 0;
        uint32_t size = 0;
        memcpy(&offset, index_data.data() + pos, 4);
        memcpy(&size, index_data.data() + pos + 4, 4);
        pos += 8;

        PfsEntry entry;
        entry.name = name;
        entry.offset = offset;
        entry.size = size;
        m_file_list.push_back(entry);
    }

    return !m_file_list.empty();
}

void PfsParser::decrypt(std::vector<char>& buffer) {
    if (m_key.empty()) return;

    size_t keyLen = m_key.size();
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] ^= m_key[i % keyLen];
    }
}

bool PfsParser::extractFile(const std::string& targetName, const std::string& outputPath) {
    std::vector<char> buffer;
    if (!extractToBuffer(targetName, buffer)) return false;

    FILE* out = fopen(outputPath.c_str(), "wb");
    if (!out) return false;
    fwrite(buffer.data(), 1, buffer.size(), out);
    fclose(out);
    return true;
}

bool PfsParser::extractToBuffer(const std::string& targetName, std::vector<char>& outputBuffer) {
    const PfsEntry* target = nullptr;

    for (const auto& entry : m_file_list) {
        std::string dbName = entry.name;
        std::replace(dbName.begin(), dbName.end(), '\\', '/');

        if (dbName == targetName) {
            target = &entry;
            break;
        }
    }

    if (!target) return false;

    fseek(m_file, target->offset, SEEK_SET);
    outputBuffer.resize(target->size);
    if (fread(outputBuffer.data(), 1, target->size, m_file) != target->size) return false;

    decrypt(outputBuffer);

    return true;
}