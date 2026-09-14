#include "oboe_audio_engine.h"
#include <cstring>
#include <algorithm>

OboeAudioEngine::OboeAudioEngine() {}
OboeAudioEngine::~OboeAudioEngine() {}

bool OboeAudioEngine::initialize() {
    LOGI("[OboeAudioEngine] Starting Oboe Audio Stream...");

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::I16)
           ->setChannelCount(oboe::ChannelCount::Stereo)
           ->setSampleRate(44100)
           ->setDataCallback(this);

    oboe::Result result = builder.openStream(m_stream);
    if (result != oboe::Result::OK) {
        LOGE("[OboeAudioEngine] Failed to open stream: %s", oboe::convertToText(result));
        return false;
    }

    result = m_stream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("[OboeAudioEngine] Failed to start stream: %s", oboe::convertToText(result));
        return false;
    }

    LOGI("[OboeAudioEngine] Audio engine online (SampleRate: 44100Hz, Channels: 2, BufferSize: %d frames)",
         m_stream->getBufferSizeInFrames());
    return true;
}

void OboeAudioEngine::shutdown() {
    if (m_stream) {
        m_stream->stop();
        m_stream->close();
        m_stream.reset();
    }
    stopAllSounds();
    m_soundBank.clear();
}

int OboeAudioEngine::playSound(int soundId, float volume, bool loop, float pitch) {
    std::lock_guard<std::mutex> lock(m_audioMutex);

    auto it = m_soundBank.find(soundId);
    if (it == m_soundBank.end() || !it->second || it->second->empty()) {
        return -1;
    }

    int voiceId = m_nextVoiceId++;
    ActiveVoice voice;
    voice.soundId = soundId;
    voice.isPlaying = true;
    voice.loop = loop;
    voice.volume = volume;
    voice.pitch = pitch;
    voice.sampleIndex = 0;
    voice.pcmData = it->second;

    m_voices[voiceId] = voice;
    return voiceId;
}

void OboeAudioEngine::stopSound(int voiceId) {
    std::lock_guard<std::mutex> lock(m_audioMutex);
    m_voices.erase(voiceId);
}

void OboeAudioEngine::stopAllSounds() {
    std::lock_guard<std::mutex> lock(m_audioMutex);
    m_voices.clear();
}

void OboeAudioEngine::setSoundVolume(int voiceId, float volume) {
    std::lock_guard<std::mutex> lock(m_audioMutex);
    auto it = m_voices.find(voiceId);
    if (it != m_voices.end()) {
        it->second.volume = volume;
    }
}

void OboeAudioEngine::registerSound(int soundId, const std::string& name, std::shared_ptr<std::vector<int16_t>> pcm) {
    std::lock_guard<std::mutex> lock(m_audioMutex);
    m_soundBank[soundId] = pcm;
}

oboe::DataCallbackResult OboeAudioEngine::onAudioReady(oboe::AudioStream* audioStream, void* audioData, int32_t numFrames) {
    int16_t* outputBuffer = static_cast<int16_t*>(audioData);
    std::memset(outputBuffer, 0, numFrames * 2 * sizeof(int16_t));

    std::lock_guard<std::mutex> lock(m_audioMutex);
    if (m_voices.empty()) {
        return oboe::DataCallbackResult::Continue;
    }

    std::vector<int32_t> mixBuffer(numFrames * 2, 0);

    for (auto it = m_voices.begin(); it != m_voices.end();) {
        ActiveVoice& voice = it->second;
        if (!voice.isPlaying || !voice.pcmData) {
            it = m_voices.erase(it);
            continue;
        }

        const auto& pcm = *voice.pcmData;
        size_t totalSamples = pcm.size();

        for (int32_t f = 0; f < numFrames; f++) {
            if (voice.sampleIndex >= totalSamples) {
                if (voice.loop) {
                    voice.sampleIndex = 0;
                } else {
                    voice.isPlaying = false;
                    break;
                }
            }

            int16_t sampleL = pcm[voice.sampleIndex++];
            int16_t sampleR = (voice.sampleIndex < totalSamples) ? pcm[voice.sampleIndex++] : sampleL;

            mixBuffer[f * 2] += static_cast<int32_t>(sampleL * voice.volume);
            mixBuffer[f * 2 + 1] += static_cast<int32_t>(sampleR * voice.volume);
        }

        if (!voice.isPlaying) {
            it = m_voices.erase(it);
        } else {
            ++it;
        }
    }

    // Master clamp to 16-bit PCM
    for (int32_t i = 0; i < numFrames * 2; i++) {
        int32_t mixed = mixBuffer[i];
        if (mixed > 32767) mixed = 32767;
        else if (mixed < -32768) mixed = -32768;
        outputBuffer[i] = static_cast<int16_t>(mixed);
    }

    return oboe::DataCallbackResult::Continue;
}
