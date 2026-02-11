#include "Xp3Parser.h"
#include <vector>
#include <cstring>
#include <sstream>
#include "zlib.h"
#include <codecvt>
#include <locale>
#include <algorithm>


struct MemoryReader {
    const char* data;
    size_t size;
    size_t pos;

    MemoryReader(const std::vector<char>& buffer) {
        data = buffer.data();
        size = buffer.size();
        pos = 0;
    }


    bool hasData() const {
        return pos < size;
    }


    size_t tell() const {
        return pos;
    }


    void seek(size_t new_pos) {
        if (new_pos <= size) {
            pos = new_pos;
        }
    }


    template <typename T>
    bool read(T& output) {
        if (pos + sizeof(T) > size) return false;
        memcpy(&output, data + pos, sizeof(T));
        pos += sizeof(T);
        return true;
    }


    bool readBuffer(void* dest, size_t length) {
        if (pos + length > size) return false;
        memcpy(dest, data + pos, length);
        pos += length;
        return true;
    }
};

bool decompress_zlib(const std::vector<char>& compressed, std::vector<char>& decompressed) {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;


    stream.avail_in = compressed.size();
    stream.next_in = (Bytef*)compressed.data();


    stream.avail_out = decompressed.size();
    stream.next_out = (Bytef*)decompressed.data();

    if (inflateInit(&stream) != Z_OK) {
        return false;
    }

    int ret = inflate(&stream, Z_FINISH);


    inflateEnd(&stream);


    if (ret != Z_STREAM_END) {
        return false;
    }

    return true;
}

const uint8_t XP3_HEADER[] = { 'X', 'P', '3', 0x0D, 0x0A, 0x20, 0x0A, 0x1A, 0x8B, 0x67, 0x01 };

Xp3Parser::Xp3Parser(FILE* file_stream) : m_file_stream(file_stream) {}

Xp3Parser::~Xp3Parser() {
    if (m_file_stream) {
        fclose(m_file_stream);
    }
}

bool Xp3Parser::parse() {
    if (!m_file_stream) {
        return false;
    }


    std::vector<char> header_buffer(sizeof(XP3_HEADER));
    size_t bytes_read = fread(header_buffer.data(), 1, header_buffer.size(), m_file_stream);
    if (bytes_read != header_buffer.size() || memcmp(header_buffer.data(), XP3_HEADER, sizeof(XP3_HEADER)) != 0) {
        return false;
    }


    std::vector<char> index_data;
    if (!read_index(index_data)) {
        return false;
    }


    parse_index_data(index_data);

    return !m_file_list.empty();
}

bool Xp3Parser::read_index(std::vector<char>& out_index_data) {

    fseek(m_file_stream, 11, SEEK_SET);
    int64_t index_offset;
    if (fread(&index_offset, sizeof(index_offset), 1, m_file_stream) != 1) {
        return false;
    }


    fseek(m_file_stream, index_offset, SEEK_SET);

    char header_type;
    if (fread(&header_type, sizeof(header_type), 1, m_file_stream) != 1) {
        return false;
    }

    if (header_type == 1) {
        int64_t packed_size, unpacked_size;
        if (fread(&packed_size, sizeof(packed_size), 1, m_file_stream) != 1) return false;
        if (fread(&unpacked_size, sizeof(unpacked_size), 1, m_file_stream) != 1) return false;

        if (packed_size <= 0 || unpacked_size <= 0) return false;

        std::vector<char> compressed_buffer(packed_size);
        if (fread(compressed_buffer.data(), 1, packed_size, m_file_stream) != packed_size) return false;

        out_index_data.resize(unpacked_size);
        return decompress_zlib(compressed_buffer, out_index_data);

    } else if (header_type == 0) {
        int64_t index_size;
        if (fread(&index_size, sizeof(index_size), 1, m_file_stream) != 1) return false;
        if (index_size <= 0) return false;

        out_index_data.resize(index_size);
        if (fread(out_index_data.data(), 1, index_size, m_file_stream) != index_size) return false;
        return true;
    }

    return false;
}

void Xp3Parser::parse_index_data(const std::vector<char>& index_data) {

    MemoryReader reader(index_data);

    while (reader.hasData()) {
        uint32_t chunk_signature;
        int64_t chunk_size;


        if (!reader.read(chunk_signature)) break;
        if (!reader.read(chunk_size)) break;

        size_t next_chunk_pos = reader.tell() + chunk_size;

        if (chunk_signature == 0x656C6946) { // "File"
            Xp3Entry current_entry;


            while (reader.tell() < next_chunk_pos) {
                uint32_t section_signature;
                int64_t section_size;

                if (!reader.read(section_signature)) break;
                if (!reader.read(section_size)) break;

                size_t next_section_pos = reader.tell() + section_size;

                switch (section_signature) {
                    case 0x6f666e69: { // "info"
                        uint32_t flags;
                        reader.read(flags);

                        reader.read(current_entry.unpacked_size);
                        reader.read(current_entry.packed_size);
                        current_entry.is_packed = (current_entry.unpacked_size != current_entry.packed_size);

                        int16_t name_len;
                        reader.read(name_len);

                        if (name_len > 0) {
                            std::vector<char16_t> name_buffer(name_len);

                            reader.readBuffer(name_buffer.data(), name_len * 2);
                            current_entry.name.assign(name_buffer.begin(), name_buffer.end());
                        }
                        break;
                    }
                    case 0x6d676573: { // "segm"
                        int segment_count = section_size / 28;
                        for (int i = 0; i < segment_count; ++i) {
                            Xp3Segment segment;
                            uint32_t is_compressed_flag;

                            reader.read(is_compressed_flag);
                            segment.is_compressed = (is_compressed_flag == 1);
                            reader.read(segment.offset);
                            reader.read(segment.size);
                            reader.read(segment.packed_size);

                            current_entry.segments.push_back(segment);
                        }
                        break;
                    }
                    case 0x726c6461: { // "adlr"
                        reader.read(current_entry.hash);
                        break;
                    }
                }
                reader.seek(next_section_pos);
            }
            m_file_list.push_back(current_entry);
        }

        reader.seek(next_chunk_pos);
    }
}
bool Xp3Parser::extractFile(const std::string& targetName, const std::string& outputPath) {

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;

    const Xp3Entry* targetEntry = nullptr;

    for (const auto& entry : m_file_list) {
        std::string entryNameUtf8;
        try {

            entryNameUtf8 = converter.to_bytes(entry.name);
        } catch (...) {
            continue;
        }

        std::replace(entryNameUtf8.begin(), entryNameUtf8.end(), '\\', '/');

        if (entryNameUtf8 == targetName) {
            targetEntry = &entry;
            break;
        }
    }

    if (!targetEntry) return false;

    std::vector<char> outputBuffer;


    for (const auto& segment : targetEntry->segments) {
        fseek(m_file_stream, segment.offset, SEEK_SET);
        std::vector<char> segmentData(segment.packed_size);
        if (fread(segmentData.data(), 1, segment.packed_size, m_file_stream) != segment.packed_size) return false;

        if (segment.is_compressed) {
            std::vector<char> decompressedChunk(segment.size);
            if (!decompress_zlib(segmentData, decompressedChunk)) return false;
            outputBuffer.insert(outputBuffer.end(), decompressedChunk.begin(), decompressedChunk.end());
        } else {
            outputBuffer.insert(outputBuffer.end(), segmentData.begin(), segmentData.end());
        }
    }

    FILE* outFile = fopen(outputPath.c_str(), "wb");
    if (!outFile) return false;
    fwrite(outputBuffer.data(), 1, outputBuffer.size(), outFile);
    fclose(outFile);

    return true;
}

bool Xp3Parser::extractToBuffer(const std::string& targetName, std::vector<char>& outputBuffer) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    const Xp3Entry* targetEntry = nullptr;

    for (const auto& entry : m_file_list) {
        std::string entryNameUtf8;
        try {
            entryNameUtf8 = converter.to_bytes(entry.name);
        } catch (...) { continue; }
        std::replace(entryNameUtf8.begin(), entryNameUtf8.end(), '\\', '/');

        if (entryNameUtf8 == targetName) {
            targetEntry = &entry;
            break;
        }
    }

    if (!targetEntry) return false;

    outputBuffer.clear();
    outputBuffer.reserve(targetEntry->unpacked_size);

    for (const auto& segment : targetEntry->segments) {
        fseek(m_file_stream, segment.offset, SEEK_SET);
        std::vector<char> segmentData(segment.packed_size);
        if (fread(segmentData.data(), 1, segment.packed_size, m_file_stream) != segment.packed_size) return false;

        if (segment.is_compressed) {
            std::vector<char> decompressedChunk(segment.size);
            if (!decompress_zlib(segmentData, decompressedChunk)) return false;
            outputBuffer.insert(outputBuffer.end(), decompressedChunk.begin(), decompressedChunk.end());
        } else {
            outputBuffer.insert(outputBuffer.end(), segmentData.begin(), segmentData.end());
        }
    }

    return true;
}
const std::vector<Xp3Entry>& Xp3Parser::get_file_list() const {
    return m_file_list;
}