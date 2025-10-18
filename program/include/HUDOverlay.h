#pragma once
#include <vector>
#include <string>
#include <functional>

namespace ScotlandYard { namespace Core { class Application; } }



namespace ScotlandYard {
namespace UI {

    struct Color { float r, g, b, a; };

    constexpr float k_DefaultViewportWidth = 1280.0f;
    constexpr float k_DefaultViewportHeight = 720.0f;
    constexpr int k_TicketSlotCount = 24;
    constexpr float k_DefaultInsetYMax = 0.20f;
    constexpr float k_DefaultInsetXMax = 0.45f;


    struct HUDStyle {
        // vertical positions of HUD (NDC: +1 up)
        float topY0 = 0.90f, topY1 = 1.0f;   // upper
        float botY0 = -0.98f, botY1 = -0.85f; // footer

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
        float pillsGapPx = 12.0f;
        float pillsPadXPx = 14.0f;
        float pillsPadYPx = 6.0f;

        float slotNumberDYPx = -3.0f;
    };

    struct TicketSlot {
        Color color = { -1.f, -1.f, -1.f, -1.f };
        bool  used = false;
    };



    bool InitHUD();
    void ShutdownHUD();

    void SetViewport(int width, int height);

    void SetHUDStyle(const HUDStyle& style);
    void SetTicketStates(const std::vector<TicketSlot>& slots);
    void SetTopBar(const std::vector<std::string>& labels,
        const std::vector<Color>& pillColors,
        const std::vector<int>& counts);
    void SetRound(int round_1_to_24);

    void RenderHUD(Core::Application* p_App);

    void DrawRoundedRectScreen(float x0, float y0, float x1, float y1, Color c, int radiusPx, Core::Application* p_App);

    void LoadCameraIconPNG(const char* path, Core::Application* p_App);
    void SetCameraToggleCallback(std::function<void()> cb);
    void HandleMouseClick(int x_px, int y_px);


} // namespace UI
} // namespace ScotlandYard
