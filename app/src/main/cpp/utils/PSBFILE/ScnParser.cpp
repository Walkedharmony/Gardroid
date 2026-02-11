#include "ScnParser.h"
#include "psb.hpp"
#include "ScnTxtLogic.hpp"
#include "ScnKsLogic.hpp"

#include <fstream>
#include <vector>
#include <android/log.h>

#define TAG "ScnParser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

bool EndsWith(const std::string& fullString, const std::string& ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    return false;
}

int ScnParser::extractText(const std::string& inputPath, const std::string& outputPath) {
    LOGI("Opening file: %s", inputPath.c_str());

    std::ifstream f(inputPath, std::ios::binary);
    if (!f.is_open()) {
        LOGE("Failed to open input file.");
        return 0;
    }

    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);

    if (size == 0) {
        LOGE("File is empty.");
        return 0;
    }

    std::vector<unsigned char> buffer(size);
    f.read((char*)buffer.data(), size);
    f.close();


    try {
        psb_t psb(buffer.data());

        const psb_objects_t* root = psb.get_objects();

        if (!root) {
            LOGE("Root node is NULL or Invalid PSB.");
            return 0;
        }


        std::ofstream outFile(outputPath);
        if (!outFile.is_open()) {
            LOGE("Failed to create output file: %s", outputPath.c_str());
            return 0;
        }

        if (EndsWith(inputPath, ".txt.scn")) {
            LOGI("Mode: .TXT.SCN (Modern Parser)");

            std::vector<TXT::Dialog> dialogs;
            int idCounter = 1;

            TXT::Extract(psb, root, dialogs, idCounter);

            for (const auto& d : dialogs) {
                outFile << d.id << "|" << d.type << "|" << d.text << "\n";
            }
            LOGI("Exported %zu lines (TXT Mode).", dialogs.size());

        } else {
            LOGI("Mode: KS/Standard SCN (Classic Parser)");

            ScnKs::Extract(psb, root, outFile);

            LOGI("Export finished (KS Mode).");
        }

        outFile.close();
        return 1;

    } catch (std::exception& e) {
        LOGE("Exception C++: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("Unknown Critical Error.");
        return 0;
    }
}