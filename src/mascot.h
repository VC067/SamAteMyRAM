#pragma once
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include <random>

enum class MascotState {
    Idle,
    Eating,
    HalfFull,
    AlmostFull,
    FinalForm,
    Deflating
};

struct RamParticle {
    float x, y;
    float startX, startY;
    float targetX, targetY;
    float progress;   // 0.0 to 1.0
    float speed;
    float rotation;
    float scale;
    bool active;
};

struct CrumbParticle {
    float x, y;
    float vx, vy;
    float life;       // 1.0 down to 0.0
    float decay;
    char text[2];     // '0' or '1' or '*'
    D2D1_COLOR_F color;
    bool active;
};

class MascotRenderer {
public:
    MascotRenderer();
    ~MascotRenderer();

    void Initialize(ID2D1RenderTarget* pRT, IDWriteFactory* pDWrite);
    void DiscardDeviceResources();
    void Update(float deltaTime, float memoryProgress, bool isSacrificing, bool isAccepted, float mouseX, float mouseY);
    void Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds);
    void TriggerDeflation();

    MascotState GetCurrentState() const { return m_state; }

private:
    void RenderKnotBody(ID2D1RenderTarget* pRT, float cx, float cy, float scale, float wobble);
    void RenderEyes(ID2D1RenderTarget* pRT, float cx, float cy, float scale);
    void RenderMouth(ID2D1RenderTarget* pRT, float cx, float cy, float scale);
    void RenderLimbs(ID2D1RenderTarget* pRT, float cx, float cy, float scale);
    void RenderRamParticles(ID2D1RenderTarget* pRT);
    void RenderCrumbs(ID2D1RenderTarget* pRT);
    void SpawnRamStick(float mouthX, float mouthY);
    void SpawnCrumbs(float x, float y);

    MascotState m_state;
    float m_memoryProgress;  // 0.0 to 1.0
    float m_currentBellyScale;
    float m_targetBellyScale;
    float m_bellyVelocity;

    float m_animTime;
    float m_blinkTimer;
    bool m_isBlinking;
    float m_chewPhase;
    float m_burpTimer;
    float m_burpRingRadius;

    float m_lookAtX;
    float m_lookAtY;

    // Deflation state
    bool m_isDeflating;
    float m_deflateTime;
    bool m_wasAccepted;
    float m_flourishTime;

    // Direct2D Resources
    ID2D1SolidColorBrush* m_pBrushGreen;
    ID2D1SolidColorBrush* m_pBrushGreenDark;
    ID2D1SolidColorBrush* m_pBrushGreenLight;
    ID2D1SolidColorBrush* m_pBrushWhite;
    ID2D1SolidColorBrush* m_pBrushBlack;
    ID2D1SolidColorBrush* m_pBrushMouthInside;
    ID2D1SolidColorBrush* m_pBrushTongue;
    ID2D1SolidColorBrush* m_pBrushGold;
    ID2D1SolidColorBrush* m_pBrushChip;
    ID2D1RadialGradientBrush* m_pBellyGradient;
    IDWriteTextFormat* m_pBitTextFormat;

    std::vector<RamParticle> m_ramParticles;
    std::vector<CrumbParticle> m_crumbs;
    std::mt19937 m_rng;
    float m_spawnCooldown;
};
