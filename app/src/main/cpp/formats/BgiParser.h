#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>

struct BgiEntry {
    std::string name;
    uint32_t offset;
    uint32_t size;
};

class BgiParser {
public:
    BgiParser(FILE* file);
    ~BgiParser();

    bool parse();
    const std::vector<BgiEntry>& get_file_list() const { return m_file_list; }


    bool extractFile(const std::string& targetName, const std::string& outputPath);


    bool extractToBuffer(const std::string& targetName, std::vector<char>& outputBuffer);

private:
    FILE* m_file;
    std::vector<BgiEntry> m_file_list;


    uint32_t readU32() {
        uint32_t val;
        fread(&val, 1, 4, m_file);
        return val;
    }
};