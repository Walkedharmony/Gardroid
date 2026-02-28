#include "AstPacker.h"
#include <fstream>
#include <vector>
#include <regex>
#include <sstream>
#include <android/log.h>

#define TAG_PACKER "AstPacker"
#define LOGE_PACKER(...) __android_log_print(ANDROID_LOG_ERROR, TAG_PACKER, __VA_ARGS__)
#define LOGI_PACKER(...) __android_log_print(ANDROID_LOG_INFO, TAG_PACKER, __VA_ARGS__)

using namespace std;

// Gunakan anonymous namespace untuk helper agar tidak bentrok dengan file cpp lain
namespace {
    string readFile(const string& path) {
        ifstream file(path, ios::in | ios::binary);
        if (!file) return "";
        ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    vector<string> readLines(const string& path) {
        vector<string> lines;
        ifstream file(path);
        string line;
        if (!file) return lines;
        while (getline(file, line)) {
            // Hapus karakter carriage return (\r) jika file dari Windows
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        return lines;
    }
}

int AstPacker::repack(const string& astPath, const string& txtPath, const string& outPath, const string& targetLang) {
    try {
        string astContent = readFile(astPath);
        vector<string> transLines = readLines(txtPath);

        if (astContent.empty() || transLines.empty()) {
            LOGE_PACKER("File kosong atau gagal dibaca. AST: %s, TXT: %s", astPath.c_str(), txtPath.c_str());
            return -1;
        }

        // Logika Regex (Anti-Crash)
        string regexStr = "(" + targetLang + "\\s*=\\s*\\{\\s*\\{\\s*)(?:\\\"([^\\\"]*)\\\"|\\[\\[([\\s\\S]*?)\\]\\])";
        regex pattern(regexStr);

        auto begin = sregex_iterator(astContent.begin(), astContent.end(), pattern);
        auto end = sregex_iterator();
        ptrdiff_t matchCount = distance(begin, end);

        if (matchCount != transLines.size()) {
            LOGE_PACKER("Mismatch! AST lines: %td, TXT lines: %zu", matchCount, transLines.size());
            return 0; // Gagal karena jumlah baris berbeda
        }

        ostringstream newContent;
        size_t lastPos = 0;
        int lineIdx = 0;

        // Reset iterator
        begin = sregex_iterator(astContent.begin(), astContent.end(), pattern);

        for (sregex_iterator i = begin; i != end; ++i) {
            smatch match = *i;
            newContent << astContent.substr(lastPos, match.position() - lastPos);

            string prefix = match[1].str();
            string translatedText = transLines[lineIdx];

            newContent << prefix;

            // Smart Quoting: Cegah Syntax Error Lua
            if (translatedText.find('"') != string::npos) {
                newContent << "[[" << translatedText << "]]";
            } else {
                newContent << "\"" << translatedText << "\"";
            }

            lastPos = match.position() + match.length();
            lineIdx++;
        }

        // Masukkan sisa file terbawah
        newContent << astContent.substr(lastPos);

        // Tulis ke output file
        ofstream outFs(outPath, ios::binary);
        if (!outFs) {
            LOGE_PACKER("Gagal membuat output file: %s", outPath.c_str());
            return -1;
        }
        outFs << newContent.str();
        outFs.close();

        LOGI_PACKER("Repack sukses tersimpan di: %s", outPath.c_str());
        return 1; // Sukses

    } catch (const std::regex_error& e) {
        LOGE_PACKER("Regex Error: %s", e.what());
        return -2;
    } catch (const exception& e) {
        LOGE_PACKER("Fatal Error: %s", e.what());
        return -3;
    }
}