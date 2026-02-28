#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <set>

class KirikiriDescrambler {
public:
    static std::vector<uint8_t> Descramble(std::vector<uint8_t>& data);

private:
    static std::vector<uint8_t> DescrambleMode0(std::vector<uint8_t>& data);
    static std::vector<uint8_t> DescrambleMode1(std::vector<uint8_t>& data);
    static std::vector<uint8_t> DecompressMode2(std::vector<uint8_t>& data);
};

class KirikiriParser {
public:
    static std::vector<std::string> ExtractText(std::vector<uint8_t>& buffer);
    static int ExtractTextToFile(const std::string& inputPath, const std::string& outputPath);
    static int RepackText(const std::string& ksPath, const std::string& txtPath, const std::string& outputPath);

private:
    static std::string Utf16LeToUtf8(const uint8_t* data, size_t size);
    static std::string Trim(const std::string& str);
    static void ParseInlineText(const std::string& line, std::vector<std::string>& outText);
    static void ExtractAttributes(const std::string& cmdLine, std::vector<std::string>& outText);
    static std::string ReplaceAttributesInTag(const std::string& tag, const std::vector<std::string>& txtLines, size_t& txtIdx);
    static std::vector<uint8_t> Utf8ToUtf16Le(const std::string& utf8);
    static std::vector<std::string> LoadTxtLines(const std::string& path);

    static const std::set<std::string> NameCommands;
    static const std::set<std::string> MessageCommands;
};