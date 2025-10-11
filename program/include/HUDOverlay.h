#pragma once
#include <vector>
#include <string>
#include <functional>
#include <SDL2/SDL_ttf.h>

namespace UI {

    struct Color { float r, g, b, a; };


    struct HUDStyle {
        // vertical positions of HUD (NDC: +1 up)
        float topY0 = 0.90f, topY1 = 1.0f;   // upper
        float botY0 = -0.98f, botY1 = -0.89f; // footer

        // horizontal margins
        float topInsetX = 0.22f;   // bigger value = shorter
        float botInsetX = 0.06f;

        // window padding
        float insetY = 0.02f;

        // colors
        Color barColor = { 5 / 255.f, 5 / 255.f, 36 / 255.f, 0.67f };
        Color slotColor = { 19 / 255.f,19 / 255.f, 57 / 255.f, 0.67f };
        Color textColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        // radius
        int barRadiusPx = 5;
        int slotRadiusPx = 5;

        // Layout top-bara
        float pillsGapPx = 8.0f;   // margin
        float pillsPadXPx = 14.0f;  // horizontal padding
        float pillsPadYPx = 6.0f;   // vertical padding
        float pillsFontScale = 0.75f;
    };

    struct TicketSlot {
        Color color = { -1.f, -1.f, -1.f, -1.f };
        bool  used = false; // na przyszłość
    };

    bool InitHUD();
    void ShutdownHUD();

    void SetViewport(int width, int height);

    void SetHUDStyle(const HUDStyle& style);

    // font not working yet
    void SetFont(TTF_Font* font);


    void RenderHUD();
    inline void RenderSimpleHUD() { RenderHUD(); }

} // namespace UI
