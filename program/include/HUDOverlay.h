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

    enum class TicketMark {
        None = 0, Taxi, Bus, Metro, Water, Black, DoubleMove
    };

    struct TicketSlot {
        Color color{ -1.f,-1.f,-1.f,-1.f };
        bool  used = false;
        TicketMark mark = TicketMark::None;   // NOWE: co pokazać zamiast numeru
    };

    // used tickets
    void SetSlotMark(int round_1_to_24, TicketMark mark, bool markUsed = true);



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

        // Layout top-bar
        static constexpr float k_PillsGapPx = 12.0f;
        static constexpr float k_PillsPaddingXPx = 14.0f;
        static constexpr float k_PillsPaddingYPx = 6.0f;
        static constexpr float k_SlotNumberDeltaYPx = -3.0f;
        static constexpr float k_CameraButtonScale = 0.75f;

        float pillsGapPx = k_PillsGapPx;
        float pillsPadXPx = k_PillsPaddingXPx;
        float pillsPadYPx = k_PillsPaddingYPx;
        float slotNumberDYPx = k_SlotNumberDeltaYPx;
    };



    bool InitHUD();
    void ShutdownHUD();

    void SetViewport(int width, int height);

    void SetHUDStyle(const HUDStyle& style);
    void SetTicketStates(const std::vector<TicketSlot>& slots);
    void SetTopBar(const std::vector<std::string>& vec_Labels = {},
        const std::vector<Color>& vec_PillColors = {},
        const std::vector<int>& vec_Counts = {});
    void SetRound(int round_1_to_24);

    void RenderHUD(Core::Application* p_App);

    void DrawRoundedRectScreen(float x0, float y0, float x1, float y1, Color c, int radiusPx, Core::Application* p_App);

    void DrawTextCenteredPx(const std::string& s_Text, float f_X0_px, float f_Y0_px, float f_X1_px, float f_Y1_px, Color col, Core::Application* p_App, float f_DeltaYPx = 0.0f);

    void LoadCameraIconPNG(const char* path, Core::Application* p_App);
    void LoadPauseIconPNG(const char* path, Core::Application* p_App);
    void SetCameraToggleCallback(std::function<void()> cb);
    void SetPauseCallback(std::function<void()> cb);
    void HandleMouseClick(int x_px, int y_px);
    void ShowPausedModal(bool show);

    void SetPausedResumeCallback(std::function<void()> cb);
    void SetPausedDebugCallback(std::function<void()> cb);
    void SetPausedMenuCallback(std::function<void()> cb);

    void HandleMouseMotion(int x_px, int y_px);
    void SetPausedDebugState(bool enabled);


} // namespace UI
} // namespace ScotlandYard
