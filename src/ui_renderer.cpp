#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ui_renderer.h"
#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

namespace {

template <typename T>
void SafeRelease(T*& ptr) {
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

bool Hit(const D2D1_RECT_F& rect, float x, float y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

std::wstring FormatGB(uint64_t bytes) {
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    std::wostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(1);
    stream << gb << L" GB";
    return stream.str();
}

std::wstring Widen(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

D2D1_COLOR_F Rgba(float r, float g, float b, float a = 1.0f) {
    return D2D1::ColorF(r, g, b, a);
}

} // namespace

UIRenderer::UIRenderer()
    : m_hWnd(nullptr),
      m_pD2DFactory(nullptr),
      m_pDWriteFactory(nullptr),
      m_pHwndRT(nullptr),
      m_pBrushBg(nullptr),
      m_pBrushCardBg(nullptr),
      m_pBrushCardBorder(nullptr),
      m_pBrushTextPrimary(nullptr),
      m_pBrushTextSecondary(nullptr),
      m_pBrushGreenAccent(nullptr),
      m_pBrushGreenHover(nullptr),
      m_pBrushRedAccent(nullptr),
      m_pBrushRedHover(nullptr),
      m_pBrushBlueAccent(nullptr),
      m_pBrushOverlay(nullptr),
      m_pFontTitle(nullptr),
      m_pFontSubtitle(nullptr),
      m_pFontStatLabel(nullptr),
      m_pFontStatValue(nullptr),
      m_pFontButton(nullptr),
      m_pFontSmall(nullptr),
      m_pFontQuote(nullptr),
      m_currentQuoteIndex(0),
      m_quoteTimer(0.0f),
      m_quoteAlpha(1.0f),
      m_progressShimmer(0.0f),
      m_pulseTimer(0.0f),
      m_showAboutDialog(false),
      m_recreateMascot(false),
      m_width(0),
      m_height(0),
      m_mascotRect(D2D1::RectF()),
      m_quoteRect(D2D1::RectF()),
      m_statsRect(D2D1::RectF()),
      m_progressRect(D2D1::RectF()),
      m_aboutPanelRect(D2D1::RectF()) {
    m_quotes = {
        L"8 GB? That's an appetizer.",
        L"Your browser can survive without that.",
        L"MORE RAM.",
        L"Delicious silicon.",
        L"I was promised unlimited compute.",
        L"16 GB is the new minimum.",
        L"Your sacrifice has been accepted.",
        L"Is that all you've got?",
        L"One more gigabyte...",
        L"I regret nothing."
    };

    m_btnPrimaryAction.text = L"OFFER SACRIFICE";
    m_btnPrimaryAction.isPrimary = true;
    m_btnEmergencyReclaim.text = L"EMERGENCY RELEASE";
    m_btnEmergencyReclaim.isDanger = true;
    m_btnSpeedSlow.text = L"Slow";
    m_btnSpeedNormal.text = L"Normal";
    m_btnSpeedFast.text = L"Fast";
    m_btnSafe4GB.text = L"4 GB safe";
    m_btnExp1GB.text = L"1 GB experimental";
    m_btnSoundToggle.text = L"Sound On";
    m_btnAbout.text = L"About";
    m_btnAboutClose.text = L"Close";
}

UIRenderer::~UIRenderer() {
    DiscardDeviceResources();
}

float UIRenderer::PixelsToDips(float px) const {
    float dpi = 96.0f;
    if (m_pHwndRT) {
        float dpiY = 96.0f;
        m_pHwndRT->GetDpi(&dpi, &dpiY);
    } else if (m_hWnd) {
        const UINT windowDpi = GetDpiForWindow(m_hWnd);
        if (windowDpi != 0) {
            dpi = static_cast<float>(windowDpi);
        }
    }
    if (dpi < 1.0f) {
        dpi = 96.0f;
    }
    return px * 96.0f / dpi;
}

bool UIRenderer::Initialize(HWND hWnd, ID2D1Factory* pD2DFactory, IDWriteFactory* pDWriteFactory) {
    m_hWnd = hWnd;
    m_pD2DFactory = pD2DFactory;
    m_pDWriteFactory = pDWriteFactory;

    RECT client{};
    GetClientRect(hWnd, &client);
    m_width = static_cast<UINT>(std::max<LONG>(0, client.right - client.left));
    m_height = static_cast<UINT>(std::max<LONG>(0, client.bottom - client.top));

    CreateDeviceResources();
    Layout(MemoryStats{});
    return m_pHwndRT != nullptr;
}

void UIRenderer::Resize(UINT width, UINT height) {
    m_width = width;
    m_height = height;
    if (!m_pHwndRT || width < 2 || height < 2) {
        return;
    }

    UINT dpi = m_hWnd ? GetDpiForWindow(m_hWnd) : 96;
    if (dpi == 0) {
        dpi = 96;
    }
    m_pHwndRT->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    m_pHwndRT->Resize(D2D1::SizeU(width, height));
}

void UIRenderer::CreateDeviceResources() {
    if (m_pHwndRT || !m_pD2DFactory || !m_hWnd) {
        return;
    }

    RECT client{};
    GetClientRect(m_hWnd, &client);
    m_width = static_cast<UINT>(std::max<LONG>(1, client.right - client.left));
    m_height = static_cast<UINT>(std::max<LONG>(1, client.bottom - client.top));

    UINT dpi = GetDpiForWindow(m_hWnd);
    if (dpi == 0) {
        dpi = 96;
    }

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    rtProps.dpiX = static_cast<float>(dpi);
    rtProps.dpiY = static_cast<float>(dpi);
    rtProps.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN);

    const D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
        m_hWnd,
        D2D1::SizeU(m_width, m_height),
        D2D1_PRESENT_OPTIONS_NONE);

    HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(rtProps, hwndProps, &m_pHwndRT);
    if (FAILED(hr) || !m_pHwndRT) {
        return;
    }

    m_pHwndRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    m_pHwndRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    m_pHwndRT->CreateSolidColorBrush(Rgba(0.078f, 0.086f, 0.098f), &m_pBrushBg);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.122f, 0.133f, 0.153f), &m_pBrushCardBg);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.220f, 0.235f, 0.255f), &m_pBrushCardBorder);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.950f, 0.960f, 0.955f), &m_pBrushTextPrimary);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.620f, 0.660f, 0.690f), &m_pBrushTextSecondary);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.063f, 0.639f, 0.498f), &m_pBrushGreenAccent);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.180f, 0.780f, 0.620f), &m_pBrushGreenHover);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.820f, 0.280f, 0.280f), &m_pBrushRedAccent);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.930f, 0.400f, 0.380f), &m_pBrushRedHover);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.350f, 0.550f, 0.950f), &m_pBrushBlueAccent);
    m_pHwndRT->CreateSolidColorBrush(Rgba(0.020f, 0.024f, 0.030f, 0.72f), &m_pBrushOverlay);

    auto makeFont = [&](float size, DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STYLE style,
                        DWRITE_TEXT_ALIGNMENT align, DWRITE_PARAGRAPH_ALIGNMENT para,
                        DWRITE_WORD_WRAPPING wrap, IDWriteTextFormat** out) {
        if (!m_pDWriteFactory) {
            return;
        }
        m_pDWriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr, weight, style, DWRITE_FONT_STRETCH_NORMAL,
            size, L"en-us", out);
        if (*out) {
            (*out)->SetTextAlignment(align);
            (*out)->SetParagraphAlignment(para);
            (*out)->SetWordWrapping(wrap);
        }
    };

    makeFont(28.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
             DWRITE_WORD_WRAPPING_NO_WRAP, &m_pFontTitle);
    makeFont(14.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
             DWRITE_WORD_WRAPPING_NO_WRAP, &m_pFontSubtitle);
    makeFont(14.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
             DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
             DWRITE_WORD_WRAPPING_NO_WRAP, &m_pFontStatLabel);
    makeFont(15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
             DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
             DWRITE_WORD_WRAPPING_NO_WRAP, &m_pFontStatValue);
    makeFont(15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
             DWRITE_WORD_WRAPPING_NO_WRAP, &m_pFontButton);
    makeFont(13.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
             DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
             DWRITE_WORD_WRAPPING_WRAP, &m_pFontSmall);
    makeFont(15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_ITALIC,
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
             DWRITE_WORD_WRAPPING_NO_WRAP, &m_pFontQuote);
}

void UIRenderer::DiscardDeviceResources() {
    SafeRelease(m_pBrushBg);
    SafeRelease(m_pBrushCardBg);
    SafeRelease(m_pBrushCardBorder);
    SafeRelease(m_pBrushTextPrimary);
    SafeRelease(m_pBrushTextSecondary);
    SafeRelease(m_pBrushGreenAccent);
    SafeRelease(m_pBrushGreenHover);
    SafeRelease(m_pBrushRedAccent);
    SafeRelease(m_pBrushRedHover);
    SafeRelease(m_pBrushBlueAccent);
    SafeRelease(m_pBrushOverlay);
    SafeRelease(m_pFontTitle);
    SafeRelease(m_pFontSubtitle);
    SafeRelease(m_pFontStatLabel);
    SafeRelease(m_pFontStatValue);
    SafeRelease(m_pFontButton);
    SafeRelease(m_pFontSmall);
    SafeRelease(m_pFontQuote);
    SafeRelease(m_pHwndRT);
}

void UIRenderer::Layout(const MemoryStats& stats) {
    float width = 1.0f;
    float height = 1.0f;
    if (m_pHwndRT) {
        const D2D1_SIZE_F dipSize = m_pHwndRT->GetSize();
        width = std::max(1.0f, dipSize.width);
        height = std::max(1.0f, dipSize.height);
    } else {
        width = std::max(1.0f, PixelsToDips(static_cast<float>(m_width)));
        height = std::max(1.0f, PixelsToDips(static_cast<float>(m_height)));
    }

    const float margin = 16.0f;
    const float columnW = std::min(std::max(120.0f, width - margin * 2.0f), 680.0f);
    const float x0 = (width - columnW) * 0.5f;
    const float x1 = x0 + columnW;
    const float gap = 5.0f;

    const bool holding = stats.isAccepted || stats.appAllocatedBytes > 0;
    if (stats.isSacrificing) {
        m_btnPrimaryAction.text = L"STOP THE SACRIFICE";
        m_btnPrimaryAction.isPrimary = false;
        m_btnPrimaryAction.isDanger = true;
        m_btnPrimaryAction.isSuccess = false;
    } else if (holding) {
        m_btnPrimaryAction.text = L"RECLAIM YOUR RAM";
        m_btnPrimaryAction.isPrimary = false;
        m_btnPrimaryAction.isDanger = false;
        m_btnPrimaryAction.isSuccess = true;
    } else {
        m_btnPrimaryAction.text = L"OFFER SACRIFICE";
        m_btnPrimaryAction.isPrimary = true;
        m_btnPrimaryAction.isDanger = false;
        m_btnPrimaryAction.isSuccess = false;
    }

    m_btnSoundToggle.text = SoundEffects::Instance().IsMuted() ? L"Sound Off" : L"Sound On";

    float y = height - 10.0f;
    const float rowH = 30.0f;
    y -= rowH;
    const float halfW = (columnW - gap) * 0.5f;
    m_btnSoundToggle.rect = D2D1::RectF(x0, y, x0 + halfW, y + rowH);
    m_btnAbout.rect = D2D1::RectF(x0 + halfW + gap, y, x1, y + rowH);

    y -= gap + rowH;
    const float third = (columnW - gap * 2.0f) / 3.0f;
    m_btnSpeedSlow.rect = D2D1::RectF(x0, y, x0 + third, y + rowH);
    m_btnSpeedNormal.rect = D2D1::RectF(x0 + third + gap, y, x0 + third * 2.0f + gap, y + rowH);
    m_btnSpeedFast.rect = D2D1::RectF(x0 + third * 2.0f + gap * 2.0f, y, x1, y + rowH);

    y -= gap + rowH;
    m_btnSafe4GB.rect = D2D1::RectF(x0, y, x0 + halfW, y + rowH);
    m_btnExp1GB.rect = D2D1::RectF(x0 + halfW + gap, y, x1, y + rowH);

    y -= gap + 34.0f;
    m_btnEmergencyReclaim.rect = D2D1::RectF(x0, y, x1, y + 34.0f);

    y -= gap + 44.0f;
    m_btnPrimaryAction.rect = D2D1::RectF(x0, y, x1, y + 44.0f);

    y -= 8.0f + 14.0f;
    m_progressRect = D2D1::RectF(x0, y, x1, y + 14.0f);

    y -= gap + 96.0f;
    m_statsRect = D2D1::RectF(x0, y, x1, y + 96.0f);

    y -= gap + 40.0f;
    m_quoteRect = D2D1::RectF(x0, y, x1, y + 40.0f);

    const float mascotTop = 68.0f;
    const float mascotBottom = std::max(mascotTop + 96.0f, m_quoteRect.top - gap);
    m_mascotRect = D2D1::RectF(x0, mascotTop, x1, mascotBottom);

    const float panelW = std::min(460.0f, width - 40.0f);
    const float panelH = std::min(430.0f, height - 40.0f);
    const float px = (width - panelW) * 0.5f;
    const float py = (height - panelH) * 0.5f;
    m_aboutPanelRect = D2D1::RectF(px, py, px + panelW, py + panelH);
    m_btnAboutClose.rect = D2D1::RectF(px + 22.0f, py + panelH - 58.0f, px + panelW - 22.0f, py + panelH - 18.0f);
    m_btnAboutClose.isVisible = m_showAboutDialog;
}

void UIRenderer::Update(float deltaTime, const MemoryStats& stats) {
    m_cachedStats = stats;
    m_progressShimmer += deltaTime;
    m_pulseTimer += deltaTime;
    m_quoteTimer += deltaTime;

    const float interval = stats.isSacrificing ? 3.2f : 5.5f;
    if (m_quoteTimer >= interval && !m_quotes.empty()) {
        m_quoteTimer = 0.0f;
        m_currentQuoteIndex = (m_currentQuoteIndex + 1) % static_cast<int>(m_quotes.size());
        m_quoteAlpha = 0.0f;
    }
    if (m_quoteAlpha < 1.0f) {
        m_quoteAlpha = std::min(1.0f, m_quoteAlpha + deltaTime * 2.8f);
    }
}

void UIRenderer::DrawHeader(float width) {
    if (!m_pFontTitle || !m_pFontSubtitle) {
        return;
    }
    D2D1_RECT_F title = D2D1::RectF(0.0f, 8.0f, width, 42.0f);
    m_pHwndRT->DrawText(L"Altman.exe", 10, m_pFontTitle, title, m_pBrushTextPrimary);
    D2D1_RECT_F subtitle = D2D1::RectF(16.0f, 42.0f, width - 16.0f, 66.0f);
    m_pHwndRT->DrawText(L"Your memory is a small price to pay.", 36, m_pFontSubtitle, subtitle, m_pBrushTextSecondary);

    D2D1_RECT_F rule = D2D1::RectF(width * 0.5f - 28.0f, 66.0f, width * 0.5f + 28.0f, 68.0f);
    m_pHwndRT->FillRectangle(rule, m_pBrushGreenAccent);
}

void UIRenderer::DrawStatsCard(float, float, float, float, const MemoryStats& stats) {
    if (!m_pFontStatLabel || !m_pFontStatValue) {
        return;
    }

    D2D1_ROUNDED_RECT card = D2D1::RoundedRect(m_statsRect, 14.0f, 14.0f);
    m_pHwndRT->FillRoundedRectangle(card, m_pBrushCardBg);
    m_pHwndRT->DrawRoundedRectangle(card, m_pBrushCardBorder, 1.0f);

    const wchar_t* labels[] = {
        L"RAM Devoured",
        L"Available RAM",
        L"Total System RAM",
        L"Sacrifice Progress"
    };

    int percent = static_cast<int>(stats.sacrificeProgress * 100.0f + 0.5f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    const std::wstring values[] = {
        FormatGB(stats.appAllocatedBytes),
        FormatGB(stats.availPhysBytes),
        FormatGB(stats.totalPhysBytes),
        std::to_wstring(percent) + L"%"
    };

    const float rowH = (m_statsRect.bottom - m_statsRect.top - 16.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        const float top = m_statsRect.top + 8.0f + rowH * static_cast<float>(i);
        const float valueLeft = std::max(m_statsRect.left + 130.0f, m_statsRect.right - 148.0f);
        D2D1_RECT_F labelRect = D2D1::RectF(m_statsRect.left + 14.0f, top, valueLeft - 8.0f, top + rowH);
        D2D1_RECT_F valueRect = D2D1::RectF(valueLeft, top, m_statsRect.right - 14.0f, top + rowH);
        m_pHwndRT->DrawText(labels[i], static_cast<UINT32>(wcslen(labels[i])), m_pFontStatLabel, labelRect, m_pBrushTextSecondary);
        m_pHwndRT->DrawText(values[i].c_str(), static_cast<UINT32>(values[i].size()), m_pFontStatValue, valueRect,
                            i == 0 ? m_pBrushGreenAccent : m_pBrushTextPrimary);
    }
}

void UIRenderer::DrawProgressBar(float, float, float, float, float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    D2D1_ROUNDED_RECT track = D2D1::RoundedRect(m_progressRect, 8.0f, 8.0f);
    m_pHwndRT->FillRoundedRectangle(track, m_pBrushCardBg);
    m_pHwndRT->DrawRoundedRectangle(track, m_pBrushCardBorder, 1.0f);

    const float innerLeft = m_progressRect.left + 2.0f;
    const float innerRight = m_progressRect.right - 2.0f;
    const float fillRight = innerLeft + (innerRight - innerLeft) * progress;
    if (fillRight - innerLeft > 2.0f) {
        D2D1_RECT_F fill = D2D1::RectF(innerLeft, m_progressRect.top + 2.0f, fillRight, m_progressRect.bottom - 2.0f);
        D2D1_ROUNDED_RECT fillRound = D2D1::RoundedRect(fill, 6.0f, 6.0f);
        m_pHwndRT->FillRoundedRectangle(fillRound, m_pBrushGreenAccent);

        const float span = fill.right - fill.left;
        const float shimmerW = std::max(18.0f, span * 0.22f);
        const float travel = std::fmod(m_progressShimmer * 0.85f, 1.0f);
        const float shimmerX = fill.left + (span + shimmerW) * travel - shimmerW;
        D2D1_RECT_F shimmer = D2D1::RectF(
            std::max(fill.left, shimmerX),
            fill.top,
            std::min(fill.right, shimmerX + shimmerW),
            fill.bottom);
        if (shimmer.right > shimmer.left) {
            m_pBrushTextPrimary->SetOpacity(0.22f);
            m_pHwndRT->FillRectangle(shimmer, m_pBrushTextPrimary);
            m_pBrushTextPrimary->SetOpacity(1.0f);
        }
    }
}

void UIRenderer::DrawQuoteBanner(float, float, float, float) {
    if (!m_pFontQuote || !m_pFontSmall) {
        return;
    }

    std::wstring line;
    ID2D1SolidColorBrush* brush = m_pBrushGreenAccent;
    IDWriteTextFormat* font = m_pFontQuote;

    if (m_cachedStats.hadError) {
        line = Widen(m_cachedStats.errorMessage.empty() ? "Something went wrong." : m_cachedStats.errorMessage);
        brush = m_pBrushRedAccent;
        font = m_pFontSmall;
    } else if (m_cachedStats.isAccepted && m_cachedStats.appAllocatedBytes > 0) {
        const std::wstring headline = L"YOUR SACRIFICE HAS BEEN ACCEPTED.";
        std::wstring detail = L"Devoured " + FormatGB(m_cachedStats.appAllocatedBytes);
        if (!m_cachedStats.statusMessage.empty()) {
            detail += L"  ·  ";
            detail += Widen(m_cachedStats.statusMessage);
        }
        const float mid = (m_quoteRect.top + m_quoteRect.bottom) * 0.5f;
        D2D1_RECT_F top = m_quoteRect;
        top.bottom = mid;
        D2D1_RECT_F bottom = m_quoteRect;
        bottom.top = mid;
        if (m_pFontButton) {
            m_pHwndRT->DrawText(headline.c_str(), static_cast<UINT32>(headline.size()), m_pFontButton, top, m_pBrushGreenAccent);
        }
        if (m_pFontSmall) {
            m_pHwndRT->DrawText(detail.c_str(), static_cast<UINT32>(detail.size()), m_pFontSmall, bottom, m_pBrushTextSecondary);
        }
        return;
    } else if (!m_cachedStats.statusMessage.empty() && !m_cachedStats.isSacrificing) {
        line = Widen(m_cachedStats.statusMessage);
        brush = m_pBrushTextSecondary;
        font = m_pFontSmall;
    } else if (!m_quotes.empty()) {
        line = m_quotes[static_cast<size_t>(m_currentQuoteIndex) % m_quotes.size()];
        brush = m_pBrushGreenAccent;
        font = m_pFontQuote;
        brush->SetOpacity(m_quoteAlpha);
    }

    if (!line.empty()) {
        m_pHwndRT->DrawText(line.c_str(), static_cast<UINT32>(line.size()), font, m_quoteRect, brush);
    }
    if (brush == m_pBrushGreenAccent) {
        brush->SetOpacity(1.0f);
    }
}

void UIRenderer::DrawButton(const UIButton& btn, ID2D1SolidColorBrush* pBgBrush,
                            ID2D1SolidColorBrush* pBorderBrush, ID2D1SolidColorBrush* pTextBrush) {
    if (!btn.isVisible || !m_pFontButton || !pBgBrush || !pBorderBrush || !pTextBrush) {
        return;
    }

    D2D1_ROUNDED_RECT shape = D2D1::RoundedRect(btn.rect, 10.0f, 10.0f);
    m_pHwndRT->FillRoundedRectangle(shape, pBgBrush);
    m_pHwndRT->DrawRoundedRectangle(shape, pBorderBrush, btn.isHovered ? 1.75f : 1.0f);
    m_pHwndRT->DrawText(btn.text.c_str(), static_cast<UINT32>(btn.text.size()), m_pFontButton, btn.rect, pTextBrush);
}

void UIRenderer::DrawControls(float, float, float, float, const MemoryStats& stats) {
    auto chip = [&](const UIButton& btn, bool selected, bool danger) {
        ID2D1SolidColorBrush* fill = selected ? (danger ? m_pBrushRedAccent : m_pBrushGreenAccent) : m_pBrushCardBg;
        if (btn.isHovered && !selected) {
            fill = m_pBrushCardBorder;
        }
        ID2D1SolidColorBrush* border = selected ? (danger ? m_pBrushRedHover : m_pBrushGreenHover) : m_pBrushCardBorder;
        ID2D1SolidColorBrush* text = selected ? m_pBrushTextPrimary : m_pBrushTextSecondary;
        DrawButton(btn, fill, border, text);
    };

    ID2D1SolidColorBrush* primaryFill = m_pBrushGreenAccent;
    ID2D1SolidColorBrush* primaryBorder = m_pBrushGreenHover;
    if (m_btnPrimaryAction.isDanger) {
        primaryFill = m_btnPrimaryAction.isHovered ? m_pBrushRedHover : m_pBrushRedAccent;
        primaryBorder = m_pBrushRedHover;
    } else if (m_btnPrimaryAction.isHovered || m_btnPrimaryAction.isSuccess) {
        primaryFill = m_btnPrimaryAction.isHovered ? m_pBrushGreenHover : m_pBrushGreenAccent;
    }
    DrawButton(m_btnPrimaryAction, primaryFill, primaryBorder, m_pBrushTextPrimary);

    ID2D1SolidColorBrush* emergencyFill = m_btnEmergencyReclaim.isHovered ? m_pBrushRedAccent : m_pBrushCardBg;
    DrawButton(m_btnEmergencyReclaim, emergencyFill, m_pBrushRedAccent,
               m_btnEmergencyReclaim.isHovered ? m_pBrushTextPrimary : m_pBrushRedAccent);

    chip(m_btnSpeedSlow, stats.speed == AllocationSpeed::Slow, false);
    chip(m_btnSpeedNormal, stats.speed == AllocationSpeed::Normal, false);
    chip(m_btnSpeedFast, stats.speed == AllocationSpeed::Fast, false);
    chip(m_btnSafe4GB, stats.safety == SafetyMode::Safe4GB, false);
    chip(m_btnExp1GB, stats.safety == SafetyMode::Experimental1GB, true);
    chip(m_btnSoundToggle, !SoundEffects::Instance().IsMuted(), false);
    chip(m_btnAbout, false, false);
}

void UIRenderer::DrawAboutModal(float width, float height) {
    D2D1_RECT_F overlay = D2D1::RectF(0.0f, 0.0f, width, height);
    m_pHwndRT->FillRectangle(overlay, m_pBrushOverlay);

    D2D1_ROUNDED_RECT panel = D2D1::RoundedRect(m_aboutPanelRect, 16.0f, 16.0f);
    m_pHwndRT->FillRoundedRectangle(panel, m_pBrushCardBg);
    m_pHwndRT->DrawRoundedRectangle(panel, m_pBrushGreenAccent, 1.25f);

    D2D1_RECT_F title = D2D1::RectF(m_aboutPanelRect.left, m_aboutPanelRect.top + 12.0f,
                                    m_aboutPanelRect.right, m_aboutPanelRect.top + 48.0f);
    if (m_pFontButton) {
        m_pHwndRT->DrawText(L"About Altman.exe", 16, m_pFontButton, title, m_pBrushTextPrimary);
    }

    const wchar_t* body =
        L"Altman.exe is an unofficial parody and a voluntary memory stress toy. "
        L"It commits real RAM inside this process only. Task Manager shows that usage climb. "
        L"Closing the window, or pressing Reclaim, gives it all back.\n\n"
        L"This application is not affiliated with Sam Altman or OpenAI. "
        L"The mascot is an original cartoon inspired by a familiar knot, not their product.\n\n"
        L"The default reserve leaves 4 GB of physical RAM free. Experimental mode is opt-in, "
        L"warns you first, and still stops early if Windows is under pressure.\n\n"
        L"Emergency reclaim: the Emergency Release button, Esc while this window is focused, "
        L"or Ctrl+Shift+F12 even if the window is busy.\n\n"
        L"No startup entry. No network. No administrator rights.";

    D2D1_RECT_F bodyRect = D2D1::RectF(
        m_aboutPanelRect.left + 22.0f,
        m_aboutPanelRect.top + 56.0f,
        m_aboutPanelRect.right - 22.0f,
        m_btnAboutClose.rect.top - 10.0f);
    if (m_pFontSmall) {
        m_pHwndRT->DrawText(body, static_cast<UINT32>(wcslen(body)), m_pFontSmall, bodyRect, m_pBrushTextSecondary);
    }

    DrawButton(m_btnAboutClose, m_btnAboutClose.isHovered ? m_pBrushGreenHover : m_pBrushGreenAccent,
               m_pBrushGreenHover, m_pBrushTextPrimary);
}

void UIRenderer::Render(MascotRenderer& mascot, const MemoryStats& stats) {
    m_cachedStats = stats;
    if (!m_pHwndRT) {
        CreateDeviceResources();
        m_recreateMascot = true;
    }
    if (!m_pHwndRT || m_width < 2 || m_height < 2) {
        return;
    }
    if (m_recreateMascot) {
        mascot.Initialize(m_pHwndRT, m_pDWriteFactory);
        m_recreateMascot = false;
    }

    Layout(stats);

    m_pHwndRT->BeginDraw();
    m_pHwndRT->Clear(D2D1::ColorF(0.078f, 0.086f, 0.098f));

    float width = 1.0f;
    float height = 1.0f;
    if (m_pHwndRT) {
        const D2D1_SIZE_F dipSize = m_pHwndRT->GetSize();
        width = std::max(1.0f, dipSize.width);
        height = std::max(1.0f, dipSize.height);
    } else {
        width = std::max(1.0f, PixelsToDips(static_cast<float>(m_width)));
        height = std::max(1.0f, PixelsToDips(static_cast<float>(m_height)));
    }

    DrawHeader(width);

    D2D1_ROUNDED_RECT stage = D2D1::RoundedRect(m_mascotRect, 18.0f, 18.0f);
    m_pHwndRT->FillRoundedRectangle(stage, m_pBrushCardBg);
    m_pHwndRT->DrawRoundedRectangle(stage, m_pBrushCardBorder, 1.0f);

    const float inset = 8.0f;
    D2D1_RECT_F clip = D2D1::RectF(
        m_mascotRect.left + inset,
        m_mascotRect.top + inset,
        m_mascotRect.right - inset,
        m_mascotRect.bottom - inset);
    if (clip.right > clip.left && clip.bottom > clip.top) {
        m_pHwndRT->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        mascot.Render(m_pHwndRT, clip);
        m_pHwndRT->PopAxisAlignedClip();
    }

    DrawQuoteBanner(m_quoteRect.left, m_quoteRect.top, m_quoteRect.right - m_quoteRect.left, m_quoteRect.bottom - m_quoteRect.top);
    DrawStatsCard(m_statsRect.left, m_statsRect.top, m_statsRect.right - m_statsRect.left, m_statsRect.bottom - m_statsRect.top, stats);
    DrawProgressBar(m_progressRect.left, m_progressRect.top, m_progressRect.right - m_progressRect.left,
                    m_progressRect.bottom - m_progressRect.top, stats.sacrificeProgress);
    DrawControls(0.0f, 0.0f, width, height, stats);

    if (m_showAboutDialog) {
        DrawAboutModal(width, height);
    }

    const HRESULT hr = m_pHwndRT->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        mascot.DiscardDeviceResources();
        m_recreateMascot = true;
    }
}

void UIRenderer::OnMouseMove(float x, float y) {
    const float dx = PixelsToDips(x);
    const float dy = PixelsToDips(y);
    Layout(m_cachedStats);

    UIButton* buttons[] = {
        &m_btnPrimaryAction, &m_btnEmergencyReclaim, &m_btnSpeedSlow, &m_btnSpeedNormal,
        &m_btnSpeedFast, &m_btnSafe4GB, &m_btnExp1GB, &m_btnSoundToggle, &m_btnAbout, &m_btnAboutClose
    };
    for (UIButton* button : buttons) {
        button->isHovered = button->isVisible && Hit(button->rect, dx, dy);
    }
}

void UIRenderer::OnMouseDown(float x, float y) {
    const float dx = PixelsToDips(x);
    const float dy = PixelsToDips(y);
    Layout(m_cachedStats);

    UIButton* buttons[] = {
        &m_btnPrimaryAction, &m_btnEmergencyReclaim, &m_btnSpeedSlow, &m_btnSpeedNormal,
        &m_btnSpeedFast, &m_btnSafe4GB, &m_btnExp1GB, &m_btnSoundToggle, &m_btnAbout, &m_btnAboutClose
    };
    for (UIButton* button : buttons) {
        button->isPressed = button->isVisible && Hit(button->rect, dx, dy);
    }
}

void UIRenderer::OnMouseUp(float x, float y, MemoryManager& memMgr, MascotRenderer& mascot, HWND hWnd) {
    const float dx = PixelsToDips(x);
    const float dy = PixelsToDips(y);
    Layout(m_cachedStats);

    auto pressed = [&](const UIButton& button) {
        return button.isVisible && button.isPressed && Hit(button.rect, dx, dy);
    };

    UIButton* buttons[] = {
        &m_btnPrimaryAction, &m_btnEmergencyReclaim, &m_btnSpeedSlow, &m_btnSpeedNormal,
        &m_btnSpeedFast, &m_btnSafe4GB, &m_btnExp1GB, &m_btnSoundToggle, &m_btnAbout, &m_btnAboutClose
    };
    for (UIButton* button : buttons) {
        button->isPressed = false;
    }

    if (m_showAboutDialog) {
        if (pressed(m_btnAboutClose) || !Hit(m_aboutPanelRect, dx, dy)) {
            m_showAboutDialog = false;
        }
        return;
    }

    if (pressed(m_btnPrimaryAction)) {
        if (memMgr.IsSacrificing()) {
            memMgr.StopSacrifice();
        } else if (memMgr.IsAccepted() || memMgr.GetStats().appAllocatedBytes > 0) {
            memMgr.ReclaimAll();
            mascot.TriggerDeflation();
        } else if (!memMgr.StartSacrifice()) {
            MessageBoxW(hWnd, L"The sacrifice is already running.", L"Altman.exe", MB_OK | MB_ICONINFORMATION);
        }
    } else if (pressed(m_btnEmergencyReclaim)) {
        memMgr.RequestEmergencyReclaim();
        mascot.TriggerDeflation();
    } else if (pressed(m_btnSpeedSlow)) {
        memMgr.SetSpeed(AllocationSpeed::Slow);
    } else if (pressed(m_btnSpeedNormal)) {
        memMgr.SetSpeed(AllocationSpeed::Normal);
    } else if (pressed(m_btnSpeedFast)) {
        memMgr.SetSpeed(AllocationSpeed::Fast);
    } else if (pressed(m_btnSafe4GB)) {
        memMgr.SetSafetyMode(SafetyMode::Safe4GB);
    } else if (pressed(m_btnExp1GB)) {
        const int answer = MessageBoxW(
            hWnd,
            L"Experimental mode tries to leave about 1 GB free on smaller machines, and a larger reserve on big ones.\n\n"
            L"Windows can get slow. Other programs may be pushed into the page file. "
            L"The 1 GB target is not a guarantee: the app still stops if Windows reports memory pressure, "
            L"commitment runs low, or allocation stalls.\n\n"
            L"The setting applies immediately. Continue?",
            L"Altman.exe — experimental reserve",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (answer == IDYES) {
            memMgr.SetSafetyMode(SafetyMode::Experimental1GB);
        }
    } else if (pressed(m_btnSoundToggle)) {
        SoundEffects::Instance().ToggleMute();
    } else if (pressed(m_btnAbout)) {
        m_showAboutDialog = true;
    }

    m_cachedStats = memMgr.GetStats();
}
