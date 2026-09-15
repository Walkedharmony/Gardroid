#include <jni.h>
#include <string>
#include "ArchiveFactory.h"
#include "../formats/Xp3Packer.h"
#include "../formats/PfsPacker.h"
#include "../utils/TlgDecoder.h"

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getArchiveFileList(JNIEnv* env, jobject, jstring filePath) {
    const char* nativePath = env->GetStringUTFChars(filePath, 0);
    jobjectArray result = ArchiveFactory::getFileList(env, nativePath);
    env->ReleaseStringUTFChars(filePath, nativePath);
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_extractFile(
        JNIEnv* env, jobject, jstring archivePath, jstring internalPath, jstring outputPath) {
    const char* cArchive = env->GetStringUTFChars(archivePath, 0);
    const char* cOutput = env->GetStringUTFChars(outputPath, 0);
    
    const char* cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    bool success = ArchiveFactory::extractFile(env, cArchive, utf8Internal, cOutput);

    env->ReleaseStringUTFChars(archivePath, cArchive);
    env->ReleaseStringUTFChars(outputPath, cOutput);
    return success;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_initParser(JNIEnv* env, jobject, jstring archivePath) {
    const char* path = env->GetStringUTFChars(archivePath, 0);
    ArchiveHandle* handle = ArchiveFactory::createParser(path);
    env->ReleaseStringUTFChars(archivePath, path);
    return reinterpret_cast<jlong>(handle);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_extractFileFromPointer(
        JNIEnv* env, jobject, jlong parserPointer, jstring internalPath, jstring outputPath) {
    ArchiveHandle* handle = reinterpret_cast<ArchiveHandle*>(parserPointer);
    const char* cOutput = env->GetStringUTFChars(outputPath, 0);
    
    const char* cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    bool success = ArchiveFactory::extractFile(env, handle, utf8Internal, cOutput);

    env->ReleaseStringUTFChars(outputPath, cOutput);
    return success;
}

extern "C" JNIEXPORT void JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_closeParser(JNIEnv* env, jobject, jlong parserPointer) {
    ArchiveHandle* handle = reinterpret_cast<ArchiveHandle*>(parserPointer);
    ArchiveFactory::destroyParser(handle);
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getFileBuffer(
        JNIEnv* env, jobject, jstring archivePath, jstring internalPath) {
    const char* cArchive = env->GetStringUTFChars(archivePath, 0);
    
    const char* cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    std::vector<char> buffer = ArchiveFactory::getFileBuffer(env, cArchive, utf8Internal);

    env->ReleaseStringUTFChars(archivePath, cArchive);

    if (!buffer.empty()) {
        jbyteArray result = env->NewByteArray(buffer.size());
        env->SetByteArrayRegion(result, 0, buffer.size(), (const jbyte*)buffer.data());
        return result;
    }
    return nullptr;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getTlgPreview(
        JNIEnv* env, jobject, jstring archivePath, jstring internalPath) {
    const char* cArchive = env->GetStringUTFChars(archivePath, 0);
    
    const char* cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    jintArray result = ArchiveFactory::getTlgPreview(env, cArchive, utf8Internal);

    env->ReleaseStringUTFChars(archivePath, cArchive);
    return result;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getTlgPreviewOffline(
        JNIEnv* env, jobject, jstring filePath) {
    const char* cFilePath = env->GetStringUTFChars(filePath, 0);
    
    std::vector<char> rawData;
    FILE* file = fopen(cFilePath, "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        fseek(file, 0, SEEK_SET);
        rawData.resize(size);
        fread(rawData.data(), 1, size, file);
        fclose(file);
    }
    
    jintArray result = nullptr;
    if (!rawData.empty()) {
        std::vector<uint32_t> pixels;
        uint32_t width, height;
        if (TlgDecoder::decode(rawData, pixels, width, height)) {
            size_t pixelCount = pixels.size();
            result = env->NewIntArray(pixelCount + 2);
            if (result) {
                std::vector<jint> finalData(pixelCount + 2);
                finalData[0] = width;
                finalData[1] = height;
                for (size_t i = 0; i < pixelCount; i++) {
                    finalData[i + 2] = static_cast<jint>(pixels[i]);
                }
                env->SetIntArrayRegion(result, 0, pixelCount + 2, finalData.data());
            }
        }
    }

    env->ReleaseStringUTFChars(filePath, cFilePath);
    return result;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_getBgiPreview(
        JNIEnv* env, jobject, jstring archivePath, jstring internalPath) {
    const char* cArchive = env->GetStringUTFChars(archivePath, 0);
    
    const char* cInternalRaw = env->GetStringUTFChars(internalPath, 0);
    std::string utf8Internal(cInternalRaw);
    env->ReleaseStringUTFChars(internalPath, cInternalRaw);

    jintArray result = ArchiveFactory::getBgiPreview(env, cArchive, utf8Internal);

    env->ReleaseStringUTFChars(archivePath, cArchive);
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_repackXp3(
        JNIEnv* env, jobject, jstring sourceFolder, jstring outputFile, jobject listener) {
    const char* cSource = env->GetStringUTFChars(sourceFolder, 0);
    const char* cOutput = env->GetStringUTFChars(outputFile, 0);

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
        JNIEnv* env, jobject, jstring sourceFolder, jstring outputFile, jobject listener) {
    const char* cSource = env->GetStringUTFChars(sourceFolder, 0);
    const char* cOutput = env->GetStringUTFChars(outputFile, 0);

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
