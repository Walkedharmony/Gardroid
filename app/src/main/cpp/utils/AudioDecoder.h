#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <jni.h>
#include <android/log.h>

class AudioDecoder {
public:
    enum Format {
        UNKNOWN,
        OGG_VORBIS,
        OPUS
    };

    AudioDecoder();
    ~AudioDecoder();

    bool openFile(const std::string& path);

    Format getFormat() const { return format; }
    int getChannels() const { return channels; }
    int getSampleRate() const { return sampleRate; }
    long long getTotalSamples() const { return totalSamples; }
    double getDurationSeconds() const;

    int decode(short* pcm_out, int max_samples_per_channel);
    bool seek(long long sample_offset);
    void close();
    const char* getCodecName() const;

private:
    Format format = UNKNOWN;
    int channels = 0;
    int sampleRate = 0;
    long long totalSamples = 0;

    void* vorbisHandle = nullptr;
    void* opusHandle = nullptr;

    std::mutex decodeMutex;
};
