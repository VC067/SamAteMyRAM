#include "audio.h"
#include <thread>
#include <cstring>
#include <random>

namespace {
    // Generate standard RIFF WAV in-memory buffer
    std::vector<char> CreateWavBuffer(const std::vector<short>& samples, int sampleRate) {
        int dataSize = static_cast<int>(samples.size() * sizeof(short));
        int chunkSize = 36 + dataSize;
        std::vector<char> wav(44 + dataSize);

        // RIFF header
        std::memcpy(&wav[0], "RIFF", 4);
        std::memcpy(&wav[4], &chunkSize, 4);
        std::memcpy(&wav[8], "WAVE", 4);

        // fmt subchunk
        std::memcpy(&wav[12], "fmt ", 4);
        int subchunk1Size = 16;
        short audioFormat = 1; // PCM
        short numChannels = 1; // Mono
        int byteRate = sampleRate * numChannels * sizeof(short);
        short blockAlign = numChannels * sizeof(short);
        short bitsPerSample = 16;

        std::memcpy(&wav[16], &subchunk1Size, 4);
        std::memcpy(&wav[20], &audioFormat, 2);
        std::memcpy(&wav[22], &numChannels, 2);
        std::memcpy(&wav[24], &sampleRate, 4);
        std::memcpy(&wav[28], &byteRate, 4);
        std::memcpy(&wav[32], &blockAlign, 2);
        std::memcpy(&wav[34], &bitsPerSample, 2);

        // data subchunk
        std::memcpy(&wav[36], "data", 4);
        std::memcpy(&wav[40], &dataSize, 4);
        if (dataSize > 0) {
            std::memcpy(&wav[44], samples.data(), dataSize);
        }
        return wav;
    }
}

void SoundEffects::PlayPCM(const std::vector<short>& samples, int sampleRate) {
    if (m_muted || samples.empty()) return;

    // One sound at a time. Each playback owns its buffer so overlapping chomps cannot free it early.
    static std::atomic<bool> s_playing{false};
    bool expected = false;
    if (!s_playing.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([samples, sampleRate]() {
        std::vector<char> wav = CreateWavBuffer(samples, sampleRate);
        PlaySoundA(wav.data(), NULL, SND_MEMORY | SND_SYNC | SND_NODEFAULT);
        s_playing.store(false);
    }).detach();
}

void SoundEffects::PlayChomp() {
    if (m_muted) return;
    const int sampleRate = 22050;
    const int durationSamples = sampleRate * 12 / 100; // 120ms
    std::vector<short> samples(durationSamples);
    
    // Crunchy bite frequency sweep with noise burst
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> noise(-0.4f, 0.4f);

    for (int i = 0; i < durationSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float progress = static_cast<float>(i) / durationSamples;
        float freq = 600.0f - 400.0f * progress;
        float env = (1.0f - progress) * (1.0f - progress);
        float tone = std::sin(2.0f * 3.14159f * freq * t);
        float n = noise(rng);
        float mixed = (tone * 0.6f + n * 0.4f) * env;
        samples[i] = static_cast<short>(mixed * 24000.0f);
    }
    PlayPCM(samples, sampleRate);
}

void SoundEffects::PlaySwallow() {
    if (m_muted) return;
    const int sampleRate = 22050;
    const int durationSamples = sampleRate * 18 / 100; // 180ms
    std::vector<short> samples(durationSamples);

    for (int i = 0; i < durationSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float progress = static_cast<float>(i) / durationSamples;
        // Pitch drop gulp
        float freq = 450.0f - 250.0f * std::sin(progress * 3.14159f * 0.5f);
        float env = std::sin(progress * 3.14159f);
        float tone = std::sin(2.0f * 3.14159f * freq * t);
        samples[i] = static_cast<short>(tone * env * 22000.0f);
    }
    PlayPCM(samples, sampleRate);
}

void SoundEffects::PlayBurp() {
    if (m_muted) return;
    const int sampleRate = 22050;
    const int durationSamples = sampleRate * 35 / 100; // 350ms
    std::vector<short> samples(durationSamples);

    for (int i = 0; i < durationSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float progress = static_cast<float>(i) / durationSamples;
        // Low rumble frequency modulation
        float mod = std::sin(2.0f * 3.14159f * 25.0f * t);
        float freq = 80.0f + 30.0f * mod - 20.0f * progress;
        float env = std::sin(progress * 3.14159f);
        float tone = std::sin(2.0f * 3.14159f * freq * t);
        samples[i] = static_cast<short>(tone * env * 26000.0f);
    }
    PlayPCM(samples, sampleRate);
}

void SoundEffects::PlayAccepted() {
    if (m_muted) return;
    const int sampleRate = 22050;
    // Ascending celebratory compute arpeggio: C5 (523), E5 (659), G5 (784), C6 (1046)
    float notes[] = { 523.25f, 659.25f, 783.99f, 1046.50f };
    int noteSamples = sampleRate * 12 / 100; // 120ms each
    int totalSamples = noteSamples * 4 + sampleRate * 3 / 10; // extra sustain
    std::vector<short> samples(totalSamples, 0);

    for (int n = 0; n < 4; ++n) {
        int start = n * noteSamples;
        float freq = notes[n];
        int noteLen = (n == 3) ? (noteSamples + sampleRate * 3 / 10) : noteSamples;
        for (int i = 0; i < noteLen; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float progress = static_cast<float>(i) / noteLen;
            float env = (1.0f - progress * 0.8f);
            float tone = std::sin(2.0f * 3.14159f * freq * t) * 0.7f + 
                         std::sin(4.0f * 3.14159f * freq * t) * 0.3f;
            if (start + i < totalSamples) {
                samples[start + i] += static_cast<short>(tone * env * 20000.0f);
            }
        }
    }
    PlayPCM(samples, sampleRate);
}

void SoundEffects::PlayReclaim() {
    if (m_muted) return;
    const int sampleRate = 22050;
    const int durationSamples = sampleRate * 45 / 100; // 450ms
    std::vector<short> samples(durationSamples);

    for (int i = 0; i < durationSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float progress = static_cast<float>(i) / durationSamples;
        // Slide whistle down
        float freq = 900.0f - 700.0f * progress * progress;
        float env = 1.0f - progress;
        float tone = std::sin(2.0f * 3.14159f * freq * t);
        samples[i] = static_cast<short>(tone * env * 22000.0f);
    }
    PlayPCM(samples, sampleRate);
}
