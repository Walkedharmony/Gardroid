#pragma once

#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>

struct ArcEntry {
    std::string name;
    uint32_t offset;
    uint32_t size;
};

class ArcParser {
public:
    explicit ArcParser(FILE* file);
    ~ArcParser();

    bool parse();
    const std::vector<ArcEntry>& get_file_list() const { return m_file_list; }

    bool extractFile(const std::string& targetName, const std::string& outputPath);
    bool extractToBuffer(const std::string& targetName, std::vector<char>& outputBuffer);

private:
    FILE* m_file;
    std::vector<ArcEntry> m_file_list;
    uint32_t m_base_offset;

    uint32_t readU32();
    bool isScriptFile(const std::string& name);

    inline uint8_t rotByteR(uint8_t v, int count) {
        return (v >> count) | (v << (8 - count));
    }
};