#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>
#include <codecvt>
#include <locale>
#include <algorithm>
#include <cstring>


#include "formats/Xp3Parser.h"
#include "formats/PfsParser.h"
#include "formats/Xp3Packer.h"
#include "formats/BgiParser.h"
#include "formats/PfsPacker.h"
#include "formats/ArcParser.h"
#include "utils/TlgDecoder.h"
#include "utils/BgiImageDecoder.h"
#include "utils/AstParser.h"
#include "utils/PSBFILE/ScnParser.h"
#include "utils/KirikiriKS.h"
#include "utils/AstPacker.h"

#define TAG "GardroidNative"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Convert Raw Shift-JIS (from Archive) -> UTF-8 (for Java String)
std::string sjisToUtf8(JNIEnv* env, const std::string& sjisData) {
    if (sjisData.empty()) return "";

    jbyteArray bytes = env->NewByteArray(sjisData.length());
    env->SetByteArrayRegion(bytes, 0, sjisData.length(), (const jbyte*)sjisData.data());

    jstring encoding = env->NewStringUTF("Shift_JIS");
    jclass strClass = env->FindClass("java/lang/String");
    jmethodID ctor = env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");

    jstring javaString = (jstring)env->NewObject(strClass, ctor, bytes, encoding);

    const char* utf8Chars = env->GetStringUTFChars(javaString, nullptr);
    std::string result(utf8Chars);

    env->ReleaseStringUTFChars(javaString, utf8Chars);
    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(encoding);
    env->DeleteLocalRef(javaString);
    env->DeleteLocalRef(strClass);

    return result;
}

std::string utf8ToSjis(JNIEnv* env, const std::string& utf8Data) {
    if (utf8Data.empty()) return "";

    jstring javaString = env->NewStringUTF(utf8Data.c_str());
    jclass strClass = env->FindClass("java/lang/String");
    jmethodID getBytes = env->GetMethodID(strClass, "getBytes", "(Ljava/lang/String;)[B");
    jstring encoding = env->NewStringUTF("Shift_JIS");

    jbyteArray bytes = (jbyteArray)env->CallObjectMethod(javaString, getBytes, encoding);

    jsize length = env->GetArrayLength(bytes);
    std::vector<char> buffer(length);
    env->GetByteArrayRegion(bytes, 0, length, (jbyte*)buffer.data());

    std::string result(buffer.begin(), buffer.end());

    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(encoding);
    env->DeleteLocalRef(javaString);
    env->DeleteLocalRef(strClass);

    return result;
}

bool isValidUtf8(const std::string& string) {
    const unsigned char* bytes = (const unsigned char*)string.c_str();
    while (*bytes) {
        if ((// ASCII
                *bytes == 0x09 ||
                *bytes == 0x0A ||
                *bytes == 0x0D ||
                (*bytes >= 0x20 && *bytes <= 0x7E)
        )
                ) {
            bytes += 1;
            continue;
        }

        if (// Non-overlong 2-byte
                (*bytes >= 0xC2 && *bytes <= 0xDF) &&
                (*(bytes+1) >= 0x80 && *(bytes+1) <= 0xBF)
                ) {
            bytes += 2;
            continue;
        }

        if (// Excluding overlongs
                *bytes == 0xE0 &&
                (*(bytes+1) >= 0xA0 && *(bytes+1) <= 0xBF) &&
                (*(bytes+2) >= 0x80 && *(bytes+2) <= 0xBF)
                ) {
            bytes += 3;
            continue;
        }

        if (// Straight 3-byte
                (((*bytes >= 0xE1 && *bytes <= 0xEC) ||
                  *bytes == 0xEE ||
                  *bytes == 0xEF) &&
                 (*(bytes+1) >= 0x80 && *(bytes+1) <= 0xBF) &&
                 (*(bytes+2) >= 0x80 && *(bytes+2) <= 0xBF)
                )
                ) {
            bytes += 3;
            continue;
        }

        if (// Excluding overlongs
                *bytes == 0xED &&
                (*(bytes+1) >= 0x80 && *(bytes+1) <= 0x9F) &&
                (*(bytes+2) >= 0x80 && *(bytes+2) <= 0xBF)
                ) {
            bytes += 3;
            continue;
        }

        if (// Planes 1-3
                *bytes == 0xF0 &&
                (*(bytes+1) >= 0x90 && *(bytes+1) <= 0xBF) &&
                (*(bytes+2) >= 0x80 && *(bytes+2) <= 0xBF) &&
                (*(bytes+3) >= 0x80 && *(bytes+3) <= 0xBF)
                ) {
            bytes += 4;
            continue;
        }

        if (// Planes 4-15
                (*bytes >= 0xF1 && *bytes <= 0xF3) &&
                (*(bytes+1) >= 0x80 && *(bytes+1) <= 0xBF) &&
                (*(bytes+2) >= 0x80 && *(bytes+2) <= 0xBF) &&
                (*(bytes+3) >= 0x80 && *(bytes+3) <= 0xBF)
                ) {
            bytes += 4;
            continue;
        }

        if (// Plane 16
                *bytes == 0xF4 &&
                (*(bytes+1) >= 0x80 && *(bytes+1) <= 0x8F) &&
                (*(bytes+2) >= 0x80 && *(bytes+2) <= 0xBF) &&
                (*(bytes+3) >= 0x80 && *(bytes+3) <= 0xBF)
                ) {
            bytes += 4;
            continue;
        }

        return false; // Invalid UTF-8 detected
    }
    return true;
}

bool isPfsArchive(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char sig[2];
    if (fread(sig, 1, 2, f) != 2) { fclose(f); return false; }
    fclose(f);
    return sig[0] == 'p' && sig[1] == 'f';
}

bool isBgiArchive(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char sig[13];
    if (fread(sig, 1, 12, f) != 12) { fclose(f); return false; }
    fclose(f);
    if (memcmp(sig, "PackFile    ", 12) == 0) return true;
    if (memcmp(sig, "BURIKO ARC20", 12) == 0) return true;
    return false;
}

bool isWillArcArchive(const char* path) {
    if (isBgiArchive(path)) return false; // Pastikan bukan BGI

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint32_t count = 0, index_size = 0;
    if (fread(&count, 1, 4, f) == 4 && fread(&index_size, 1, 4, f) == 4) {
        fclose(f);
        // Validasi sederhana (karena tidak ada signature teks)
        return count > 0 && count < 200000 && index_size > 0;
    }
    fclose(f);
    return false;
}

struct ArchiveHandle {
    enum Type { XP3, PFS, BGI, ARC };
    Type type;
    void* parserInstance;
};

// --- JNI IMPLEMENTATION ---

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getArchiveFileList(
        JNIEnv* env,
        jobject /* this */,
        jstring filePath) {

    const char *nativePath = env->GetStringUTFChars(filePath, 0);
    std::vector<std::string> unifiedList;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    bool parseSuccess = false;

    if (isBgiArchive(nativePath)) {
        FILE* file = fopen(nativePath, "rb");
        if (file) {
            BgiParser parser(file);
            if (parser.parse()) {
                parseSuccess = true;
                for (const auto& entry : parser.get_file_list()) {
                    std::string utf8Name = sjisToUtf8(env, entry.name);
                    std::replace(utf8Name.begin(), utf8Name.end(), '\\', '/');
                    unifiedList.push_back(utf8Name + "|" + std::to_string(entry.size));
                }
            }
        }
    }
    else if (isPfsArchive(nativePath)) {
        FILE* file = fopen(nativePath, "rb");
        if (file) {
            PfsParser parser(file);
            if (parser.parse()) {
                parseSuccess = true;
                for (const auto& entry : parser.get_file_list()) {
                    std::string finalName;

                    // PERBAIKAN: Cek dulu apakah nama file sudah UTF-8?
                    if (isValidUtf8(entry.name)) {
                        finalName = entry.name; // Gunakan langsung
                    } else {
                        finalName = sjisToUtf8(env, entry.name); // Convert jika SJIS
                    }

                    std::replace(finalName.begin(), finalName.end(), '\\', '/');
                    unifiedList.push_back(finalName + "|" + std::to_string(entry.size));
                }
            }
        }
    }
    else if (isWillArcArchive(nativePath)) {
        FILE* file = fopen(nativePath, "rb");
        if (file) {
            ArcParser parser(file);
            if (parser.parse()) {
                parseSuccess = true;
                for (const auto& entry : parser.get_file_list()) {
                    std::string finalName = entry.name; // ArcParser kita sudah output UTF-8
                    std::replace(finalName.begin(), finalName.end(), '\\', '/');
                    unifiedList.push_back(finalName + "|" + std::to_string(entry.size));
                }
            }
        }
    }
    else {
        FILE* file = fopen(nativePath, "rb");
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

    env->ReleaseStringUTFChars(filePath, nativePath);

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

extern "C" JNIEXPORT jboolean JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_extractFile(
        JNIEnv* env,
        jobject /* this */,
        jstring archivePath,
        jstring internalPath,
        jstring outputPath) {

    const char *cArchive = env->GetStringUTFChars(archivePath, 0);
    const char *cOutput = env->GetStringUTFChars(outputPath, 0);

    std::string pathStr = "";

    const char *cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    bool success = false;
    FILE* file = fopen(cArchive, "rb");
    if (file) {
        if (isBgiArchive(cArchive)) {
            BgiParser parser(file);
            if (parser.parse()) {
                std::string sjisName = utf8ToSjis(env, utf8Internal);
                success = parser.extractFile(sjisName, cOutput);
            }
        }
        else if (isPfsArchive(cArchive)) {
            PfsParser parser(file);
            if (parser.parse()) {
                // --- PERBAIKAN LOGIKA PENCARIAN (FALLBACK) ---
                if (parser.extractFile(utf8Internal, cOutput)) {
                    success = true;
                } else {
                    std::string sjisName = utf8ToSjis(env, utf8Internal);
                    success = parser.extractFile(sjisName, cOutput);
                }
                // ---------------------------------------------
            }
        }
        else if (isWillArcArchive(cArchive)) {
            ArcParser parser(file);
            if (parser.parse()) success = parser.extractFile(utf8Internal, cOutput);
        }
        else {
            Xp3Parser parser(file);
            if (parser.parse()) {
                std::vector<char> buffer;
                if (parser.extractToBuffer(utf8Internal, buffer)) {

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

                    FILE* out = fopen(cOutput, "wb");
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

    env->ReleaseStringUTFChars(archivePath, cArchive);
    env->ReleaseStringUTFChars(outputPath, cOutput);

    return success;
}


extern "C" JNIEXPORT jlong JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_initParser(
        JNIEnv* env,
        jobject /* this */,
        jstring archivePath) {

    const char *path = env->GetStringUTFChars(archivePath, 0);
    FILE* file = fopen(path, "rb");

    if (!file) {
        env->ReleaseStringUTFChars(archivePath, path);
        return 0;
    }

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

    env->ReleaseStringUTFChars(archivePath, path);
    return reinterpret_cast<jlong>(handle);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_extractFileFromPointer(
        JNIEnv* env,
        jobject /* this */,
        jlong parserPointer,
        jstring internalPath,
        jstring outputPath) {

    if (parserPointer == 0) return false;

    ArchiveHandle* handle = reinterpret_cast<ArchiveHandle*>(parserPointer);
    const char *cOutput = env->GetStringUTFChars(outputPath, 0);

    const char *cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    bool success = false;

    if (handle->type == ArchiveHandle::BGI) {
        BgiParser* parser = static_cast<BgiParser*>(handle->parserInstance);
        std::string sjisName = utf8ToSjis(env, utf8Internal);
        success = parser->extractFile(sjisName, cOutput);
    }
    else if (handle->type == ArchiveHandle::PFS) {
        PfsParser* parser = static_cast<PfsParser*>(handle->parserInstance);
        if (parser->extractFile(utf8Internal, cOutput)) {
            success = true;
        } else {
            std::string sjisName = utf8ToSjis(env, utf8Internal);
            success = parser->extractFile(sjisName, cOutput);
        }
    }
    else if (handle->type == ArchiveHandle::ARC) {
        ArcParser* parser = static_cast<ArcParser*>(handle->parserInstance);
        success = parser->extractFile(utf8Internal, cOutput);
    }
    else {
        Xp3Parser* parser = static_cast<Xp3Parser*>(handle->parserInstance);
        std::vector<char> buffer;
        if (parser->extractToBuffer(utf8Internal, buffer)) {
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
                    } catch (...) {
                    }
                }
            }

            FILE* out = fopen(cOutput, "wb");
            if (out) {
                fwrite(buffer.data(), 1, buffer.size(), out);
                fclose(out);
                success = true;
            }
        }
    }

    env->ReleaseStringUTFChars(outputPath, cOutput);

    return success;
}

extern "C" JNIEXPORT void JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_closeParser(
        JNIEnv* env,
        jobject /* this */,
        jlong parserPointer) {

    if (parserPointer != 0) {
        ArchiveHandle* handle = reinterpret_cast<ArchiveHandle*>(parserPointer);

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
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getFileBuffer(
        JNIEnv* env,
        jobject /* this */,
        jstring archivePath,
        jstring internalPath) {

    const char *cArchive = env->GetStringUTFChars(archivePath, 0);

    const char *cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    std::vector<char> buffer;
    bool success = false;
    FILE* file = fopen(cArchive, "rb");

    if (file) {
        if (isBgiArchive(cArchive)) {
            BgiParser parser(file);
            if (parser.parse()) {
                std::string sjisName = utf8ToSjis(env, utf8Internal);
                success = parser.extractToBuffer(sjisName, buffer);
            }
        }
        else if (isPfsArchive(cArchive)) {
            PfsParser parser(file);
            if (parser.parse()) {
                // --- PERBAIKAN LOGIKA PENCARIAN (FALLBACK) ---
                // 1. Coba cari file menggunakan nama UTF-8 (Raw dari Java)
                if (parser.extractToBuffer(utf8Internal, buffer)) {
                    success = true;
                }
                    // 2. Jika gagal, convert ke Shift-JIS dan cari lagi
                else {
                    std::string sjisName = utf8ToSjis(env, utf8Internal);
                    if (parser.extractToBuffer(sjisName, buffer)) {
                        success = true;
                    }
                }
                // ---------------------------------------------
            }
        }
        else if (isWillArcArchive(cArchive)) {
            ArcParser parser(file);
            if (parser.parse()) {
                success = parser.extractToBuffer(utf8Internal, buffer);
            }
        }
        else {
            Xp3Parser parser(file);
            if (parser.parse()) success = parser.extractToBuffer(utf8Internal, buffer);
        }
        fclose(file); // Pastikan file ditutup
    }

    env->ReleaseStringUTFChars(archivePath, cArchive);

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

        jbyteArray result = env->NewByteArray(buffer.size());
        env->SetByteArrayRegion(result, 0, buffer.size(), (const jbyte*)buffer.data());
        return result;
    }
    return nullptr;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getTlgPreview(
        JNIEnv* env,
        jobject /* this */,
        jstring archivePath,
        jstring internalPath) {

    const char *cArchive = env->GetStringUTFChars(archivePath, 0);
    const char *cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    std::vector<char> rawData;
    bool extractSuccess = false;
    FILE* file = fopen(cArchive, "rb");

    if (file) {
        if (isPfsArchive(cArchive)) {
            PfsParser parser(file);
            if (parser.parse()) {
                std::string sjisName = utf8ToSjis(env, utf8Internal);
                extractSuccess = parser.extractToBuffer(sjisName, rawData);
            }
        } else {
            Xp3Parser parser(file);
            if (parser.parse()) extractSuccess = parser.extractToBuffer(utf8Internal, rawData);
        }
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

    env->ReleaseStringUTFChars(archivePath, cArchive);
    return result;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getBgiPreview(
        JNIEnv* env,
        jobject /* this */,
        jstring archivePath,
        jstring internalPath) {

    const char *cArchive = env->GetStringUTFChars(archivePath, 0);
    const char *cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    std::vector<char> rawData;
    bool extractSuccess = false;
    FILE* file = fopen(cArchive, "rb");

    if (file) {
        if (isBgiArchive(cArchive)) {
            BgiParser parser(file);
            if (parser.parse()) {
                std::string sjisName = utf8ToSjis(env, utf8Internal);
                extractSuccess = parser.extractToBuffer(sjisName, rawData);
            }
        }
        else if (isPfsArchive(cArchive)) {
            PfsParser parser(file);
            if (parser.parse()) {
                std::string sjisName = utf8ToSjis(env, utf8Internal);
                extractSuccess = parser.extractToBuffer(sjisName, rawData);
            }
        }
        else {
            Xp3Parser parser(file);
            if (parser.parse()) extractSuccess = parser.extractToBuffer(utf8Internal, rawData);
        }
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

    env->ReleaseStringUTFChars(archivePath, cArchive);
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_repackXp3(
        JNIEnv* env,
        jobject /* this */,
        jstring sourceFolder,
        jstring outputFile,
        jobject listener) {

    const char *cSource = env->GetStringUTFChars(sourceFolder, 0);
    const char *cOutput = env->GetStringUTFChars(outputFile, 0);

    jclass listenerClass = env->GetObjectClass(listener);
    jmethodID onProgressMethod = env->GetMethodID(listenerClass, "onProgress", "(Ljava/lang/String;II)V");

    auto progressCallback = [&](const std::string& filename, int current, int total) {
        jstring jFilename = env->NewStringUTF(filename.c_str());
        env->CallVoidMethod(listener, onProgressMethod, jFilename, current, total);
        env->DeleteLocalRef(jFilename);
    };

    bool success = Xp3Packer::pack(cSource, cOutput, progressCallback);

    env->ReleaseStringUTFChars(sourceFolder, cSource);
    env->ReleaseStringUTFChars(outputFile, cOutput);

    return success;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_repackPfs(
        JNIEnv* env,
        jobject /* this */,
        jstring sourceFolder,
        jstring outputFile,
        jobject listener) {

    const char *cSource = env->GetStringUTFChars(sourceFolder, 0);
    const char *cOutput = env->GetStringUTFChars(outputFile, 0);

    jclass listenerClass = env->GetObjectClass(listener);
    jmethodID onProgressMethod = env->GetMethodID(listenerClass, "onProgress", "(Ljava/lang/String;II)V");

    auto progressCallback = [&](const std::string& filename, int current, int total) {
        jstring jFilename = env->NewStringUTF(filename.c_str());
        env->CallVoidMethod(listener, onProgressMethod, jFilename, current, total);
        env->DeleteLocalRef(jFilename);
    };

    bool success = PfsPacker::pack(cSource, cOutput, progressCallback);

    env->ReleaseStringUTFChars(sourceFolder, cSource);
    env->ReleaseStringUTFChars(outputFile, cOutput);

    return success;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_detectAstLanguages(
        JNIEnv* env,
        jobject /* this */,
        jstring filePath) {

    const char *cPath = env->GetStringUTFChars(filePath, 0);
    std::vector<std::string> langs = AstParser::detectLanguages(cPath);
    env->ReleaseStringUTFChars(filePath, cPath);

    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(langs.size(), stringClass, env->NewStringUTF(""));

    for (size_t i = 0; i < langs.size(); ++i) {
        jstring js = env->NewStringUTF(langs[i].c_str());
        env->SetObjectArrayElement(result, i, js);
        env->DeleteLocalRef(js);
    }
    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_extractAstText(
        JNIEnv* env,
        jobject /* this */,
        jstring inputPath,
        jstring outputPath,
        jstring language) {

    const char *cInput = env->GetStringUTFChars(inputPath, 0);
    const char *cOutput = env->GetStringUTFChars(outputPath, 0);
    const char *cLang = env->GetStringUTFChars(language, 0);

    int lines = AstParser::extractText(cInput, cOutput, cLang);

    env->ReleaseStringUTFChars(inputPath, cInput);
    env->ReleaseStringUTFChars(outputPath, cOutput);
    env->ReleaseStringUTFChars(language, cLang);

    return lines;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_extractScnText(
        JNIEnv* env,
        jobject /* this */,
        jstring inputPath,
        jstring outputPath) {

    const char *cInput = env->GetStringUTFChars(inputPath, 0);
    const char *cOutput = env->GetStringUTFChars(outputPath, 0);

    int result = ScnParser::extractText(cInput, cOutput);

    env->ReleaseStringUTFChars(inputPath, cInput);
    env->ReleaseStringUTFChars(outputPath, cOutput);

    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_repackAst(
        JNIEnv* env, jobject /* this */,
        jstring astPath, jstring txtPath, jstring outPath, jstring targetLang) {

    const char* cAstPath = env->GetStringUTFChars(astPath, nullptr);
    const char* cTxtPath = env->GetStringUTFChars(txtPath, nullptr);
    const char* cOutPath = env->GetStringUTFChars(outPath, nullptr);
    const char* cLang = env->GetStringUTFChars(targetLang, nullptr);

    std::string sAst(cAstPath);
    std::string sTxt(cTxtPath);
    std::string sOut(cOutPath);
    std::string sLang(cLang);
    env->ReleaseStringUTFChars(astPath, cAstPath);
    env->ReleaseStringUTFChars(txtPath, cTxtPath);
    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(targetLang, cLang);

    return AstPacker::repack(sAst, sTxt, sOut, sLang);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_extractKsText(
        JNIEnv* env, jobject /* this */, jstring inputPath, jstring outputPath) {

    const char *cInput = env->GetStringUTFChars(inputPath, 0);
    const char *cOutput = env->GetStringUTFChars(outputPath, 0);

    int lines = KirikiriParser::ExtractTextToFile(cInput, cOutput);

    env->ReleaseStringUTFChars(inputPath, cInput);
    env->ReleaseStringUTFChars(outputPath, cOutput);

    return lines;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_repackKsText(
        JNIEnv* env, jobject /* this */,
        jstring ksPath, jstring txtPath, jstring outPath) {

    const char* cKs = env->GetStringUTFChars(ksPath, nullptr);
    const char* cTxt = env->GetStringUTFChars(txtPath, nullptr);
    const char* cOut = env->GetStringUTFChars(outPath, nullptr);

    int result = KirikiriParser::RepackText(cKs, cTxt, cOut);

    env->ReleaseStringUTFChars(ksPath, cKs);
    env->ReleaseStringUTFChars(txtPath, cTxt);
    env->ReleaseStringUTFChars(outPath, cOut);

    return result;
}