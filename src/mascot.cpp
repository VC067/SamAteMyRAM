#include "mascot.h"
#include "audio.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

MascotRenderer::MascotRenderer()
    : m_state(MascotState::Idle),
      m_memoryProgress(0.0f),
      m_currentBellyScale(0.0f),
      m_targetBellyScale(0.0f),
      m_bellyVelocity(0.0f),
      m_animTime(0.0f),
      m_blinkTimer(3.0f),
      m_isBlinking(false),
      m_chewPhase(0.0f),
      m_burpTimer(0.0f),
      m_burpRingRadius(0.0f),
      m_lookAtX(0.0f),
      m_lookAtY(0.0f),
      m_isDeflating(false),
      m_deflateTime(0.0f),
      m_wasAccepted(false),
      m_flourishTime(0.0f),
      m_pBrushGreen(nullptr),
      m_pBrushGreenDark(nullptr),
      m_pBrushGreenLight(nullptr),
      m_pBrushWhite(nullptr),
      m_pBrushBlack(nullptr),
      m_pBrushMouthInside(nullptr),
      m_pBrushTongue(nullptr),
      m_pBrushGold(nullptr),
      m_pBrushChip(nullptr),
      m_pBellyGradient(nullptr),
      m_pBitTextFormat(nullptr),
      m_rng(42),
      m_spawnCooldown(0.0f) {
}

MascotRenderer::~MascotRenderer() {
    DiscardDeviceResources();
}

void MascotRenderer::DiscardDeviceResources() {
    if (m_pBrushGreen) m_pBrushGreen->Release();
    if (m_pBrushGreenDark) m_pBrushGreenDark->Release();
    if (m_pBrushGreenLight) m_pBrushGreenLight->Release();
    if (m_pBrushWhite) m_pBrushWhite->Release();
    if (m_pBrushBlack) m_pBrushBlack->Release();
    if (m_pBrushMouthInside) m_pBrushMouthInside->Release();
    if (m_pBrushTongue) m_pBrushTongue->Release();
    if (m_pBrushGold) m_pBrushGold->Release();
    if (m_pBrushChip) m_pBrushChip->Release();
    if (m_pBellyGradient) m_pBellyGradient->Release();
    if (m_pBitTextFormat) m_pBitTextFormat->Release();
    m_pBrushGreen = nullptr;
    m_pBrushGreenDark = nullptr;
    m_pBrushGreenLight = nullptr;
    m_pBrushWhite = nullptr;
    m_pBrushBlack = nullptr;
    m_pBrushMouthInside = nullptr;
    m_pBrushTongue = nullptr;
    m_pBrushGold = nullptr;
    m_pBrushChip = nullptr;
    m_pBellyGradient = nullptr;
    m_pBitTextFormat = nullptr;
}

void MascotRenderer::Initialize(ID2D1RenderTarget* pRT, IDWriteFactory* pDWrite) {
    DiscardDeviceResources();
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.063f, 0.639f, 0.498f, 1.0f), &m_pBrushGreen);        // #10A37F
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.035f, 0.420f, 0.325f, 1.0f), &m_pBrushGreenDark);    // #096B53
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.200f, 0.880f, 0.680f, 1.0f), &m_pBrushGreenLight);   // #33E0AD
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.98f, 0.98f, 0.98f, 1.0f), &m_pBrushWhite);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.10f, 1.0f), &m_pBrushBlack);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.04f, 0.05f, 1.0f), &m_pBrushMouthInside);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.40f, 0.48f, 1.0f), &m_pBrushTongue);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.92f, 0.76f, 0.20f, 1.0f), &m_pBrushGold);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.14f, 1.0f), &m_pBrushChip);

    if (pDWrite) {
        pDWrite->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f,
            L"en-us",
            &m_pBitTextFormat
        );
    }
}

void MascotRenderer::TriggerDeflation() {
    m_isDeflating = true;
    m_deflateTime = 0.0f;
    m_flourishTime = 0.0f;
    m_wasAccepted = false;
    m_state = MascotState::Deflating;
    SoundEffects::Instance().PlayReclaim();
}

void MascotRenderer::SpawnRamStick(float mouthX, float mouthY) {
    RamParticle p;
    std::uniform_real_distribution<float> sideDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> yDist(-120.0f, 120.0f);
    std::uniform_real_distribution<float> rotDist(-0.4f, 0.4f);

    bool fromLeft = sideDist(m_rng) > 0.5f;
    p.startX = fromLeft ? (mouthX - 320.0f) : (mouthX + 320.0f);
    p.startY = mouthY + yDist(m_rng);
    p.targetX = mouthX;
    p.targetY = mouthY;
    p.x = p.startX;
    p.y = p.startY;
    p.progress = 0.0f;
    p.speed = 1.3f + sideDist(m_rng) * 0.8f;
    p.rotation = rotDist(m_rng);
    p.scale = 0.7f + sideDist(m_rng) * 0.5f;
    p.active = true;

    m_ramParticles.push_back(p);
}

void MascotRenderer::SpawnCrumbs(float x, float y) {
    std::uniform_real_distribution<float> angleDist(0.0f, 6.283f);
    std::uniform_real_distribution<float> speedDist(40.0f, 150.0f);
    std::uniform_int_distribution<int> bitDist(0, 1);

    for (int i = 0; i < 6; ++i) {
        CrumbParticle c;
        c.x = x;
        c.y = y;
        float angle = angleDist(m_rng);
        float speed = speedDist(m_rng);
        c.vx = std::cos(angle) * speed;
        c.vy = std::sin(angle) * speed - 30.0f; // slight upward bounce
        c.life = 1.0f;
        c.decay = 1.5f + angleDist(m_rng) * 0.5f;
        c.text[0] = (bitDist(m_rng) == 1) ? '1' : '0';
        c.text[1] = '\0';
        c.color = D2D1::ColorF(0.20f, 0.88f, 0.68f, 1.0f);
        c.active = true;
        m_crumbs.push_back(c);
    }
}

void MascotRenderer::Update(float deltaTime, float memoryProgress, bool isSacrificing, bool isAccepted, float mouseX, float mouseY) {
    m_animTime += deltaTime;
    m_memoryProgress = memoryProgress;

    // Handle Deflation Animation
    if (m_isDeflating) {
        m_deflateTime += deltaTime;
        float t = m_deflateTime / 1.5f;
        if (t >= 1.0f) {
            m_isDeflating = false;
            m_currentBellyScale = 0.0f;
            m_targetBellyScale = 0.0f;
            m_state = MascotState::Idle;
        } else {
            // Rapid comical deflating wobble
            m_targetBellyScale = (1.0f - t) * (1.0f - t);
            m_currentBellyScale = m_targetBellyScale + 0.15f * std::sin(m_deflateTime * 35.0f) * (1.0f - t);
            return;
        }
    }

    if (isAccepted && !m_wasAccepted) {
        m_flourishTime = 1.15f;
        SoundEffects::Instance().PlayAccepted();
    }
    m_wasAccepted = isAccepted;
    if (!isAccepted) {
        m_flourishTime = 0.0f;
    } else if (m_flourishTime > 0.0f) {
        m_flourishTime -= deltaTime;
        if (m_flourishTime < 0.0f) m_flourishTime = 0.0f;
    }

    const bool chewing = isSacrificing || m_flourishTime > 0.0f;

    // Determine Mascot State. A short chew plays after the limit is reached, then final form.
    if (m_flourishTime > 0.0f) {
        m_state = MascotState::AlmostFull;
    } else if (isAccepted || memoryProgress >= 0.99f) {
        m_state = MascotState::FinalForm;
    } else if (memoryProgress >= 0.75f) {
        m_state = MascotState::AlmostFull;
    } else if (memoryProgress >= 0.40f) {
        m_state = MascotState::HalfFull;
    } else if (isSacrificing) {
        m_state = MascotState::Eating;
    } else {
        m_state = MascotState::Idle;
    }

    // Belly scale target based on memory progress. Final flourish fills the belly.
    m_targetBellyScale = (m_flourishTime > 0.0f || isAccepted) ? 1.0f : memoryProgress;

    // Smooth spring physics for belly wobble
    float springK = 35.0f;
    float damping = 0.75f;
    float force = (m_targetBellyScale - m_currentBellyScale) * springK;
    m_bellyVelocity = (m_bellyVelocity + force * deltaTime) * damping;
    m_currentBellyScale += m_bellyVelocity * deltaTime;

    // Chewing animation. Heavier states chew slower.
    if (chewing) {
        float chewRate = 12.0f;
        if (m_state == MascotState::HalfFull) chewRate = 9.0f;
        if (m_state == MascotState::AlmostFull) chewRate = 6.5f;
        m_chewPhase += deltaTime * chewRate;
        m_spawnCooldown -= deltaTime;
        if (m_spawnCooldown <= 0.0f) {
            SpawnRamStick(m_lookAtX, m_lookAtY);
            m_spawnCooldown = 0.25f - memoryProgress * 0.12f;
            if (m_state == MascotState::AlmostFull) m_spawnCooldown += 0.08f;
            if (m_spawnCooldown < 0.10f) m_spawnCooldown = 0.10f;
        }
    } else {
        m_chewPhase *= 0.9f;
    }

    // Blinking timer
    m_blinkTimer -= deltaTime;
    if (m_blinkTimer <= 0.0f) {
        m_isBlinking = !m_isBlinking;
        m_blinkTimer = m_isBlinking ? 0.15f : (2.5f + (rand() % 200) / 100.0f);
    }

    // Occasional burp shockwave when heavy
    if (m_state == MascotState::AlmostFull || m_state == MascotState::HalfFull) {
        m_burpTimer += deltaTime;
        if (m_burpTimer > 6.0f) {
            m_burpTimer = 0.0f;
            m_burpRingRadius = 10.0f;
            SoundEffects::Instance().PlayBurp();
        }
    }
    if (m_burpRingRadius > 0.0f) {
        m_burpRingRadius += deltaTime * 200.0f;
        if (m_burpRingRadius > 180.0f) m_burpRingRadius = 0.0f;
    }

    // Update RAM particles
    for (auto& p : m_ramParticles) {
        if (!p.active) continue;
        p.progress += deltaTime * p.speed;
        if (p.progress >= 1.0f) {
            p.active = false;
            SpawnCrumbs(p.targetX, p.targetY);
            SoundEffects::Instance().PlayChomp();
            // Jolt belly spring
            m_bellyVelocity += 1.5f;
        } else {
            // Bezier curve toward mouth
            float t = p.progress;
            float heightArc = -60.0f * std::sin(t * 3.14159f);
            p.x = p.startX + (p.targetX - p.startX) * t;
            p.y = p.startY + (p.targetY - p.startY) * t + heightArc;
            p.rotation += deltaTime * 3.0f;
        }
    }
    m_ramParticles.erase(
        std::remove_if(m_ramParticles.begin(), m_ramParticles.end(), [](const RamParticle& p) { return !p.active; }),
        m_ramParticles.end()
    );

    // Update Crumb particles
    for (auto& c : m_crumbs) {
        if (!c.active) continue;
        c.x += c.vx * deltaTime;
        c.y += c.vy * deltaTime;
        c.vy += 180.0f * deltaTime; // gravity
        c.life -= c.decay * deltaTime;
        if (c.life <= 0.0f) c.active = false;
    }
    m_crumbs.erase(
        std::remove_if(m_crumbs.begin(), m_crumbs.end(), [](const CrumbParticle& c) { return !c.active; }),
        m_crumbs.end()
    );
}

void MascotRenderer::RenderKnotBody(ID2D1RenderTarget* pRT, float cx, float cy, float scale, float wobble) {
    // 6-petaled ChatGPT-style knot geometry
    const int numPetals = 6;
    float baseRadius = 48.0f * scale;
    float petalDist = 32.0f * scale * (1.0f + wobble * 0.15f);

    // Outer Petal Loops
    for (int i = 0; i < numPetals; ++i) {
        float angle = i * (6.28318f / numPetals) + m_animTime * 0.4f;
        float px = cx + std::cos(angle) * petalDist;
        float py = cy + std::sin(angle) * petalDist;

        D2D1_ROUNDED_RECT loopRect = D2D1::RoundedRect(
            D2D1::RectF(px - 22.0f * scale, py - 38.0f * scale, px + 22.0f * scale, py + 38.0f * scale),
            20.0f * scale, 20.0f * scale
        );

        D2D1_MATRIX_3X2_F rotMatrix = D2D1::Matrix3x2F::Rotation(
            (angle * 180.0f / 3.14159f) + 45.0f,
            D2D1::Point2F(px, py)
        );
        pRT->SetTransform(rotMatrix);
        pRT->FillRoundedRectangle(&loopRect, m_pBrushGreenDark);
        pRT->DrawRoundedRectangle(&loopRect, m_pBrushGreenLight, 3.5f * scale);
    }
    pRT->SetTransform(D2D1::Matrix3x2F::Identity());

    // Expanding Central Belly
    float bellyScaleFactor = 1.0f + m_currentBellyScale * 1.5f;
    float bellyRadius = baseRadius * bellyScaleFactor;
    float bellyOffsetY = 15.0f * m_currentBellyScale * scale;

    D2D1_ELLIPSE belly = D2D1::Ellipse(
        D2D1::Point2F(cx, cy + bellyOffsetY),
        bellyRadius * 1.05f,
        bellyRadius * (1.0f + 0.12f * std::sin(m_animTime * 5.0f) * wobble)
    );

    // Belly glow / shadow
    pRT->FillEllipse(&belly, m_pBrushGreen);
    pRT->DrawEllipse(&belly, m_pBrushGreenLight, 4.0f * scale);

    // Belly button on huge belly
    if (m_currentBellyScale > 0.4f) {
        D2D1_ELLIPSE navel = D2D1::Ellipse(
            D2D1::Point2F(cx, cy + bellyOffsetY + bellyRadius * 0.5f),
            3.0f * scale, 2.0f * scale
        );
        pRT->DrawEllipse(&navel, m_pBrushGreenDark, 2.0f);
    }
}

void MascotRenderer::RenderLimbs(ID2D1RenderTarget* pRT, float cx, float cy, float scale) {
    float bellyRadius = 48.0f * scale * (1.0f + m_currentBellyScale * 1.5f);
    float armDist = bellyRadius * 0.95f;

    const bool waving = m_chewPhase > 0.15f &&
        m_state != MascotState::FinalForm &&
        m_state != MascotState::Idle &&
        m_state != MascotState::Deflating;
    float armAngleL = waving ? (std::sin(m_animTime * 12.0f) * 0.4f) : 0.0f;
    D2D1_ROUNDED_RECT armL = D2D1::RoundedRect(
        D2D1::RectF(cx - armDist - 18.0f * scale, cy + armAngleL * 20.0f, cx - armDist + 6.0f * scale, cy + 20.0f * scale + armAngleL * 20.0f),
        8.0f * scale, 8.0f * scale
    );
    pRT->FillRoundedRectangle(&armL, m_pBrushGreen);
    pRT->DrawRoundedRectangle(&armL, m_pBrushGreenLight, 2.0f);

    // Right Arm
    float armAngleR = waving ? (-std::sin(m_animTime * 12.0f) * 0.4f) : 0.0f;
    D2D1_ROUNDED_RECT armR = D2D1::RoundedRect(
        D2D1::RectF(cx + armDist - 6.0f * scale, cy + armAngleR * 20.0f, cx + armDist + 18.0f * scale, cy + 20.0f * scale + armAngleR * 20.0f),
        8.0f * scale, 8.0f * scale
    );
    pRT->FillRoundedRectangle(&armR, m_pBrushGreen);
    pRT->DrawRoundedRectangle(&armR, m_pBrushGreenLight, 2.0f);

    // Stubby Feet
    float footOffsetY = cy + bellyRadius * 0.90f;
    D2D1_ROUNDED_RECT footL = D2D1::RoundedRect(
        D2D1::RectF(cx - 30.0f * scale, footOffsetY, cx - 10.0f * scale, footOffsetY + 16.0f * scale),
        8.0f * scale, 8.0f * scale
    );
    D2D1_ROUNDED_RECT footR = D2D1::RoundedRect(
        D2D1::RectF(cx + 10.0f * scale, footOffsetY, cx + 30.0f * scale, footOffsetY + 16.0f * scale),
        8.0f * scale, 8.0f * scale
    );
    pRT->FillRoundedRectangle(&footL, m_pBrushGreenDark);
    pRT->FillRoundedRectangle(&footR, m_pBrushGreenDark);
}

void MascotRenderer::RenderEyes(ID2D1RenderTarget* pRT, float cx, float cy, float scale) {
    float eyeSpacing = 28.0f * scale;
    float eyeY = cy - 20.0f * scale;
    float eyeWidth = 14.0f * scale;
    float eyeHeight = m_isBlinking ? (2.0f * scale) : (20.0f * scale);

    // Left Eye
    D2D1_ELLIPSE eyeL = D2D1::Ellipse(D2D1::Point2F(cx - eyeSpacing, eyeY), eyeWidth, eyeHeight);
    pRT->FillEllipse(&eyeL, m_pBrushWhite);
    pRT->DrawEllipse(&eyeL, m_pBrushBlack, 2.0f * scale);

    // Right Eye
    D2D1_ELLIPSE eyeR = D2D1::Ellipse(D2D1::Point2F(cx + eyeSpacing, eyeY), eyeWidth, eyeHeight);
    pRT->FillEllipse(&eyeR, m_pBrushWhite);
    pRT->DrawEllipse(&eyeR, m_pBrushBlack, 2.0f * scale);

    if (!m_isBlinking) {
        // Pupils with directional tracking
        float pupilOffsetX = 0.0f;
        float pupilOffsetY = 0.0f;
        if (m_state == MascotState::Eating && !m_ramParticles.empty()) {
            pupilOffsetX = (m_ramParticles[0].x > cx) ? 4.0f * scale : -4.0f * scale;
            pupilOffsetY = 3.0f * scale;
        } else if (m_state == MascotState::FinalForm) {
            pupilOffsetY = -3.0f * scale; // looking up proudly
        }

        float pupilRadius = 6.5f * scale;
        D2D1_ELLIPSE pupilL = D2D1::Ellipse(D2D1::Point2F(cx - eyeSpacing + pupilOffsetX, eyeY + pupilOffsetY), pupilRadius, pupilRadius);
        D2D1_ELLIPSE pupilR = D2D1::Ellipse(D2D1::Point2F(cx + eyeSpacing + pupilOffsetX, eyeY + pupilOffsetY), pupilRadius, pupilRadius);
        pRT->FillEllipse(&pupilL, m_pBrushBlack);
        pRT->FillEllipse(&pupilR, m_pBrushBlack);

        // Specular glint
        D2D1_ELLIPSE glintL = D2D1::Ellipse(D2D1::Point2F(cx - eyeSpacing + pupilOffsetX - 2.5f * scale, eyeY + pupilOffsetY - 2.5f * scale), 2.2f * scale, 2.2f * scale);
        D2D1_ELLIPSE glintR = D2D1::Ellipse(D2D1::Point2F(cx + eyeSpacing + pupilOffsetX - 2.5f * scale, eyeY + pupilOffsetY - 2.5f * scale), 2.2f * scale, 2.2f * scale);
        pRT->FillEllipse(&glintL, m_pBrushWhite);
        pRT->FillEllipse(&glintR, m_pBrushWhite);
    }
}

void MascotRenderer::RenderMouth(ID2D1RenderTarget* pRT, float cx, float cy, float scale) {
    float mouthY = cy + 12.0f * scale;
    m_lookAtX = cx;
    m_lookAtY = mouthY;

    const bool chomping = m_chewPhase > 0.15f &&
        m_state != MascotState::FinalForm &&
        m_state != MascotState::Idle &&
        m_state != MascotState::Deflating;

    if (chomping) {
        // Chomp cycle
        float chomp = std::abs(std::sin(m_chewPhase));
        float mouthW = (24.0f + 12.0f * chomp) * scale;
        float mouthH = (10.0f + 22.0f * chomp) * scale;

        D2D1_ROUNDED_RECT mouthRect = D2D1::RoundedRect(
            D2D1::RectF(cx - mouthW, mouthY - mouthH * 0.5f, cx + mouthW, mouthY + mouthH * 0.5f),
            mouthH * 0.5f, mouthH * 0.5f
        );
        pRT->FillRoundedRectangle(&mouthRect, m_pBrushMouthInside);
        pRT->DrawRoundedRectangle(&mouthRect, m_pBrushBlack, 2.5f * scale);

        // Cute pink tongue
        D2D1_ELLIPSE tongue = D2D1::Ellipse(
            D2D1::Point2F(cx, mouthY + mouthH * 0.25f),
            mouthW * 0.6f, mouthH * 0.35f
        );
        pRT->FillEllipse(&tongue, m_pBrushTongue);
    } else if (m_state == MascotState::FinalForm) {
        // Huge satisfied smile
        float mouthW = 34.0f * scale;
        float mouthH = 18.0f * scale;
        D2D1_ROUNDED_RECT mouthRect = D2D1::RoundedRect(
            D2D1::RectF(cx - mouthW, mouthY - 4.0f * scale, cx + mouthW, mouthY + mouthH),
            mouthH * 0.5f, mouthH * 0.5f
        );
        pRT->FillRoundedRectangle(&mouthRect, m_pBrushMouthInside);
        pRT->DrawRoundedRectangle(&mouthRect, m_pBrushBlack, 2.5f * scale);

        D2D1_ELLIPSE tongue = D2D1::Ellipse(
            D2D1::Point2F(cx, mouthY + mouthH * 0.5f),
            mouthW * 0.5f, mouthH * 0.4f
        );
        pRT->FillEllipse(&tongue, m_pBrushTongue);
    } else {
        // Idle happy smile arc
        D2D1_ELLIPSE smile = D2D1::Ellipse(
            D2D1::Point2F(cx, mouthY),
            18.0f * scale, 8.0f * scale
        );
        pRT->DrawEllipse(&smile, m_pBrushBlack, 2.5f * scale);
    }
}

void MascotRenderer::RenderRamParticles(ID2D1RenderTarget* pRT) {
    for (const auto& p : m_ramParticles) {
        if (!p.active) continue;

        D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Rotation(
            p.rotation * 180.0f / 3.14159f,
            D2D1::Point2F(p.x, p.y)
        );
        pRT->SetTransform(transform);

        float pw = 36.0f * p.scale;
        float ph = 14.0f * p.scale;
        D2D1_ROUNDED_RECT stick = D2D1::RoundedRect(
            D2D1::RectF(p.x - pw * 0.5f, p.y - ph * 0.5f, p.x + pw * 0.5f, p.y + ph * 0.5f),
            2.0f, 2.0f
        );
        pRT->FillRoundedRectangle(&stick, m_pBrushGreenDark);
        pRT->DrawRoundedRectangle(&stick, m_pBrushGreenLight, 1.5f);

        // Gold pins along bottom edge
        D2D1_RECT_F pins = D2D1::RectF(p.x - pw * 0.45f, p.y + ph * 0.30f, p.x + pw * 0.45f, p.y + ph * 0.50f);
        pRT->FillRectangle(&pins, m_pBrushGold);

        // 3 Black DRAM chips
        float chipW = 7.0f * p.scale;
        float chipH = 6.0f * p.scale;
        float startChipX = p.x - pw * 0.35f;
        for (int c = 0; c < 3; ++c) {
            D2D1_RECT_F chipRect = D2D1::RectF(
                startChipX + c * (chipW + 3.0f * p.scale),
                p.y - ph * 0.35f,
                startChipX + c * (chipW + 3.0f * p.scale) + chipW,
                p.y - ph * 0.35f + chipH
            );
            pRT->FillRectangle(&chipRect, m_pBrushChip);
        }
    }
    pRT->SetTransform(D2D1::Matrix3x2F::Identity());
}

void MascotRenderer::RenderCrumbs(ID2D1RenderTarget* pRT) {
    if (!m_pBitTextFormat) return;

    for (const auto& c : m_crumbs) {
        if (!c.active) continue;

        WCHAR wtext[2];
        wtext[0] = static_cast<WCHAR>(c.text[0]);
        wtext[1] = L'\0';

        D2D1_RECT_F textRect = D2D1::RectF(c.x - 10.0f, c.y - 10.0f, c.x + 10.0f, c.y + 10.0f);
        m_pBrushGreenLight->SetOpacity(c.life);
        pRT->DrawText(
            wtext, 1,
            m_pBitTextFormat,
            &textRect,
            m_pBrushGreenLight
        );
    }
    m_pBrushGreenLight->SetOpacity(1.0f);
}

void MascotRenderer::Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds) {
    if (!pRT || !m_pBrushGreen) return;

    float cx = (bounds.left + bounds.right) * 0.5f;
    float cy = (bounds.top + bounds.bottom) * 0.5f;

    // Gentle floating breathing bob
    float floatOffset = std::sin(m_animTime * 2.5f) * 6.0f;
    cy += floatOffset;

    // Burp shockwave ring
    if (m_burpRingRadius > 0.0f) {
        float alpha = 1.0f - (m_burpRingRadius / 180.0f);
        m_pBrushGreenLight->SetOpacity(alpha * 0.6f);
        D2D1_ELLIPSE ring = D2D1::Ellipse(D2D1::Point2F(cx, cy), m_burpRingRadius, m_burpRingRadius * 0.6f);
        pRT->DrawEllipse(&ring, m_pBrushGreenLight, 3.0f);
        m_pBrushGreenLight->SetOpacity(1.0f);
    }

    // Celebrate aura in Final Form
    if (m_state == MascotState::FinalForm) {
        float pulse = 1.0f + 0.08f * std::sin(m_animTime * 6.0f);
        D2D1_ELLIPSE aura = D2D1::Ellipse(D2D1::Point2F(cx, cy), 140.0f * pulse, 140.0f * pulse);
        m_pBrushGreenLight->SetOpacity(0.15f);
        pRT->FillEllipse(&aura, m_pBrushGreenLight);
        m_pBrushGreenLight->SetOpacity(1.0f);
    }

    // Shrink the whole body as the belly grows so the mascot stays inside its panel.
    float fit = 1.0f / (1.0f + m_currentBellyScale * 0.72f);
    float mascotBaseScale = 1.12f * fit;
    float wobble = (m_state == MascotState::Eating || m_state == MascotState::AlmostFull) ? 1.0f : 0.2f;

    RenderLimbs(pRT, cx, cy, mascotBaseScale);
    RenderKnotBody(pRT, cx, cy, mascotBaseScale, wobble);
    RenderEyes(pRT, cx, cy, mascotBaseScale);
    RenderMouth(pRT, cx, cy, mascotBaseScale);

    // Particles
    RenderRamParticles(pRT);
    RenderCrumbs(pRT);
}
