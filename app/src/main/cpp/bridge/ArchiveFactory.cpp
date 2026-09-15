#include "ArchiveFactory.h"
#include "../utils/StringUtils.h"
#include "../formats/Xp3Parser.h"
#include "../formats/PfsParser.h"
#include "../formats/BgiParser.h"
#include "../formats/ArcParser.h"
#include "../utils/TlgDecoder.h"
#include "../utils/BgiImageDecoder.h"
#include "../utils/KirikiriKS.h"
#include <codecvt>
#include <locale>
#include <algorithm>
#include <android/log.h>

#define TAG "GardroidNative"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

bool ArchiveFactory::isPfsArchive(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char sig[2];
    if (fread(sig, 1, 2, f) != 2) { fclose(f); return false; }
    fclose(f);
    return sig[0] == 'p' && sig[1] == 'f';
}

bool ArchiveFactory::isBgiArchive(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char sig[13];
    if (fread(sig, 1, 12, f) != 12) { fclose(f); return false; }
    fclose(f);
    if (memcmp(sig, "PackFile    ", 12) == 0) return true;
    if (memcmp(sig, "BURIKO ARC20", 12) == 0) return true;
    return false;
}

bool ArchiveFactory::isWillArcArchive(const char* path) {
    if (isBgiArchive(path)) return false;

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint32_t count = 0, index_size = 0;
    if (fread(&count, 1, 4, f) == 4 && fread(&index_size, 1, 4, f) == 4) {
        fclose(f);
        return count > 0 && count < 200000 && index_size > 0;
    }
    fclose(f);
    return false;
}

ArchiveHandle* ArchiveFactory::createParser(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return nullptr;

    ArchiveHandle* handle = new ArchiveHandle();

    if (isBgiArchive(path)) {
        BgiParser* parser = new BgiParser(file);
        if (parser->parse()) {
            handle->type = ArchiveHandle::BGI;
            handle->parserInstance = parser;
        } else {
            delete parser;
            delete handle;
            handle = nullptr;
        }
    }
    else if (isPfsArchive(path)) {
        PfsParser* parser = new PfsParser(file);
        if (parser->parse()) {
            handle->type = ArchiveHandle::PFS;
            handle->parserInstance = parser;
        } else {
            delete parser;
            delete handle;
            handle = nullptr;
        }
    }
    else if (isWillArcArchive(path)) {
        ArcParser* parser = new ArcParser(file);
        if (parser->parse()) {
            handle->type = ArchiveHandle::ARC;
            handle->parserInstance = parser;
        } else {
            delete parser;
            delete handle;
            handle = nullptr;
        }
    }
    else {
        Xp3Parser* parser = new Xp3Parser(file);
        if (parser->parse()) {
            handle->type = ArchiveHandle::XP3;
            handle->parserInstance = parser;
        } else {
            delete parser;
            delete handle;
            handle = nullptr;
        }
    }

    return handle;
}

void ArchiveFactory::destroyParser(ArchiveHandle* handle) {
    if (!handle) return;
    
    if (handle->type == ArchiveHandle::BGI) {
        delete static_cast<BgiParser*>(handle->parserInstance);
    }
    else if (handle->type == ArchiveHandle::PFS) {
        delete static_cast<PfsParser*>(handle->parserInstance);
    }
    else if (handle->type == ArchiveHandle::ARC) {
        delete static_cast<ArcParser*>(handle->parserInstance);
    }
    else {
        delete static_cast<Xp3Parser*>(handle->parserInstance);
    }

    delete handle;
}

jobjectArray ArchiveFactory::getFileList(JNIEnv* env, const char* path) {
    std::vector<std::string> unifiedList;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    bool parseSuccess = false;

    if (isBgiArchive(path)) {
        FILE* file = fopen(path, "rb");
        if (file) {
            BgiParser parser(file);
            if (parser.parse()) {
                parseSuccess = true;
                for (const auto& entry : parser.get_file_list()) {
                    std::string utf8Name = StringUtils::sjisToUtf8(env, entry.name);
                    std::replace(utf8Name.begin(), utf8Name.end(), '\\', '/');
                    unifiedList.push_back(utf8Name + "|" + std::to_string(entry.size));
                }
            }
        }
    }
    else if (isPfsArchive(path)) {
        FILE* file = fopen(path, "rb");
        if (file) {
            PfsParser parser(file);
            if (parser.parse()) {
                parseSuccess = true;
                for (const auto& entry : parser.get_file_list()) {
                    std::string finalName;
                    if (StringUtils::isValidUtf8(entry.name)) {
                        finalName = entry.name;
                    } else {
                        finalName = StringUtils::sjisToUtf8(env, entry.name);
                    }
                    std::replace(finalName.begin(), finalName.end(), '\\', '/');
                    unifiedList.push_back(finalName + "|" + std::to_string(entry.size));
                }
            }
        }
    }
    else if (isWillArcArchive(path)) {
        FILE* file = fopen(path, "rb");
        if (file) {
            ArcParser parser(file);
            if (parser.parse()) {
                parseSuccess = true;
                for (const auto& entry : parser.get_file_list()) {
                    std::string finalName = entry.name;
                    std::replace(finalName.begin(), finalName.end(), '\\', '/');
                    unifiedList.push_back(finalName + "|" + std::to_string(entry.size));
                }
            }
        }
    }
    else {
        FILE* file = fopen(path, "rb");
        if (file) {
            Xp3Parser parser(file);
            if (parser.parse()) {
                parseSuccess = true;
                for (const auto& entry : parser.get_file_list()) {
                    std::string nameUtf8;
                    try { nameUtf8 = converter.to_bytes(entry.name); } catch(...) { nameUtf8 = "Error_Encoding"; }
                    std::replace(nameUtf8.begin(), nameUtf8.end(), '\\', '/');
                    unifiedList.push_back(nameUtf8 + "|" + std::to_string(entry.unpacked_size));
                }
            }
        }
    }

    if (!parseSuccess) return nullptr;

    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(unifiedList.size(), stringClass, env->NewStringUTF(""));

    for (size_t i = 0; i < unifiedList.size(); ++i) {
        jstring js = env->NewStringUTF(unifiedList[i].c_str());
        env->SetObjectArrayElement(result, i, js);
        env->DeleteLocalRef(js);
    }

    return result;
}

bool ArchiveFactory::extractFile(JNIEnv* env, ArchiveHandle* handle, const std::string& internalPath, const char* outputPath) {
    if (!handle) return false;
    bool success = false;

    if (handle->type == ArchiveHandle::BGI) {
        BgiParser* parser = static_cast<BgiParser*>(handle->parserInstance);
        std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
        success = parser->extractFile(sjisName, outputPath);
    }
    else if (handle->type == ArchiveHandle::PFS) {
        PfsParser* parser = static_cast<PfsParser*>(handle->parserInstance);
        if (parser->extractFile(internalPath, outputPath)) {
            success = true;
        } else {
            std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
            success = parser->extractFile(sjisName, outputPath);
        }
    }
    else if (handle->type == ArchiveHandle::ARC) {
        ArcParser* parser = static_cast<ArcParser*>(handle->parserInstance);
        success = parser->extractFile(internalPath, outputPath);
    }
    else {
        Xp3Parser* parser = static_cast<Xp3Parser*>(handle->parserInstance);
        std::vector<char> buffer;
        if (parser->extractToBuffer(internalPath, buffer)) {
            if (buffer.size() >= 5) {
                uint8_t m0 = (uint8_t)buffer[0];
                uint8_t m1 = (uint8_t)buffer[1];
                uint8_t m2 = (uint8_t)buffer[2];
                uint8_t m3 = (uint8_t)buffer[3];
                uint8_t m4 = (uint8_t)buffer[4];

                if (m0 == 0xFE && m1 == 0xFE && (m2 == 0x00 || m2 == 0x01 || m2 == 0x02) && m3 == 0xFF && m4 == 0xFE) {
                    std::vector<uint8_t> u8Buffer(buffer.begin(), buffer.end());
                    try {
                        u8Buffer = KirikiriDescrambler::Descramble(u8Buffer);
                        buffer.assign(u8Buffer.begin(), u8Buffer.end());
                    } catch (...) {}
                }
            }
            FILE* out = fopen(outputPath, "wb");
            if (out) {
                fwrite(buffer.data(), 1, buffer.size(), out);
                fclose(out);
                success = true;
            }
        }
    }
    return success;
}

bool ArchiveFactory::extractFile(JNIEnv* env, const char* archivePath, const std::string& internalPath, const char* outputPath) {
    bool success = false;
    FILE* file = fopen(archivePath, "rb");
    if (file) {
        if (isBgiArchive(archivePath)) {
            BgiParser parser(file);
            if (parser.parse()) {
                std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
                success = parser.extractFile(sjisName, outputPath);
            }
        }
        else if (isPfsArchive(archivePath)) {
            PfsParser parser(file);
            if (parser.parse()) {
                if (parser.extractFile(internalPath, outputPath)) {
                    success = true;
                } else {
                    std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
                    success = parser.extractFile(sjisName, outputPath);
                }
            }
        }
        else if (isWillArcArchive(archivePath)) {
            ArcParser parser(file);
            if (parser.parse()) success = parser.extractFile(internalPath, outputPath);
        }
        else {
            Xp3Parser parser(file);
            if (parser.parse()) {
                std::vector<char> buffer;
                if (parser.extractToBuffer(internalPath, buffer)) {
                    if (buffer.size() >= 5) {
                        uint8_t m0 = (uint8_t)buffer[0];
                        uint8_t m1 = (uint8_t)buffer[1];
                        uint8_t m2 = (uint8_t)buffer[2];
                        uint8_t m3 = (uint8_t)buffer[3];
                        uint8_t m4 = (uint8_t)buffer[4];

                        if (m0 == 0xFE && m1 == 0xFE && (m2 == 0x00 || m2 == 0x01 || m2 == 0x02) && m3 == 0xFF && m4 == 0xFE) {
                            std::vector<uint8_t> u8Buffer(buffer.begin(), buffer.end());
                            try {
                                u8Buffer = KirikiriDescrambler::Descramble(u8Buffer);
                                buffer.assign(u8Buffer.begin(), u8Buffer.end());
                            } catch (...) {}
                        }
                    }

                    FILE* out = fopen(outputPath, "wb");
                    if (out) {
                        fwrite(buffer.data(), 1, buffer.size(), out);
                        fclose(out);
                        success = true;
                    }
                }
            }
        }
        fclose(file);
    }
    return success;
}

std::vector<char> ArchiveFactory::getFileBuffer(JNIEnv* env, const char* archivePath, const std::string& internalPath) {
    std::vector<char> buffer;
    bool success = false;
    FILE* file = fopen(archivePath, "rb");

    if (file) {
        if (isBgiArchive(archivePath)) {
            BgiParser parser(file);
            if (parser.parse()) {
                std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
                success = parser.extractToBuffer(sjisName, buffer);
            }
        }
        else if (isPfsArchive(archivePath)) {
            PfsParser parser(file);
            if (parser.parse()) {
                if (parser.extractToBuffer(internalPath, buffer)) {
                    success = true;
                }
                else {
                    std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
                    if (parser.extractToBuffer(sjisName, buffer)) {
                        success = true;
                    }
                }
            }
        }
        else if (isWillArcArchive(archivePath)) {
            ArcParser parser(file);
            if (parser.parse()) {
                success = parser.extractToBuffer(internalPath, buffer);
            }
        }
        else {
            Xp3Parser parser(file);
            if (parser.parse()) success = parser.extractToBuffer(internalPath, buffer);
        }
        fclose(file);
    }

    if (success && !buffer.empty()) {
        if (buffer.size() >= 5) {
            uint8_t m0 = (uint8_t)buffer[0];
            uint8_t m1 = (uint8_t)buffer[1];
            uint8_t m2 = (uint8_t)buffer[2];
            uint8_t m3 = (uint8_t)buffer[3];
            uint8_t m4 = (uint8_t)buffer[4];
            if (m0 == 0xFE && m1 == 0xFE && (m2 == 0x00 || m2 == 0x01 || m2 == 0x02) && m3 == 0xFF && m4 == 0xFE) {
                std::vector<uint8_t> u8Buffer(buffer.begin(), buffer.end());
                try {
                    u8Buffer = KirikiriDescrambler::Descramble(u8Buffer);
                    buffer.assign(u8Buffer.begin(), u8Buffer.end());
                } catch (const std::exception& e) {
                    LOGE("Gagal descramble Kirikiri KS: %s", e.what());
                }
            }
        }
    } else {
        buffer.clear();
    }
    
    return buffer;
}

jintArray ArchiveFactory::getTlgPreview(JNIEnv* env, const char* archivePath, const std::string& internalPath) {
    std::vector<char> rawData;
    bool extractSuccess = false;
    FILE* file = fopen(archivePath, "rb");

    if (file) {
        if (isPfsArchive(archivePath)) {
            PfsParser parser(file);
            if (parser.parse()) {
                std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
                extractSuccess = parser.extractToBuffer(sjisName, rawData);
            }
        } else {
            Xp3Parser parser(file);
            if (parser.parse()) extractSuccess = parser.extractToBuffer(internalPath, rawData);
        }
        fclose(file);
    }

    jintArray result = nullptr;

    if (extractSuccess && !rawData.empty()) {
        std::vector<uint32_t> pixels;
        uint32_t width, height;

        if (TlgDecoder::decode(rawData, pixels, width, height)) {
            std::vector<int> combined(2 + pixels.size());
            combined[0] = (int)width;
            combined[1] = (int)height;
            memcpy(combined.data() + 2, pixels.data(), pixels.size() * sizeof(uint32_t));

            result = env->NewIntArray(combined.size());
            env->SetIntArrayRegion(result, 0, combined.size(), combined.data());
        }
    }
    return result;
}

jintArray ArchiveFactory::getBgiPreview(JNIEnv* env, const char* archivePath, const std::string& internalPath) {
    std::vector<char> rawData;
    bool extractSuccess = false;
    FILE* file = fopen(archivePath, "rb");

    if (file) {
        if (isBgiArchive(archivePath)) {
            BgiParser parser(file);
            if (parser.parse()) {
                std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
                extractSuccess = parser.extractToBuffer(sjisName, rawData);
            }
        }
        else if (isPfsArchive(archivePath)) {
            PfsParser parser(file);
            if (parser.parse()) {
                std::string sjisName = StringUtils::utf8ToSjis(env, internalPath);
                extractSuccess = parser.extractToBuffer(sjisName, rawData);
            }
        }
        else {
            Xp3Parser parser(file);
            if (parser.parse()) extractSuccess = parser.extractToBuffer(internalPath, rawData);
        }
        fclose(file);
    }

    jintArray result = nullptr;

    if (extractSuccess && !rawData.empty()) {
        std::vector<uint32_t> pixels;
        uint32_t width, height;

        if (BgiImageDecoder::decode(rawData, pixels, width, height)) {
            std::vector<int> combined(2 + pixels.size());
            combined[0] = (int)width;
            combined[1] = (int)height;
            memcpy(combined.data() + 2, pixels.data(), pixels.size() * sizeof(uint32_t));

            result = env->NewIntArray(combined.size());
            env->SetIntArrayRegion(result, 0, combined.size(), combined.data());
        }
    }
    return result;
}
