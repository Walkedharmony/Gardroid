#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <jni.h>
#include <string>

namespace StringUtils {
    std::string sjisToUtf8(JNIEnv* env, const std::string& sjisData);
    std::string utf8ToSjis(JNIEnv* env, const std::string& utf8Data);
    bool isValidUtf8(const std::string& string);
}

#endif // STRINGUTILS_H
