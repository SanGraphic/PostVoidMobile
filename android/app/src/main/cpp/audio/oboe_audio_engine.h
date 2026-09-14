#pragma once

#include <oboe/Oboe.h>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "gms_types.h"

struct ActiveVoice {
    int soundId = -1;
    bool isPlaying = false;
    bool loop = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    size_t sampleIndex = 0;
    std::shared_ptr<std::vector<int16_t>> pcmData;
};

class OboeAudioEngine : public oboe::AudioStreamDataCallback {
public:
    static OboeAudioEngine& get() {
        static OboeAudioEngine instance;
        return instance;
    }

    bool initialize();
    void shutdown();

    int playSound(int soundId, float volume, bool loop, float pitch = 1.0f);
    void stopSound(int voiceId);
    void stopAllSounds();
    void setSoundVolume(int voiceId, float volume);

    void registerSound(int soundId, const std::string& name, std::shared_ptr<std::vector<int16_t>> pcm);

    // Oboe Callback
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* audioStream, void* audioData, int32_t numFrames) override;

private:
    OboeAudioEngine();
    ~OboeAudioEngine();

    std::shared_ptr<oboe::AudioStream> m_stream;
    std::mutex m_audioMutex;

    int m_nextVoiceId = 1;
    std::unordered_map<int, ActiveVoice> m_voices;
    std::unordered_map<int, std::shared_ptr<std::vector<int16_t>>> m_soundBank;
};
