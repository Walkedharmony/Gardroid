#include <jni.h>
#include <string>
#include <vector>
#include "../utils/AstParser.h"
#include "../utils/PSBFILE/ScnParser.h"
#include "../utils/KirikiriKS.h"
#include "../utils/AstPacker.h"

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_detectAstLanguages(
        JNIEnv* env, jobject, jstring filePath) {
    const char* cPath = env->GetStringUTFChars(filePath, 0);
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
        JNIEnv* env, jobject, jstring inputPath, jstring outputPath, jstring language) {
    const char* cInput = env->GetStringUTFChars(inputPath, 0);
    const char* cOutput = env->GetStringUTFChars(outputPath, 0);
    const char* cLang = env->GetStringUTFChars(language, 0);

    int lines = AstParser::extractText(cInput, cOutput, cLang);

    env->ReleaseStringUTFChars(inputPath, cInput);
    env->ReleaseStringUTFChars(outputPath, cOutput);
    env->ReleaseStringUTFChars(language, cLang);

    return lines;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_extractScnText(
        JNIEnv* env, jobject, jstring inputPath, jstring outputPath) {
    const char* cInput = env->GetStringUTFChars(inputPath, 0);
    const char* cOutput = env->GetStringUTFChars(outputPath, 0);

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
