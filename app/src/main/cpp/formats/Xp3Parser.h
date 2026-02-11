#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <cstdio>

struct Xp3Segment {
    bool is_compressed;
    int64_t offset;
    uint64_t size;
    uint64_t packed_size;
};

class Xp3Entry {
public:
    std::u16string name;
    uint32_t hash;
    uint64_t unpacked_size;
    uint64_t packed_size;
    bool is_packed;
    std::vector<Xp3Segment> segments;
};

class Xp3Parser {
public:
    explicit Xp3Parser(FILE* file_stream);
    ~Xp3Parser();

    bool parse();
    const std::vector<Xp3Entry>& get_file_list() const;
    bool extractFile(const std::string& targetName, const std::string& outputPath);
    bool extractToBuffer(const std::string& targetName, std::vector<char>& outputBuffer);

private:
    FILE* m_file_stream;
    std::vector<Xp3Entry> m_file_list;

    bool read_index(std::vector<char>& out_index_data);
    void parse_index_data(const std::vector<char>& index_data);
};