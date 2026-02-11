#include "AstParser.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <filesystem>
#include <android/log.h>

#define TAG "AstParser"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using namespace std;

string AstParser::readFile(const string& path) {
    ifstream file(path, ios::in | ios::binary);
    if (!file) {
        LOGE("Gagal membuka file: %s", path.c_str());
        return "";
    }
    ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool AstParser::isValidLanguageKey(const string& key) {

    for (char c : key) {
        if (isdigit(c)) return false;
    }

    static const set<string> ignoredKeys = {
            "ast", "text", "vo", "label", "select", "name",
            "msg", "bg", "fg", "se", "ex", "cgdel", "msgoff",
            "user", "quake", "extrans", "bgm", "video",
            "linknext", "linkback", "line", "pagebreak",
            "hide", "clickpass", "savetitle", "eval", "sys",
            "astname", "astver", "mode", "next", "scene",
            "rt", "log", "param", "include", "var",
            "top", "bottom", "left", "right", "center",
            "width", "height", "x", "y", "z", "zoom", "alpha",
            "time", "ease", "accel", "file", "path", "rule",
            "ch", "index", "set", "buf", "storage", "visible",
            "layer", "pos", "size", "anchor", "pivot",
            "r", "g", "b", "a", "m", "w", "k", "p", "s"
    };

    if (ignoredKeys.find(key) != ignoredKeys.end()) return false;

    return true;
}

vector<string> AstParser::detectLanguages(const string& filePath) {
    LOGD("Scanning languages in: %s", filePath.c_str());

    string content = readFile(filePath);
    if (content.empty()) return {};

    set<string> languages;

    try {

        regex pattern(R"((\w+)\s*=\s*\{)");

        auto words_begin = sregex_iterator(content.begin(), content.end(), pattern);
        auto words_end = sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            smatch match = *i;
            string key = match[1].str();

            if (isValidLanguageKey(key)) {
                LOGD("[FOUND] Bahasa terdeteksi: %s", key.c_str());
                languages.insert(key);
            }
        }
    } catch (const std::regex_error& e) {
        LOGE("Regex Error: %s", e.what());
    }

    return vector<string>(languages.begin(), languages.end());
}

int AstParser::extractText(const std::string& inputPath, const std::string& outputPath, const std::string& targetLang) {
    string content = readFile(inputPath);
    if (content.empty()) return 0;

    string regexStr = targetLang + R"(\s*=\s*\{\s*\{\s*(?:\"([\s\S]*?)\"|\[\[([\s\S]*?)\]\]))";

    try {
        regex pattern(regexStr);
        auto begin = sregex_iterator(content.begin(), content.end(), pattern);
        auto end = sregex_iterator();

        ofstream outFile(outputPath);
        if (!outFile) return 0;

        int count = 0;
        for (sregex_iterator i = begin; i != end; ++i) {
            smatch match = *i;
            string text = match[1].matched ? match[1].str() : match[2].str();
            outFile << text << "\n";
            count++;
        }
        outFile.close();
        return count;
    } catch (...) {
        return 0;
    }
}