#pragma once
#include <string>
#include <regex>
#include <algorithm>

namespace ScnHelper {
    inline std::string Clean(std::string text) {
        if (text.empty()) return "";

        text = std::regex_replace(text, std::regex("%[^;]+;"), "");

        size_t pos;
        while ((pos = text.find("\\n")) != std::string::npos) text.replace(pos, 2, "");

        text = std::regex_replace(text, std::regex("[\r\n]+"), "");

        text.erase(0, text.find_first_not_of(" \t"));
        if (!text.empty()) text.erase(text.find_last_not_of(" \t") + 1);
        return text;
    }
}