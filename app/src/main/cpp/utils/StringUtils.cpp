#include "StringUtils.h"
#include <vector>

namespace StringUtils {

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

} // namespace StringUtils
