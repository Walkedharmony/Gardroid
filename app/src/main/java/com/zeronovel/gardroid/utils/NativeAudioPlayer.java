package com.zeronovel.gardroid.utils;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Handler;
import android.os.Looper;

import com.zeronovel.gardroid.bridge.NativeAudioInfo;
import com.zeronovel.gardroid.bridge.NativeLib;

public class NativeAudioPlayer {

    public interface PlaybackListener {
        void onProgress(long currentSample, long totalSamples, double currentSeconds, double totalSeconds);

        void onCompletion();

        void onError(String message);
    }

    private final NativeLib nativeLib;
    private long decoderHandle = 0;
    private NativeAudioInfo audioInfo;
    private AudioTrack audioTrack;

    private boolean isPlaying = false;
    private boolean isPaused = false;
    private Thread decodeThread;

    private long currentSampleOffset = 0;
    private PlaybackListener listener;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    public NativeAudioPlayer(NativeLib nativeLib) {
        this.nativeLib = nativeLib;
    }

    public void setPlaybackListener(PlaybackListener listener) {
        this.listener = listener;
    }

    public NativeAudioInfo open(String path) {
        release();
        decoderHandle = nativeLib.nativeOpenAudio(path);
        if (decoderHandle != 0) {
            audioInfo = nativeLib.nativeGetAudioInfo(decoderHandle);
            initAudioTrack();
            return audioInfo;
        }
        return null;
    }

    private void initAudioTrack() {
        if (audioInfo == null)
            return;

        int channelConfig = (audioInfo.channels == 1) ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO;
        int bufferSize = AudioTrack.getMinBufferSize(audioInfo.sampleRate, channelConfig,
                AudioFormat.ENCODING_PCM_16BIT);

        bufferSize = Math.max(bufferSize, audioInfo.sampleRate * audioInfo.channels * 2 / 10);

        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
                audioInfo.sampleRate,
                channelConfig,
                AudioFormat.ENCODING_PCM_16BIT,
                bufferSize,
                AudioTrack.MODE_STREAM);
    }

    public void play() {
        if (decoderHandle == 0 || audioTrack == null)
            return;

        if (isPaused) {
            isPaused = false;
            audioTrack.play();
            return;
        }

        isPlaying = true;
        isPaused = false;
        audioTrack.play();

        decodeThread = new Thread(() -> {
            int bufferSize = audioInfo.sampleRate / 10; // 100ms chunks
            short[] pcmBuffer = new short[bufferSize * audioInfo.channels];

            while (isPlaying) {
                if (isPaused) {
                    try {
                        Thread.sleep(50);
                    } catch (InterruptedException e) {
                        break;
                    }
                    continue;
                }

                int samplesRead = nativeLib.nativeDecodeAudio(decoderHandle, pcmBuffer, bufferSize);

                if (samplesRead > 0) {
                    audioTrack.write(pcmBuffer, 0, samplesRead * audioInfo.channels);
                    currentSampleOffset += samplesRead;

                    if (listener != null) {
                        mainHandler.post(() -> listener.onProgress(
                                currentSampleOffset,
                                audioInfo.totalSamples,
                                (double) currentSampleOffset / audioInfo.sampleRate,
                                audioInfo.durationSeconds));
                    }
                } else if (samplesRead == 0) {
                    // EOF
                    isPlaying = false;
                    if (listener != null) {
                        mainHandler.post(() -> listener.onCompletion());
                    }
                    break;
                } else {
                    // Error
                    isPlaying = false;
                    if (listener != null) {
                        mainHandler.post(() -> listener.onError("Decode error"));
                    }
                    break;
                }
            }
        });
        decodeThread.start();
    }

    public void pause() {
        isPaused = true;
        if (audioTrack != null) {
            audioTrack.pause();
        }
    }

    public void stop() {
        isPlaying = false;
        isPaused = false;
        if (decodeThread != null) {
            try {
                decodeThread.join(500);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        if (audioTrack != null) {
            audioTrack.stop();
            audioTrack.flush();
        }
        currentSampleOffset = 0;
    }

    public void seekTo(double seconds) {
        if (decoderHandle == 0 || audioInfo == null)
            return;

        long sampleOffset = (long) (seconds * audioInfo.sampleRate);
        if (sampleOffset < 0)
            sampleOffset = 0;
        if (sampleOffset > audioInfo.totalSamples)
            sampleOffset = audioInfo.totalSamples;

        if (nativeLib.nativeSeekAudio(decoderHandle, sampleOffset)) {
            currentSampleOffset = sampleOffset;
            if (audioTrack != null && isPlaying && !isPaused) {
                audioTrack.flush();
            }
            if (listener != null) {
                mainHandler.post(() -> listener.onProgress(
                        currentSampleOffset,
                        audioInfo.totalSamples,
                        (double) currentSampleOffset / audioInfo.sampleRate,
                        audioInfo.durationSeconds));
            }
        }
    }

    public void release() {
        stop();
        if (audioTrack != null) {
            audioTrack.release();
            audioTrack = null;
        }
        if (decoderHandle != 0) {
            nativeLib.nativeCloseAudio(decoderHandle);
            decoderHandle = 0;
        }
        audioInfo = null;
    }

    public NativeAudioInfo getAudioInfo() {
        return audioInfo;
    }

    public boolean isPlaying() {
        return isPlaying && !isPaused;
    }
}
