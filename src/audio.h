#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <atomic>
#include <cmath>

class SoundEffects {
public:
    static SoundEffects& Instance() {
        static SoundEffects s_instance;
        return s_instance;
    }

    void SetMuted(bool muted) { m_muted = muted; }
    bool IsMuted() const { return m_muted; }
    void ToggleMute() { m_muted = !m_muted; }

    void PlayChomp();
    void PlaySwallow();
    void PlayBurp();
    void PlayAccepted();
    void PlayReclaim();

private:
    SoundEffects() : m_muted(false) {}
    ~SoundEffects() {}

    void PlayPCM(const std::vector<short>& samples, int sampleRate = 22050);

    std::atomic<bool> m_muted;
};
