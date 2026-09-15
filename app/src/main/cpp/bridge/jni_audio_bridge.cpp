#include <jni.h>
#include <string>
#include <memory>
#include "../utils/AudioDecoder.h"

extern "C" JNIEXPORT jlong JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_nativeOpenAudio(JNIEnv* env, jobject /* this */, jstring path) {
    const char* nativePath = env->GetStringUTFChars(path, nullptr);
    
    AudioDecoder* decoder = new AudioDecoder();
    if (decoder->openFile(nativePath)) {
        env->ReleaseStringUTFChars(path, nativePath);
        return reinterpret_cast<jlong>(decoder);
    }
    
    delete decoder;
    env->ReleaseStringUTFChars(path, nativePath);
    return 0;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_nativeGetAudioInfo(JNIEnv* env, jobject /* this */, jlong handle) {
    AudioDecoder* decoder = reinterpret_cast<AudioDecoder*>(handle);
    if (!decoder) return nullptr;
    
    jclass infoClass = env->FindClass("com/zeronovel/gardroid/bridge/NativeAudioInfo");
    if (!infoClass) return nullptr;
    
    jmethodID constructor = env->GetMethodID(infoClass, "<init>", "(IIJDILjava/lang/String;)V");
    if (!constructor) return nullptr;
    
    jstring codecStr = env->NewStringUTF(decoder->getCodecName());
    
    jobject infoObj = env->NewObject(infoClass, constructor, 
        decoder->getChannels(),
        decoder->getSampleRate(),
        decoder->getTotalSamples(),
        decoder->getDurationSeconds(),
        decoder->getFormat(),
        codecStr);
        
    return infoObj;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_nativeDecodeAudio(JNIEnv* env, jobject /* this */, jlong handle, jshortArray buffer, jint maxSamples) {
    AudioDecoder* decoder = reinterpret_cast<AudioDecoder*>(handle);
    if (!decoder) return -1;
    
    jshort* nativeBuffer = env->GetShortArrayElements(buffer, nullptr);
    int samplesRead = decoder->decode(nativeBuffer, maxSamples);
    
    env->ReleaseShortArrayElements(buffer, nativeBuffer, 0);
    return samplesRead;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_nativeSeekAudio(JNIEnv* env, jobject /* this */, jlong handle, jlong sampleOffset) {
    AudioDecoder* decoder = reinterpret_cast<AudioDecoder*>(handle);
    if (!decoder) return false;
    
    return decoder->seek(sampleOffset);
}

extern "C" JNIEXPORT void JNICALL
Java_com_zeronovel_gardroid_bridge_NativeLib_nativeCloseAudio(JNIEnv* env, jobject /* this */, jlong handle) {
    AudioDecoder* decoder = reinterpret_cast<AudioDecoder*>(handle);
    if (decoder) {
        delete decoder;
    }
}
