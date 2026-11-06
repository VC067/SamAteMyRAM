#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include "memory_manager.h"
#include "mascot.h"

struct UIButton {
    D2D1_RECT_F rect;
    std::wstring text;
    bool isHovered = false;
    bool isPressed = false;
    bool isVisible = true;
    bool isPrimary = false;
    bool isDanger = false;
    bool isSuccess = false;
};

class UIRenderer {
public:
    UIRenderer();
    ~UIRenderer();

    bool Initialize(HWND hWnd, ID2D1Factory* pD2DFactory, IDWriteFactory* pDWriteFactory);
    void Resize(UINT width, UINT height);
    void Update(float deltaTime, const MemoryStats& stats);
    void Render(MascotRenderer& mascot, const MemoryStats& stats);

    // Hit Testing & Mouse input
    void OnMouseMove(float x, float y);
    void OnMouseDown(float x, float y);
    void OnMouseUp(float x, float y, MemoryManager& memMgr, MascotRenderer& mascot, HWND hWnd);

    ID2D1HwndRenderTarget* GetRenderTarget() { return m_pHwndRT; }
    IDWriteFactory* GetDWriteFactory() { return m_pDWriteFactory; }

    bool IsAboutOpen() const { return m_showAboutDialog; }
    void CloseAbout() { m_showAboutDialog = false; }
    void ReleaseGraphics() { DiscardDeviceResources(); }

private:
    void CreateDeviceResources();
    void DiscardDeviceResources();
    void Layout(const MemoryStats& stats);
    float PixelsToDips(float px) const;

    void DrawHeader(float width);
    void DrawStatsCard(float x, float y, float w, float h, const MemoryStats& stats);
    void DrawProgressBar(float x, float y, float w, float h, float progress);
    void DrawQuoteBanner(float x, float y, float w, float h);
    void DrawControls(float x, float y, float w, float h, const MemoryStats& stats);
    void DrawButton(const UIButton& btn, ID2D1SolidColorBrush* pBgBrush, ID2D1SolidColorBrush* pBorderBrush, ID2D1SolidColorBrush* pTextBrush);
    void DrawAboutModal(float width, float height);

    HWND m_hWnd;
    ID2D1Factory* m_pD2DFactory;
    IDWriteFactory* m_pDWriteFactory;
    ID2D1HwndRenderTarget* m_pHwndRT;

    // Direct2D Brushes
    ID2D1SolidColorBrush* m_pBrushBg;
    ID2D1SolidColorBrush* m_pBrushCardBg;
    ID2D1SolidColorBrush* m_pBrushCardBorder;
    ID2D1SolidColorBrush* m_pBrushTextPrimary;
    ID2D1SolidColorBrush* m_pBrushTextSecondary;
    ID2D1SolidColorBrush* m_pBrushGreenAccent;
    ID2D1SolidColorBrush* m_pBrushGreenHover;
    ID2D1SolidColorBrush* m_pBrushRedAccent;
    ID2D1SolidColorBrush* m_pBrushRedHover;
    ID2D1SolidColorBrush* m_pBrushBlueAccent;
    ID2D1SolidColorBrush* m_pBrushOverlay;

    // DirectWrite Formats
    IDWriteTextFormat* m_pFontTitle;
    IDWriteTextFormat* m_pFontSubtitle;
    IDWriteTextFormat* m_pFontStatLabel;
    IDWriteTextFormat* m_pFontStatValue;
    IDWriteTextFormat* m_pFontButton;
    IDWriteTextFormat* m_pFontSmall;
    IDWriteTextFormat* m_pFontQuote;

    // UI Buttons
    UIButton m_btnPrimaryAction;
    UIButton m_btnEmergencyReclaim;
    UIButton m_btnSpeedSlow;
    UIButton m_btnSpeedNormal;
    UIButton m_btnSpeedFast;
    UIButton m_btnSafe4GB;
    UIButton m_btnExp1GB;
    UIButton m_btnSoundToggle;
    UIButton m_btnAbout;
    UIButton m_btnAboutClose;

    // Quotes
    std::vector<std::wstring> m_quotes;
    int m_currentQuoteIndex;
    float m_quoteTimer;
    float m_quoteAlpha;

    float m_progressShimmer;
    float m_pulseTimer;
    bool m_showAboutDialog;
    bool m_recreateMascot;
    UINT m_width;
    UINT m_height;

    D2D1_RECT_F m_mascotRect;
    D2D1_RECT_F m_quoteRect;
    D2D1_RECT_F m_statsRect;
    D2D1_RECT_F m_progressRect;
    D2D1_RECT_F m_aboutPanelRect;
    MemoryStats m_cachedStats;
};
