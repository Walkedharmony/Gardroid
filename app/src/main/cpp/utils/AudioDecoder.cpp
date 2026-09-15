#include "AudioDecoder.h"
#include <algorithm>
#include <cstring>
#include <fstream>

#include "stb_vorbis.c"

#include <opusfile.h>

#define LOG_TAG "NativeAudioDecoder"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

AudioDecoder::AudioDecoder() {}

AudioDecoder::~AudioDecoder() { close(); }

bool AudioDecoder::openFile(const std::string &path) {
  std::lock_guard<std::mutex> lock(decodeMutex);
  close();

  // Simple magic byte check
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    LOGE("Failed to open file: %s", path.c_str());
    return false;
  }

  char magic[4] = {0};
  file.read(magic, 4);
  file.close();

  LOGI("Audio file magic bytes: %02x %02x %02x %02x", magic[0], magic[1],
       magic[2], magic[3]);

  if (std::memcmp(magic, "OggS", 4) != 0) {
    LOGE("Not an Ogg file: %s (Magic: %c%c%c%c)", path.c_str(), magic[0],
         magic[1], magic[2], magic[3]);
    // Don't return false immediately, just log it, maybe the library can handle
    // it. return false;
  }

  // Try Opus first
  int err = 0;
  OggOpusFile *op = op_open_file(path.c_str(), &err);
  if (op != nullptr && err == 0) {
    opusHandle = op;
    format = OPUS;
    const OpusHead *head = op_head(op, -1);
    channels = head->channel_count;
    // opusfile always decodes to 48kHz by default
    sampleRate = 48000;
    totalSamples = op_pcm_total(op, -1);
    LOGI("Opened Opus file. Channels: %d, SampleRate: %d, TotalSamples: %lld",
         channels, sampleRate, totalSamples);
    return true;
  }

  // If Opus fails, try Vorbis
  stb_vorbis *vorbis = stb_vorbis_open_filename(path.c_str(), &err, nullptr);
  if (vorbis != nullptr) {
    vorbisHandle = vorbis;
    format = OGG_VORBIS;
    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    channels = info.channels;
    sampleRate = info.sample_rate;
    totalSamples = stb_vorbis_stream_length_in_samples(vorbis);
    LOGI("Opened Vorbis file. Channels: %d, SampleRate: %d, TotalSamples: %lld",
         channels, sampleRate, totalSamples);
    return true;
  }

  LOGE("Failed to open file as either Opus or Vorbis: %s", path.c_str());
  return false;
}

double AudioDecoder::getDurationSeconds() const {
  if (sampleRate == 0)
    return 0.0;
  return static_cast<double>(totalSamples) / sampleRate;
}

const char *AudioDecoder::getCodecName() const {
  if (format == OPUS)
    return "Opus (libopus)";
  if (format == OGG_VORBIS)
    return "Ogg Vorbis (stb_vorbis)";
  return "Unknown";
}

int AudioDecoder::decode(short *pcm_out, int max_samples_per_channel) {
  std::lock_guard<std::mutex> lock(decodeMutex);

  if (format == OPUS && opusHandle != nullptr) {
    // opusfile decode
    OggOpusFile *op = static_cast<OggOpusFile *>(opusHandle);
    int samples_read =
        op_read(op, pcm_out, max_samples_per_channel * channels, nullptr);
    if (samples_read < 0) {
      LOGE("Opus decode error: %d", samples_read);
      return -1;
    }
    return samples_read;
  } else if (format == OGG_VORBIS && vorbisHandle != nullptr) {
    // stb_vorbis decode
    stb_vorbis *vorbis = static_cast<stb_vorbis *>(vorbisHandle);
    int samples_read = stb_vorbis_get_samples_short_interleaved(
        vorbis, channels, pcm_out, max_samples_per_channel * channels);
    return samples_read;
  }

  return -1;
}

bool AudioDecoder::seek(long long sample_offset) {
  std::lock_guard<std::mutex> lock(decodeMutex);

  if (format == OPUS && opusHandle != nullptr) {
    OggOpusFile *op = static_cast<OggOpusFile *>(opusHandle);
    int ret = op_pcm_seek(op, sample_offset);
    return ret == 0;
  } else if (format == OGG_VORBIS && vorbisHandle != nullptr) {
    stb_vorbis *vorbis = static_cast<stb_vorbis *>(vorbisHandle);
    return stb_vorbis_seek(vorbis, sample_offset) != 0;
  }

  return false;
}

void AudioDecoder::close() {
  if (format == OPUS && opusHandle != nullptr) {
    op_free(static_cast<OggOpusFile *>(opusHandle));
    opusHandle = nullptr;
  } else if (format == OGG_VORBIS && vorbisHandle != nullptr) {
    stb_vorbis_close(static_cast<stb_vorbis *>(vorbisHandle));
    vorbisHandle = nullptr;
  }
  format = UNKNOWN;
  channels = 0;
  sampleRate = 0;
  totalSamples = 0;
}
