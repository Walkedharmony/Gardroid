package com.zeronovel.gardroid.bridge;

public class NativeAudioInfo {
    public int channels;
    public int sampleRate;
    public long totalSamples;
    public double durationSeconds;
    public int format; // 0=Unknown, 1=OGG_VORBIS, 2=OPUS
    public String codecName;

    public NativeAudioInfo(int channels, int sampleRate, long totalSamples, double durationSeconds, int format, String codecName) {
        this.channels = channels;
        this.sampleRate = sampleRate;
        this.totalSamples = totalSamples;
        this.durationSeconds = durationSeconds;
        this.format = format;
        this.codecName = codecName;
    }
}
