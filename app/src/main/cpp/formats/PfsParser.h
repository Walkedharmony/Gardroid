#pragma once

#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>

struct PfsEntry {
    std::string name;
    uint32_t offset;
    uint32_t size;
};

class PfsParser {
public:
    explicit PfsParser(FILE* file);
    ~PfsParser();

    bool parse();
    const std::vector<PfsEntry>& get_file_list() const { return m_file_list; }

    bool extractFile(const std::string& targetName, const std::string& outputPath);
    bool extractToBuffer(const std::string& targetName, std::vector<char>& outputBuffer);

private:
    FILE* m_file;
    std::vector<PfsEntry> m_file_list;
    std::vector<uint8_t> m_key;
    int m_version;


    bool parse_v8(int version);
    bool parse_v2();


    uint32_t readU32();
    int32_t readS32();
    uint8_t readU8();


    void decrypt(std::vector<char>& buffer);
};