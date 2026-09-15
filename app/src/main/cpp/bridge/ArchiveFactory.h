#ifndef ARCHIVEFACTORY_H
#define ARCHIVEFACTORY_H

#include <jni.h>
#include <string>
#include <vector>

struct ArchiveHandle {
    enum Type { XP3, PFS, BGI, ARC };
    Type type;
    void* parserInstance;
};

class ArchiveFactory {
public:
    static bool isPfsArchive(const char* path);
    static bool isBgiArchive(const char* path);
    static bool isWillArcArchive(const char* path);

    static ArchiveHandle* createParser(const char* path);
    static void destroyParser(ArchiveHandle* handle);

    static jobjectArray getFileList(JNIEnv* env, const char* path);
    
    // Extract file using an existing parser handle
    static bool extractFile(JNIEnv* env, ArchiveHandle* handle, const std::string& internalPath, const char* outputPath);
    
    // Extract file by creating a temporary parser
    static bool extractFile(JNIEnv* env, const char* archivePath, const std::string& internalPath, const char* outputPath);
    
    // Get file buffer by creating a temporary parser
    static std::vector<char> getFileBuffer(JNIEnv* env, const char* archivePath, const std::string& internalPath);
    
    // Get preview image pixels (Tlg)
    static jintArray getTlgPreview(JNIEnv* env, const char* archivePath, const std::string& internalPath);
    
    // Get preview image pixels (Bgi)
    static jintArray getBgiPreview(JNIEnv* env, const char* archivePath, const std::string& internalPath);
};

#endif // ARCHIVEFACTORY_H
