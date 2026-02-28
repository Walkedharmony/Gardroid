#include "KirikiriKS.h"
#include <stdexcept>
#include <zlib.h>
#include <regex>
#include <sstream>
#include <fstream>

// --- IMPLEMENTASI KIRIKIRI DESCRAMBLER (Tetap Sama) ---
std::vector<uint8_t> KirikiriDescrambler::Descramble(std::vector<uint8_t>& data) {
    if (data.size() < 5) return data;
    // Pengecekan Scrambled Header (FE FE 00/01/02 FF FE)
    if (data[0] != 0xFE || data[1] != 0xFE) return data;
    if (data[3] != 0xFF || data[4] != 0xFE) throw std::runtime_error("Scrambled Kirikiri file is missing BOM.");

    uint8_t mode = data[2];
    switch (mode) {
        case 0: return DescrambleMode0(data);
        case 1: return DescrambleMode1(data);
        case 2: return DecompressMode2(data);
        default: throw std::runtime_error("Unsupported scrambling mode.");
    }
}

std::vector<uint8_t> KirikiriDescrambler::DescrambleMode0(std::vector<uint8_t>& data) {
    for (size_t i = 5; i < data.size(); i += 2) {
        if (i + 1 < data.size()) {
            if (data[i + 1] == 0 && data[i] < 0x20) continue;
            data[i + 1] ^= (data[i] & 0xFE);
            data[i] ^= 1;
        }
    }
    return std::vector<uint8_t>(data.begin() + 3, data.end()); // FF FE ...
}

std::vector<uint8_t> KirikiriDescrambler::DescrambleMode1(std::vector<uint8_t>& data) {
    for (size_t i = 5; i < data.size(); i += 2) {
        if (i + 1 < data.size()) {
            uint16_t c = data[i] | (data[i + 1] << 8);
            c = ((c & 0xAAAA) >> 1) | ((c & 0x5555) << 1);
            data[i] = (uint8_t)(c & 0xFF);
            data[i + 1] = (uint8_t)(c >> 8);
        }
    }
    return std::vector<uint8_t>(data.begin() + 3, data.end());
}

std::vector<uint8_t> KirikiriDescrambler::DecompressMode2(std::vector<uint8_t>& data) {
    size_t offset = 5;
    auto readInt64 = [&](size_t pos) -> int64_t {
        int64_t val = 0;
        for (int j = 0; j < 8; j++) val |= ((int64_t)data[pos + j] << (j * 8));
        return val;
    };

    int64_t compressedLength = readInt64(offset); offset += 8;
    int64_t uncompressedLength = readInt64(offset); offset += 8;
    offset += 2;

    std::vector<uint8_t> uncompressedData(2 + uncompressedLength);
    uncompressedData[0] = 0xFF; uncompressedData[1] = 0xFE;

    z_stream strm;
    strm.zalloc = Z_NULL; strm.zfree = Z_NULL; strm.opaque = Z_NULL;
    strm.avail_in = data.size() - offset;
    strm.next_in = data.data() + offset;
    strm.avail_out = uncompressedLength;
    strm.next_out = uncompressedData.data() + 2;

    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) throw std::runtime_error("Failed to initialize zlib.");
    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END && ret != Z_OK) throw std::runtime_error("Zlib decompression failed.");
    return uncompressedData;
}
const std::set<std::string> KirikiriParser::NameCommands = { "nm", "set_title", "speaker", "Talk", "talk", "cn", "name", "名前" };
const std::set<std::string> KirikiriParser::MessageCommands = { "sel01", "sel02", "sel03", "sel04", "AddSelect", "ruby" };

std::string KirikiriParser::Utf16LeToUtf8(const uint8_t* data, size_t size) {
    std::string out;
    out.reserve(size);
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint32_t cp = data[i] | (data[i+1] << 8);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < size) {
            uint32_t trail = data[i+2] | (data[i+3] << 8);
            if (trail >= 0xDC00 && trail <= 0xDFFF) {
                cp = ((cp - 0xD800) << 10) + (trail - 0xDC00) + 0x10000;
                i += 2;
            }
        }
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

std::string KirikiriParser::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void KirikiriParser::ExtractAttributes(const std::string& cmdLine, std::vector<std::string>& outText) {
    size_t space = cmdLine.find(' ');
    if (space == std::string::npos) return;

    std::string cmd = cmdLine.substr(0, space);
    if (NameCommands.find(cmd) == NameCommands.end() && MessageCommands.find(cmd) == MessageCommands.end()) {
        return;
    }
    std::regex attrReg("([^= ]+)\\s*=\\s*(\"([^\"]*)\"|'([^']*)'|([^\"'\\s]+))");

    auto it_begin = std::sregex_iterator(cmdLine.begin() + space, cmdLine.end(), attrReg);
    auto it_end = std::sregex_iterator();

    for (auto it = it_begin; it != it_end; ++it) {
        std::smatch match = *it;
        std::string val = match[3].matched ? match[3].str() :
                          match[4].matched ? match[4].str() :
                          match[5].matched ? match[5].str() : "";

        if (!val.empty()) outText.push_back(val);
    }
}

int KirikiriParser::ExtractTextToFile(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream inFile(inputPath, std::ios::binary | std::ios::ate);
    if (!inFile) return 0;

    std::streamsize size = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!inFile.read(reinterpret_cast<char*>(buffer.data()), size)) return 0;

    std::vector<std::string> lines = ExtractText(buffer);
    if (lines.empty()) return 0;

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) return 0;

    int count = 0;
    for (const auto& line : lines) {
        outFile << line << "\r\n";
        count++;
    }
    return count;
}

void KirikiriParser::ParseInlineText(const std::string& line, std::vector<std::string>& outText) {
    std::string currentText = "";
    for (size_t i = 0; i < line.length(); ) {
        if (line[i] == '[') {
            size_t endBrack = line.find(']', i);
            if (endBrack != std::string::npos) {
                if (!currentText.empty()) {
                    outText.push_back(currentText);
                    currentText = "";
                }
                std::string tag = line.substr(i + 1, endBrack - i - 1);
                ExtractAttributes(tag, outText);
                i = endBrack + 1;
                continue;
            }
        }
        currentText += line[i];
        i++;
    }
    if (!currentText.empty()) {
        std::string cleaned = Trim(currentText);
        if (!cleaned.empty()) outText.push_back(cleaned);
    }
}

std::vector<std::string> KirikiriParser::ExtractText(std::vector<uint8_t>& buffer) {
    std::vector<std::string> extractedLines;

    std::vector<uint8_t> data = KirikiriDescrambler::Descramble(buffer);
    if (data.empty()) return extractedLines;

    std::string utf8Script;
    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        utf8Script = Utf16LeToUtf8(data.data() + 2, data.size() - 2);
    }
    else if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        utf8Script = std::string((char*)data.data() + 3, data.size() - 3);
    }
    else {
        utf8Script = std::string((char*)data.data(), data.size());
    }

    std::istringstream stream(utf8Script);
    std::string line;
    bool inScript = false;

    while (std::getline(stream, line)) {
        std::string trimLine = Trim(line);
        if (trimLine.empty()) continue;

        if (trimLine == "[iscript]" || trimLine == "@iscript" || trimLine.find("[macro") == 0 || trimLine.find("@macro") == 0) {
            inScript = true; continue;
        }
        if (trimLine == "[endscript]" || trimLine == "@endscript" || trimLine == "[endmacro]" || trimLine == "@endmacro") {
            inScript = false; continue;
        }
        if (inScript) continue;
        if (trimLine[0] == ';') continue;

        if (trimLine[0] == '*') {
            size_t pipe = trimLine.find('|');
            if (pipe != std::string::npos && pipe != trimLine.length() - 1) {
                extractedLines.push_back(Trim(trimLine.substr(pipe + 1)));
            }
            continue;
        }
        if (trimLine[0] == '#') {
            if (trimLine.length() > 1) extractedLines.push_back(Trim(trimLine.substr(1)));
            continue;
        }
        if (trimLine[0] == '@') {
            ExtractAttributes(trimLine.substr(1), extractedLines);
            continue;
        }

        ParseInlineText(trimLine, extractedLines);
    }

    return extractedLines;
}
std::vector<std::string> KirikiriParser::LoadTxtLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream file(path, std::ios::binary);
    if (!file) return lines;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (content.size() >= 3 && (unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
        content = content.substr(3);
    }

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::vector<uint8_t> KirikiriParser::Utf8ToUtf16Le(const std::string& utf8) {
    std::vector<uint8_t> out;
    out.push_back(0xFF); out.push_back(0xFE);
    size_t i = 0;
    while (i < utf8.length()) {
        uint32_t cp = 0;
        unsigned char c = utf8[i];
        if (c <= 0x7F) { cp = c; i += 1; }
        else if ((c & 0xE0) == 0xC0) { cp = ((c & 0x1F) << 6) | (utf8[i+1] & 0x3F); i += 2; }
        else if ((c & 0xF0) == 0xE0) { cp = ((c & 0x0F) << 12) | ((utf8[i+1] & 0x3F) << 6) | (utf8[i+2] & 0x3F); i += 3; }
        else if ((c & 0xF8) == 0xF0) { cp = ((c & 0x07) << 18) | ((utf8[i+1] & 0x3F) << 12) | ((utf8[i+2] & 0x3F) << 6) | (utf8[i+3] & 0x3F); i += 4; }
        else { cp = '?'; i += 1; }

        if (cp >= 0x10000) {
            cp -= 0x10000;
            uint16_t high = 0xD800 | (cp >> 10);
            uint16_t low = 0xDC00 | (cp & 0x3FF);
            out.push_back(high & 0xFF); out.push_back(high >> 8);
            out.push_back(low & 0xFF); out.push_back(low >> 8);
        } else {
            out.push_back(cp & 0xFF); out.push_back(cp >> 8);
        }
    }
    return out;
}

std::string KirikiriParser::ReplaceAttributesInTag(const std::string& tag, const std::vector<std::string>& txtLines, size_t& txtIdx) {
    size_t space = tag.find(' ');
    if (space == std::string::npos) return tag;
    std::string cmd = tag.substr(0, space);
    if (NameCommands.find(cmd) == NameCommands.end() && MessageCommands.find(cmd) == MessageCommands.end()) {
        return tag;
    }

    std::string result = tag.substr(0, space);
    std::regex attrReg("([^= ]+)\\s*=\\s*(\"([^\"]*)\"|'([^']*)'|([^\"'\\s]+))");
    auto it_begin = std::sregex_iterator(tag.begin() + space, tag.end(), attrReg);
    auto it_end = std::sregex_iterator();

    size_t lastPos = space;
    for (auto it = it_begin; it != it_end; ++it) {
        std::smatch match = *it;
        result += tag.substr(lastPos, match.position() - lastPos);

        std::string attrName = match[1].str();
        std::string fullMatch = match[0].str();
        std::string val = match[3].matched ? match[3].str() : match[4].matched ? match[4].str() : match[5].matched ? match[5].str() : "";

        if (!val.empty() && txtIdx < txtLines.size()) {
            std::string newVal = txtLines[txtIdx++];
            if (match[3].matched) result += attrName + "=\"" + newVal + "\"";
            else if (match[4].matched) result += attrName + "='" + newVal + "'";
            else result += attrName + "=" + newVal;
        } else {
            result += fullMatch;
        }
        lastPos = match.position() + match.length();
    }
    result += tag.substr(lastPos);
    return result;
}

int KirikiriParser::RepackText(const std::string& ksPath, const std::string& txtPath, const std::string& outputPath) {
    std::vector<std::string> txtLines = LoadTxtLines(txtPath);
    if (txtLines.empty()) return 0; // Gagal load TXT

    std::ifstream inFile(ksPath, std::ios::binary | std::ios::ate);
    if (!inFile) return 0;
    std::streamsize size = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!inFile.read(reinterpret_cast<char*>(buffer.data()), size)) return 0;

    std::vector<uint8_t> data = KirikiriDescrambler::Descramble(buffer);
    std::string utf8Script;
    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        utf8Script = Utf16LeToUtf8(data.data() + 2, data.size() - 2);
    } else if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        utf8Script = std::string((char*)data.data() + 3, data.size() - 3);
    } else {
        utf8Script = std::string((char*)data.data(), data.size());
    }

    std::istringstream stream(utf8Script);
    std::string line;
    std::string outScript = "";
    bool inScript = false;
    size_t txtIdx = 0;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string trimLine = Trim(line);

        if (trimLine.empty() || trimLine[0] == ';') {
            outScript += line + "\r\n"; continue;
        }

        if (trimLine == "[iscript]" || trimLine == "@iscript" || trimLine.find("[macro") == 0 || trimLine.find("@macro") == 0) {
            inScript = true; outScript += line + "\r\n"; continue;
        }
        if (trimLine == "[endscript]" || trimLine == "@endscript" || trimLine == "[endmacro]" || trimLine == "@endmacro") {
            inScript = false; outScript += line + "\r\n"; continue;
        }
        if (inScript) {
            outScript += line + "\r\n"; continue;
        }

        if (trimLine[0] == '*') {
            size_t pipe = line.find('|');
            if (pipe != std::string::npos && txtIdx < txtLines.size()) {
                outScript += line.substr(0, pipe + 1) + txtLines[txtIdx++] + "\r\n";
                continue;
            }
        } else if (trimLine[0] == '#') {
            size_t hash = line.find('#');
            if (hash != std::string::npos && txtIdx < txtLines.size()) {
                outScript += line.substr(0, hash + 1) + txtLines[txtIdx++] + "\r\n";
                continue;
            }
        } else if (trimLine[0] == '@') {
            size_t atPos = line.find('@');
            outScript += line.substr(0, atPos + 1) + ReplaceAttributesInTag(line.substr(atPos + 1), txtLines, txtIdx) + "\r\n";
            continue;
        }
        std::string newLine = "";
        size_t lastPos = 0;
        for (size_t i = 0; i < line.length(); ) {
            if (line[i] == '[') {
                size_t endBrack = line.find(']', i);
                if (endBrack != std::string::npos) {
                    if (i > lastPos) {
                        std::string segment = line.substr(lastPos, i - lastPos);
                        if (!Trim(segment).empty() && txtIdx < txtLines.size()) newLine += txtLines[txtIdx++];
                        else newLine += segment;
                    }
                    std::string tag = line.substr(i + 1, endBrack - i - 1);
                    newLine += "[" + ReplaceAttributesInTag(tag, txtLines, txtIdx) + "]";
                    i = endBrack + 1;
                    lastPos = i;
                    continue;
                }
            }
            i++;
        }
        if (lastPos < line.length()) {
            std::string segment = line.substr(lastPos);
            if (!Trim(segment).empty() && txtIdx < txtLines.size()) newLine += txtLines[txtIdx++];
            else newLine += segment;
        }
        outScript += newLine + "\r\n";
    }

    std::vector<uint8_t> finalOut = Utf8ToUtf16Le(outScript);
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) return 0;
    outFile.write(reinterpret_cast<const char*>(finalOut.data()), finalOut.size());

    return txtIdx > 0 ? 1 : 0;
}