#include "HUDOverlay.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include "Application.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <array>
#include <atomic>

namespace ScotlandYard {
namespace UI {

    namespace {
    HUDStyle g_HUDStyle{};
    int g_i_ViewportWidth = 1280;
    int g_i_ViewportHeight = 720;
    GLuint g_GLuint_TexCamera = 0;
    GLuint g_GLuint_TexPause = 0;
    std::function<void()> g_fn_OnCameraToggle;
    std::function<void()> g_fn_OnPause;
    std::atomic_bool g_b_ShowPausedModal{false};
    std::atomic_bool g_b_PausedResumeBtnHover{false};
    std::atomic_bool g_b_PausedDebugBtnHover{false};
    std::atomic_bool g_b_PausedModalBtnHover{false};

    int g_i_PausedResumeBtnX0 = 0;
    int g_i_PausedResumeBtnY0 = 0;
    int g_i_PausedResumeBtnX1 = 0;
    int g_i_PausedResumeBtnY1 = 0;

    int g_i_PausedDebugBtnX0 = 0;
    int g_i_PausedDebugBtnY0 = 0;
    int g_i_PausedDebugBtnX1 = 0;
    int g_i_PausedDebugBtnY1 = 0;

    int g_i_PausedModalBtnX0 = 0;
    int g_i_PausedModalBtnY0 = 0;
    int g_i_PausedModalBtnX1 = 0;
    int g_i_PausedModalBtnY1 = 0;

    std::function<void()> g_fn_OnPausedResume;
    std::function<void()> g_fn_OnPausedDebug;
    std::function<void()> g_fn_OnPausedMenu;
    std::atomic_bool g_b_DebugEnabled{false};
        float g_f_CamBtnNdcX0 = 0.0f;
        float g_f_CamBtnNdcY0 = 0.0f;
        float g_f_CamBtnNdcX1 = 0.0f;
        float g_f_CamBtnNdcY1 = 0.0f;
    float g_f_PauseBtnNdcX0 = 0.0f;
    float g_f_PauseBtnNdcY0 = 0.0f;
    float g_f_PauseBtnNdcX1 = 0.0f;
    float g_f_PauseBtnNdcY1 = 0.0f;

        int g_i_Round = 1;

        std::vector<TicketSlot> g_vec_TicketSlots(k_TicketSlotCount);
        std::vector<int> g_vec_PillCounts;
        std::vector<std::array<float, 4>> g_vec_PillRectNDC;



        std::vector<std::string> g_vec_PillLabels = { "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus" };
        std::vector<Color> g_vec_PillColors = {
            {0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 1.0f},
            {0xE2 / 255.0f, 0x70 / 255.0f, 0x3F / 255.0f, 1.0f},
            {0xED / 255.0f, 0xD1 / 255.0f, 0x00 / 255.0f, 1.0f},
            {0xF5 / 255.0f, 0x51 / 255.0f, 0xAE / 255.0f, 1.0f},
            {0x41 / 255.0f, 0x84 / 255.0f, 0x3D / 255.0f, 1.0f},
            {0x00 / 255.0f, 0x60 / 255.0f, 0xFF / 255.0f, 1.0f}, // water
        };

        float pxToNDC(float f_Px) { return (2.0f * f_Px) / float(g_i_ViewportHeight); }

        inline float ndcX_to_px(float f_X) { return (f_X * 0.5f + 0.5f) * g_i_ViewportWidth; }
        inline float ndcY_to_px(float f_Y) { return (f_Y * 0.5f + 0.5f) * g_i_ViewportHeight; }
        inline float px_to_ndcX(float f_P) { return (f_P / float(g_i_ViewportWidth)) * 2.0f - 1.0f; }
        inline float px_to_ndcY(float f_P) { return 1.0f - (f_P / float(g_i_ViewportHeight)) * 2.0f; }

        static void drawIcon(GLuint tex, float f_X0, float f_Y0, float f_X1, float f_Y1, Core::Application* p_App) {
            if (!tex) return;

            float f_Verts[24] = {
                f_X0, f_Y0, 0, 0,  f_X1, f_Y0, 1, 0,  f_X0, f_Y1, 0, 1,
                f_X1, f_Y0, 1, 0,  f_X1, f_Y1, 1, 1,  f_X0, f_Y1, 0, 1
            };

            glUseProgram(p_App->GetHUDTextureShader());
            glUniform4f(glGetUniformLocation(p_App->GetHUDTextureShader(), "uColor"), 1, 1, 1, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glUniform1i(glGetUniformLocation(p_App->GetHUDTextureShader(), "uTex"), 0);
            glBindVertexArray(p_App->GetHUDTextureVAO());
            glBindBuffer(GL_ARRAY_BUFFER, p_App->GetHUDTextureVBO());
            glBufferData(GL_ARRAY_BUFFER, sizeof(f_Verts), f_Verts, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
        }

        void drawRoundedRect(float f_X0, float f_Y0, float f_X1, float f_Y1, Color c, float f_RadiusNDC, Core::Application* p_App) {
            const float f_Verts[] = { f_X0, f_Y0,  f_X1, f_Y0,  f_X0, f_Y1,  f_X1, f_Y0,  f_X1, f_Y1,  f_X0, f_Y1 };

            glUseProgram(p_App->GetHUDRoundedShader());
            glUniform4f(glGetUniformLocation(p_App->GetHUDRoundedShader(), "uRect"), f_X0, f_Y0, f_X1, f_Y1);
            glUniform1f(glGetUniformLocation(p_App->GetHUDRoundedShader(), "uRadius"), f_RadiusNDC);
            glUniform4f(glGetUniformLocation(p_App->GetHUDRoundedShader(), "uColor"), c.r, c.g, c.b, c.a);
            glBindVertexArray(p_App->GetHUDRoundedVAO());
            glBindBuffer(GL_ARRAY_BUFFER, p_App->GetHUDRoundedVBO());
            glBufferData(GL_ARRAY_BUFFER, sizeof(f_Verts), f_Verts, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glUseProgram(0);
        }

        float textWidthPx(const std::string& s_Text, float f_Scale, Core::Application* p_App) {
            const auto& chars = p_App->GetCharacterMap();
            float f_W = 0.0f;
            for (char c : s_Text) {
                auto it = chars.find(c);
                if (it == chars.end()) continue;
                f_W += float(it->second.m_i_Advance >> 6) * f_Scale;
            }
            return f_W;
        }


        void drawTextPx(const std::string& s_Text, float f_XPx, float f_BaselineYPx, float f_Scale,
            float f_R, float f_G, float f_B, Core::Application* p_App)
        {
            const auto& chars = p_App->GetCharacterMap();
            GLuint prog = p_App->GetTextShaderProgram();
            GLuint vao = p_App->GetTextVAO();
            GLuint vbo = p_App->GetTextVBO();

            glUseProgram(prog);
            glUniform3f(glGetUniformLocation(prog, "textColor"), f_R, f_G, f_B);
            glm::mat4 P = glm::ortho(0.0f, (float)g_i_ViewportWidth, 0.0f, (float)g_i_ViewportHeight);
            glUniformMatrix4fv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, glm::value_ptr(P));

            glActiveTexture(GL_TEXTURE0);
            glUniform1i(glGetUniformLocation(prog, "text"), 0);
            glBindVertexArray(vao);

            float f_PenX = f_XPx;
            for (char c : s_Text) {
                auto it = chars.find(c);
                if (it == chars.end()) continue;
                const ScotlandYard::Core::Character& ch = it->second;

                float f_Xpos = f_PenX + ch.m_i_BearingX * f_Scale;
                float f_Ypos = f_BaselineYPx - (ch.m_i_Height - ch.m_i_BearingY) * f_Scale;
                float f_W = ch.m_i_Width * f_Scale;
                float f_H = ch.m_i_Height * f_Scale;

                float f_Verts[6][4] = {
                    { f_Xpos,       f_Ypos,       0.0f, 1.0f },
                    { f_Xpos,       f_Ypos + f_H, 0.0f, 0.0f },
                    { f_Xpos + f_W, f_Ypos + f_H, 1.0f, 0.0f },
                    { f_Xpos,       f_Ypos,       0.0f, 1.0f },
                    { f_Xpos + f_W, f_Ypos + f_H, 1.0f, 0.0f },
                    { f_Xpos + f_W, f_Ypos,       1.0f, 1.0f },
                };

                glBindTexture(GL_TEXTURE_2D, ch.m_TextureID);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(f_Verts), f_Verts);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                f_PenX += (ch.m_i_Advance >> 6) * f_Scale;
            }
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
        }

        void drawTextCentered(const std::string& s_Text, float f_X0, float f_Y0, float f_X1, float f_Y1, Color col, Core::Application* p_App, float f_DeltaYPx = 0.0f) {
            float f_LeftPx = ndcX_to_px(f_X0);
            float f_RightPx = ndcX_to_px(f_X1);
            float f_BottomPx = ndcY_to_px(f_Y0);
            float f_TopPx = ndcY_to_px(f_Y1);

            float f_RectHPx = std::max(1.0f, f_TopPx - f_BottomPx);
            float f_TargetHPx = std::max(14.0f, f_RectHPx * 0.60f);
            float f_Scale = f_TargetHPx / 48.0f;

            float f_Baseline = f_BottomPx + (f_RectHPx * 0.20f) + f_TargetHPx * 0.8f - 10.0f + f_DeltaYPx;

            float f_TW = textWidthPx(s_Text, f_Scale, p_App);
            float f_TX = (f_LeftPx + f_RightPx) * 0.5f - f_TW * 0.5f;

            drawTextPx(s_Text, f_TX, f_Baseline, f_Scale, col.r, col.g, col.b, p_App);
        }

        
        void computeBars(float& f_TX0, float& f_TX1, float& f_TY0, float& f_TY1,
            float& f_BX0, float& f_BX1, float& f_BY0, float& f_BY1)
        {
            const float f_IY = std::clamp(g_HUDStyle.insetY, 0.0f, k_DefaultInsetYMax);
            const float f_IT = std::clamp(g_HUDStyle.topInsetX, 0.0f, k_DefaultInsetXMax);
            const float f_IB = std::clamp(g_HUDStyle.botInsetX, 0.0f, k_DefaultInsetXMax);

            f_TX0 = -1.0f + f_IT; f_TX1 = 1.0f - f_IT;
            f_TY0 = g_HUDStyle.topY0 - f_IY; f_TY1 = g_HUDStyle.topY1 - f_IY;

            f_BX0 = -1.0f + f_IB; f_BX1 = 1.0f - f_IB;
            f_BY0 = g_HUDStyle.botY0 + f_IY; f_BY1 = g_HUDStyle.botY1 + f_IY;
        }


        void drawTopBar(float f_X0, float f_Y0, float f_X1, float f_Y1, Core::Application* p_App) {
            drawRoundedRect(f_X0, f_Y0, f_X1, f_Y1, g_HUDStyle.barColor, pxToNDC(g_HUDStyle.barRadiusPx), p_App);

            const float f_PadY = pxToNDC(g_HUDStyle.pillsPadYPx);
            const float f_PadX = pxToNDC(g_HUDStyle.pillsPadXPx);
            const float f_Gap = pxToNDC(g_HUDStyle.pillsGapPx);

            float f_InnerX0 = f_X0 + f_Gap;
            float f_InnerX1 = f_X1 - f_Gap;
            float f_InnerY0 = f_Y0 + f_PadY;
            float f_InnerY1 = f_Y1 - f_PadY;
            float f_HNdc = f_InnerY1 - f_InnerY0;

            // Round text
            int i_ClampedRound = std::clamp(g_i_Round, 1, 24);
            std::string s_Round = "Round " + std::to_string(i_ClampedRound);

            float f_TopHPx = std::max(18.0f, f_HNdc * 0.60f * g_i_ViewportHeight);
            float f_Scale = (f_TopHPx * 0.75f) / 48.0f;

            float f_LeftPx = (f_InnerX0 * 0.5f + 0.5f) * g_i_ViewportWidth;
            float f_CYPx = 0.5f * (ndcY_to_px(f_InnerY0) + ndcY_to_px(f_InnerY1));
            float f_Baseline = f_CYPx + f_TopHPx * 0.38f - 20.0f;

            drawTextPx(s_Round, f_LeftPx, f_Baseline, f_Scale,
                g_HUDStyle.textColor.r, g_HUDStyle.textColor.g, g_HUDStyle.textColor.b, p_App);

            // Camera icon
            float f_CapAreaRight = f_InnerX1;

            if (g_GLuint_TexCamera) {
                float f_BtnH = (f_InnerY1 - f_InnerY0) * HUDStyle::k_CameraButtonScale;
                float f_BtnW = f_BtnH;
                float f_BtnX1 = f_InnerX1;
                float f_BtnX0 = f_BtnX1 - f_BtnW;

                float f_CenterY = 0.5f * (f_InnerY0 + f_InnerY1);
                float f_BtnY0 = f_CenterY - 0.5f * f_BtnH;
                float f_BtnY1 = f_CenterY + 0.5f * f_BtnH;

                g_f_CamBtnNdcX0 = f_BtnX0;
                g_f_CamBtnNdcX1 = f_BtnX1;
                g_f_CamBtnNdcY0 = f_InnerY0;
                g_f_CamBtnNdcY1 = f_InnerY1;

                drawIcon(g_GLuint_TexCamera, f_BtnX0, f_BtnY0, f_BtnX1, f_BtnY1, p_App);

                f_CapAreaRight = f_BtnX0 - f_Gap;
            }

            {
                float f_BtnH = (f_InnerY1 - f_InnerY0) * HUDStyle::k_CameraButtonScale;
                float f_BtnW = f_BtnH;
                float f_BtnX1 = f_CapAreaRight;
                float f_BtnX0 = f_BtnX1 - f_BtnW;

                float f_CenterY = 0.5f * (f_InnerY0 + f_InnerY1);
                float f_BtnY0 = f_CenterY - 0.5f * f_BtnH;
                float f_BtnY1 = f_CenterY + 0.5f * f_BtnH;

                g_f_PauseBtnNdcX0 = f_BtnX0;
                g_f_PauseBtnNdcX1 = f_BtnX1;
                g_f_PauseBtnNdcY0 = f_BtnY0;
                g_f_PauseBtnNdcY1 = f_BtnY1;

                if (g_GLuint_TexPause) {
                    drawIcon(g_GLuint_TexPause, f_BtnX0, f_BtnY0, f_BtnX1, f_BtnY1, p_App);
                } else {
                    drawRoundedRect(f_BtnX0, f_BtnY0, f_BtnX1, f_BtnY1, {0.12f, 0.12f, 0.12f, 0.85f}, pxToNDC(g_HUDStyle.slotRadiusPx), p_App);
                    drawTextCentered(std::string("||"), f_BtnX0, f_BtnY0, f_BtnX1, f_BtnY1, {1,1,1,1}, p_App);
                }

                f_CapAreaRight = f_BtnX0 - f_Gap;
                }

            // Tickets layout
            float f_CurRight = f_CapAreaRight;
            g_vec_PillRectNDC.clear();
            for (int i = int(g_vec_PillLabels.size()) - 1; i >= 1; --i) {
                const std::string& s_Label = g_vec_PillLabels[i];
                Color pillColor = g_vec_PillColors[i - 1 < (int)g_vec_PillColors.size() ? i - 1 : 0];

                float f_NeededTextW = textWidthPx(s_Label, f_Scale, p_App);
                float f_PadXPx = std::max(8.0f, g_HUDStyle.pillsPadXPx);
                float f_CapHPx = f_HNdc * g_i_ViewportHeight;
                float f_CapWPx = std::max(f_CapHPx * 1.8f, f_NeededTextW + 2.0f * f_PadXPx);
                float f_CapWNdc = (f_CapWPx / g_i_ViewportWidth) * 2.0f;

                float f_CapX1 = f_CurRight;
                float f_CapX0 = f_CapX1 - f_CapWNdc;
                if (f_CapX0 < f_InnerX0) f_CapX0 = f_InnerX0;

                drawRoundedRect(f_CapX0, f_InnerY0, f_CapX1, f_InnerY1, pillColor, pxToNDC(g_HUDStyle.slotRadiusPx), p_App);
                drawTextCentered(s_Label, f_CapX0, f_InnerY0, f_CapX1, f_InnerY1, { 1, 1, 1, 1 }, p_App);

                f_CurRight = f_CapX0 - f_Gap;
                if (f_CurRight <= f_InnerX0) break;
              
                // drawing number of tickets
                int count = -1;
                int pillIdx = i;
                if (!g_vec_PillCounts.empty() && pillIdx < (int)g_vec_PillCounts.size())
                    count = g_vec_PillCounts[pillIdx];

                bool isBlack = (s_Label == "Black") || (pillIdx == 1);
                bool isDouble = (s_Label == "2x") || (pillIdx == 2);

                if ((isBlack || isDouble) && count >= 0) {
                    float h = (f_InnerY1 - f_InnerY0);
                    float s = h * 0.80f;
                    float gap = -h * 2.65f; // padding

                    float bx0 = f_CapX1 + gap;
                    float bx1 = bx0 + s;
                    float by0 = f_InnerY0 + (h - s) * 0.5f;
                    float by1 = by0 + s;

                    
                    drawTextCentered(
                        std::to_string(std::max(0, std::min(99, count))),
                        bx0, by0, bx1, by1,
                        { 1, 1, 1, 1 },
                        p_App
                    );
                }



            }
        }

        constexpr float k_BottomBarGapPx = 10.0f;
        constexpr float k_BottomBarMarginPx = 6.0f;

        void drawBottomBar(float f_X0, float f_Y0, float f_X1, float f_Y1, Core::Application* p_App) {
            drawRoundedRect(f_X0, f_Y0, f_X1, f_Y1, g_HUDStyle.barColor, pxToNDC(g_HUDStyle.barRadiusPx), p_App);

            const float f_Gap = pxToNDC(k_BottomBarGapPx);
            const float f_Margin = pxToNDC(k_BottomBarMarginPx);

            float f_SX0 = f_X0 + f_Gap;
            float f_SX1 = f_X1 - f_Gap;
            float f_SY0 = f_Y0 + f_Gap * 0.6f;
            float f_SY1 = f_Y1 - f_Gap * 0.6f;

            float f_TotalW = (f_SX1 - f_SX0) - (k_TicketSlotCount - 1) * f_Margin;
            float f_SlotW = f_TotalW / float(k_TicketSlotCount);

            float f_X = f_SX0;
            for (int i = 0; i < k_TicketSlotCount; ++i) {
                Color c = g_vec_TicketSlots[i].color;
                if (c.a < 0.0f) c = g_HUDStyle.slotColor;

                drawRoundedRect(f_X, f_SY0, f_X + f_SlotW, f_SY1, c, pxToNDC(g_HUDStyle.slotRadiusPx), p_App);

                const TicketSlot& ts = g_vec_TicketSlots[i];
                bool replaced = false;
                if (ts.used && ts.mark != TicketMark::None) {
                    float padX = px_to_ndcX(4.0f);
                    float padY = px_to_ndcY(4.0f);

                    float ix0 = f_X + padX;
                    float ix1 = f_X + f_SlotW - padX;
                    float iy0 = f_SY0 + padY;
                    float iy1 = f_SY1 - padY;

                    Color bg = { 1.f, 1.f, 1.f, 0.10f };
                    drawRoundedRect(ix0, iy0, ix1, iy1, bg, pxToNDC(g_HUDStyle.slotRadiusPx), p_App);

                    const char* label = "";
                    switch (ts.mark) {
                    case TicketMark::Taxi:       label = "T";  break;
                    case TicketMark::Bus:        label = "B";  break;
                    case TicketMark::Metro:      label = "M";  break;
                    case TicketMark::Water:      label = "W";  break;
                    case TicketMark::Black:      label = "BL"; break;
                    case TicketMark::DoubleMove: label = "2x"; break;
                    default: break;
                    }

                    drawTextCentered(
                        label,
                        f_X, f_SY0, f_X + f_SlotW, f_SY1,
                        g_HUDStyle.textColor, p_App, g_HUDStyle.slotNumberDYPx
                    );
                    replaced = true;
                }

                if (!replaced) {
                    char buf[4];
                    std::snprintf(buf, sizeof(buf), "%d", i + 1);
                    drawTextCentered(
                        buf,
                        f_X, f_SY0, f_X + f_SlotW, f_SY1,
                        g_HUDStyle.textColor, p_App, g_HUDStyle.slotNumberDYPx
                    );
                }

                f_X += f_SlotW + f_Margin;
            }
        }

    } // namespace

    // API

    void LoadCameraIconPNG(const char* p_Path, Core::Application* p_App) {
        if (g_GLuint_TexCamera) {
            p_App->UnloadTexture(g_GLuint_TexCamera);
            g_GLuint_TexCamera = 0;
        }

        g_GLuint_TexCamera = p_App->LoadTexture(p_Path);
        if (g_GLuint_TexCamera == 0) {
            std::fprintf(stderr, "[HUD] Camera icon load FAILED: %s\n", p_Path);
        }
    }

    void LoadPauseIconPNG(const char* p_Path, Core::Application* p_App) {
        if (g_GLuint_TexPause) {
            p_App->UnloadTexture(g_GLuint_TexPause);
            g_GLuint_TexPause = 0;
        }

        g_GLuint_TexPause = p_App->LoadTexture(p_Path);
        if (g_GLuint_TexPause == 0) {
            std::fprintf(stderr, "[HUD] Pause icon load FAILED: %s\n", p_Path);
        }
    }

    void SetCameraToggleCallback(std::function<void()> fn_Callback) {
        g_fn_OnCameraToggle = std::move(fn_Callback);
    }

    void SetPauseCallback(std::function<void()> fn_Callback) {
        g_fn_OnPause = std::move(fn_Callback);
    }

    void ShowPausedModal(bool show) {
        g_b_ShowPausedModal.store(show);
    }

    void SetPausedResumeCallback(std::function<void()> cb) {
        g_fn_OnPausedResume = std::move(cb);
    }

    void SetPausedDebugCallback(std::function<void()> cb) {
        g_fn_OnPausedDebug = std::move(cb);
    }

    void SetPausedMenuCallback(std::function<void()> cb) {
        g_fn_OnPausedMenu = std::move(cb);
    }

    void HandleMouseMotion(int i_XPx, int i_YPx) {
        if (!g_b_ShowPausedModal.load()) return;
        int i_H = g_i_ViewportHeight;
        int i_FlippedY = i_H - i_YPx;
        bool b_Resume = (i_XPx >= g_i_PausedResumeBtnX0 && i_XPx <= g_i_PausedResumeBtnX1 &&
                         i_FlippedY >= g_i_PausedResumeBtnY0 && i_FlippedY <= g_i_PausedResumeBtnY1);
        bool b_Debug = (i_XPx >= g_i_PausedDebugBtnX0 && i_XPx <= g_i_PausedDebugBtnX1 &&
                        i_FlippedY >= g_i_PausedDebugBtnY0 && i_FlippedY <= g_i_PausedDebugBtnY1);
        bool b_Menu = (i_XPx >= g_i_PausedModalBtnX0 && i_XPx <= g_i_PausedModalBtnX1 &&
                       i_FlippedY >= g_i_PausedModalBtnY0 && i_FlippedY <= g_i_PausedModalBtnY1);
        g_b_PausedResumeBtnHover.store(b_Resume);
        g_b_PausedDebugBtnHover.store(b_Debug);
        g_b_PausedModalBtnHover.store(b_Menu);
    }

    void SetPausedDebugState(bool enabled) {
        g_b_DebugEnabled.store(enabled);
    }

    void HandleMouseClick(int i_XPx, int i_YPx) {
        float f_NX = (i_XPx / float(g_i_ViewportWidth)) * 2.0f - 1.0f;
        float f_NY = 1.0f - (i_YPx / float(g_i_ViewportHeight)) * 2.0f;

        if (g_b_ShowPausedModal.load()) {
            int i_H = g_i_ViewportHeight;
            int i_FlippedY = i_H - i_YPx;

            // RESUME
            if (i_XPx >= g_i_PausedResumeBtnX0 && i_XPx <= g_i_PausedResumeBtnX1 &&
                i_FlippedY >= g_i_PausedResumeBtnY0 && i_FlippedY <= g_i_PausedResumeBtnY1) {
                if (g_fn_OnPausedResume) g_fn_OnPausedResume();
                else g_b_ShowPausedModal.store(false);
                g_b_PausedResumeBtnHover.store(false);
                return;
            }

            // DEBUG
            if (i_XPx >= g_i_PausedDebugBtnX0 && i_XPx <= g_i_PausedDebugBtnX1 &&
                i_FlippedY >= g_i_PausedDebugBtnY0 && i_FlippedY <= g_i_PausedDebugBtnY1) {
                if (g_fn_OnPausedDebug) g_fn_OnPausedDebug();
                return;
            }

            // MENU
            if (i_XPx >= g_i_PausedModalBtnX0 && i_XPx <= g_i_PausedModalBtnX1 &&
                i_FlippedY >= g_i_PausedModalBtnY0 && i_FlippedY <= g_i_PausedModalBtnY1) {
                if (g_fn_OnPausedMenu) g_fn_OnPausedMenu();
                else g_b_ShowPausedModal.store(false);
                g_b_PausedModalBtnHover.store(false);
                return;
            }
            return;
        }

        // Camera button
        if (f_NX >= g_f_CamBtnNdcX0 && f_NX <= g_f_CamBtnNdcX1 &&
            f_NY >= g_f_CamBtnNdcY0 && f_NY <= g_f_CamBtnNdcY1)
        {
            if (g_fn_OnCameraToggle) g_fn_OnCameraToggle();
            return;
        }

        if (f_NX >= g_f_PauseBtnNdcX0 && f_NX <= g_f_PauseBtnNdcX1 &&
            f_NY >= g_f_PauseBtnNdcY0 && f_NY <= g_f_PauseBtnNdcY1)
        {
            if (g_fn_OnPause) g_fn_OnPause();
            return;
        }

    }

    bool InitHUD() {
        return true;
    }

    void DrawRoundedRectScreen(float f_X0, float f_Y0, float f_X1, float f_Y1, Color c, int i_RadiusPx, Core::Application* p_App) {
        auto pxToNdcX = [&](float f_Px) { return (f_Px / float(g_i_ViewportWidth)) * 2.0f - 1.0f; };
        auto pxToNdcY = [&](float f_Py) { return (f_Py / float(g_i_ViewportHeight)) * 2.0f - 1.0f; };
        drawRoundedRect(pxToNdcX(f_X0), pxToNdcY(f_Y0), pxToNdcX(f_X1), pxToNdcY(f_Y1), c, pxToNDC(i_RadiusPx), p_App);
    }

    void DrawTextCenteredPx(const std::string& s_Text, float f_X0_px, float f_Y0_px, float f_X1_px, float f_Y1_px, Color col, Core::Application* p_App, float f_DeltaYPx) {
        // Convert pixel rect to NDC using bottom-left origin mapping (to match DrawRoundedRectScreen)
        float nx0 = (f_X0_px / float(g_i_ViewportWidth)) * 2.0f - 1.0f;
        float nx1 = (f_X1_px / float(g_i_ViewportWidth)) * 2.0f - 1.0f;
        float ny0 = (f_Y0_px / float(g_i_ViewportHeight)) * 2.0f - 1.0f;
        float ny1 = (f_Y1_px / float(g_i_ViewportHeight)) * 2.0f - 1.0f;
        drawTextCentered(s_Text, nx0, ny0, nx1, ny1, col, p_App, f_DeltaYPx);
    }

    void ShutdownHUD() {
    }

    void SetViewport(int i_W, int i_H) {
        g_i_ViewportWidth = std::max(1, i_W);
        g_i_ViewportHeight = std::max(1, i_H);
    }

    void SetHUDStyle(const HUDStyle& style) {
        g_HUDStyle = style;
    }

    void SetSlotMark(int round_1_to_24, TicketMark mark, bool markUsed) {
        int idx = std::clamp(round_1_to_24, 1, (int)g_vec_TicketSlots.size()) - 1;
        if (idx < 0 || idx >= (int)g_vec_TicketSlots.size()) return;
        g_vec_TicketSlots[idx].mark = mark;
        if (markUsed) g_vec_TicketSlots[idx].used = true;
        // colors from tickets
        Color c = { -1.f, -1.f, -1.f, -1.f };
        switch (mark) {
        case TicketMark::Taxi:       c = g_vec_PillColors[2]; break;
        case TicketMark::Bus:        c = g_vec_PillColors[4]; break;
        case TicketMark::Metro:      c = g_vec_PillColors[3]; break;
        case TicketMark::Water:      c = g_vec_PillColors[5]; break;
        case TicketMark::Black:      c = g_vec_PillColors[0]; break;
        case TicketMark::DoubleMove: c = g_vec_PillColors[1]; break;
        default: break;
        }

        g_vec_TicketSlots[idx].color = c;
    }

    void SetTicketStates(const std::vector<TicketSlot>& slots) {
        g_vec_TicketSlots = slots;
        if (g_vec_TicketSlots.size() < k_TicketSlotCount) g_vec_TicketSlots.resize(k_TicketSlotCount);
        if (g_vec_TicketSlots.size() > k_TicketSlotCount) g_vec_TicketSlots.resize(k_TicketSlotCount);
    }

    void SetTopBar(const std::vector<std::string>& vec_Labels,
        const std::vector<Color>& vec_PillColors,
        const std::vector<int>& vec_Counts) {
        if (!vec_Labels.empty()) g_vec_PillLabels = vec_Labels;
        if (!vec_PillColors.empty()) g_vec_PillColors = vec_PillColors;
        if (!vec_Counts.empty()) {
            g_vec_PillCounts = vec_Counts;
        } else {
            g_vec_PillCounts.clear();
        }
    }

    void SetRound(int i_Round) {
        g_i_Round = std::clamp(i_Round, 1, 24);
    }

    void RenderHUD(Core::Application* p_App) {
        GLboolean b_DepthWas = glIsEnabled(GL_DEPTH_TEST);
        GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
        GLboolean b_SRGBWas = glIsEnabled(GL_FRAMEBUFFER_SRGB);

        if (b_DepthWas) glDisable(GL_DEPTH_TEST);
        if (!b_BlendWas) glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (b_SRGBWas) glDisable(GL_FRAMEBUFFER_SRGB);

        float f_TX0, f_TX1, f_TY0, f_TY1, f_BX0, f_BX1, f_BY0, f_BY1;
        computeBars(f_TX0, f_TX1, f_TY0, f_TY1, f_BX0, f_BX1, f_BY0, f_BY1);

        drawTopBar(f_TX0, f_TY0, f_TX1, f_TY1, p_App);
        drawBottomBar(f_BX0, f_BY0, f_BX1, f_BY1, p_App);

        // Draw paused modal on top of HUD if requested
        if (g_b_ShowPausedModal.load()) {
            GLboolean b_DepthWas = glIsEnabled(GL_DEPTH_TEST);
            GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
            if (b_DepthWas) glDisable(GL_DEPTH_TEST);
            if (!b_BlendWas) glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            int i_W = p_App->GetWidth();
            int i_H = p_App->GetHeight();
            float f_ModalWpx = std::min(640.0f, float(i_W) * 0.5f);
            float f_ModalHpx = std::min(240.0f, float(i_H) * 0.35f);
            float f_Left = (i_W - f_ModalWpx) * 0.5f;
            float f_Bottom = (i_H - f_ModalHpx) * 0.5f;
            float f_Right = f_Left + f_ModalWpx;
            float f_Top = f_Bottom + f_ModalHpx;

            DrawRoundedRectScreen(f_Left, f_Bottom, f_Right, f_Top, {0.08f, 0.08f, 0.12f, 0.95f}, 12, p_App);

            ScotlandYard::UI::Color white{1.0f,1.0f,1.0f,1.0f};
            float f_TitleH = f_ModalHpx * 0.22f;
            float f_MsgH = f_ModalHpx * 0.36f;

            float f_TitleBottom = f_Bottom + f_ModalHpx * 0.74f;
            float f_TitleTop = f_TitleBottom + f_TitleH;

            float f_MsgBottom = f_Bottom + f_ModalHpx * 0.25f;
            float f_MsgTop = f_MsgBottom + f_MsgH;

            DrawTextCenteredPx("PAUSED", f_Left + 20.0f, f_TitleBottom, f_Right - 20.0f, f_TitleTop, white, p_App, -17.0f);
            DrawTextCenteredPx("", f_Left + 20.0f, f_MsgBottom, f_Right - 20.0f, f_MsgTop, white, p_App, -6.0f);

            std::string s_DebugLabel = g_b_DebugEnabled.load() ? "TURN OFF DEBUGGING MODE" : "TURN ON DEBUGGING MODE";
            std::string s_ResumeLabel = "RESUME";
            std::string s_MenuLabel = "MENU";

            float f_TargetScale = 1.0f; 
            float f_ResumeW = textWidthPx(s_ResumeLabel, 1.0f, p_App);
            float f_DebugW = textWidthPx(s_DebugLabel, 1.0f, p_App);
            float f_MenuW = textWidthPx(s_MenuLabel, 1.0f, p_App);
            float f_MaxTextW = std::max({ f_ResumeW, f_DebugW, f_MenuW });
            float f_PadXPx = std::max(12.0f, g_HUDStyle.pillsPadXPx * 0.8f);
            float f_BtnW = std::min({ (float)i_W * 0.4f, f_ModalWpx * 0.6f, f_MaxTextW + 2.0f * f_PadXPx });
            float f_BtnH = 36.0f;
            float f_Spacing = 12.0f;
            float f_StartX = f_Left + (f_ModalWpx - f_BtnW) * 0.5f;
            float f_StartY = f_Bottom + 28.0f; 

            // RESUME button
            float f_ResumeX0 = f_StartX;
            float f_ResumeY0 = f_StartY;
            float f_ResumeX1 = f_ResumeX0 + f_BtnW;
            float f_ResumeY1 = f_ResumeY0 + f_BtnH;
            if (g_b_PausedResumeBtnHover.load()) {
                float pad = 6.0f;
                DrawRoundedRectScreen(f_ResumeX0 - pad, f_ResumeY0 - pad, f_ResumeX1 + pad, f_ResumeY1 + pad, {1.0f,1.0f,1.0f,0.08f}, 14, p_App);
            }
            DrawRoundedRectScreen(f_ResumeX0, f_ResumeY0, f_ResumeX1, f_ResumeY1, {0.0f,0.6f,0.2f,1.0f}, 10, p_App);
            DrawTextCenteredPx(s_ResumeLabel, f_ResumeX0, f_ResumeY0, f_ResumeX1, f_ResumeY1, white, p_App, -4.0f);

            // DEBUGGING MODE button
            float f_DebugX0 = f_StartX;
            float f_DebugY0 = f_ResumeY1 + f_Spacing;
            float f_DebugX1 = f_DebugX0 + f_BtnW;
            float f_DebugY1 = f_DebugY0 + f_BtnH;
            if (g_b_PausedDebugBtnHover.load()) {
                float pad = 6.0f;
                DrawRoundedRectScreen(f_DebugX0 - pad, f_DebugY0 - pad, f_DebugX1 + pad, f_DebugY1 + pad, {1.0f,1.0f,1.0f,0.08f}, 14, p_App);
            }
            DrawRoundedRectScreen(f_DebugX0, f_DebugY0, f_DebugX1, f_DebugY1, {0.0f,0.6f,0.2f,1.0f}, 10, p_App);
            DrawTextCenteredPx(s_DebugLabel, f_DebugX0, f_DebugY0, f_DebugX1, f_DebugY1, white, p_App, -4.0f);

            // MENU button
            float f_MenuX0 = f_StartX;
            float f_MenuY0 = f_DebugY1 + f_Spacing;
            float f_MenuX1 = f_MenuX0 + f_BtnW;
            float f_MenuY1 = f_MenuY0 + f_BtnH;
            if (g_b_PausedModalBtnHover.load()) {
                float pad = 6.0f;
                DrawRoundedRectScreen(f_MenuX0 - pad, f_MenuY0 - pad, f_MenuX1 + pad, f_MenuY1 + pad, {1.0f,1.0f,1.0f,0.08f}, 14, p_App);
            }
            DrawRoundedRectScreen(f_MenuX0, f_MenuY0, f_MenuX1, f_MenuY1, {0.0f,0.6f,0.2f,1.0f}, 10, p_App);
            DrawTextCenteredPx(s_MenuLabel, f_MenuX0, f_MenuY0, f_MenuX1, f_MenuY1, white, p_App, -4.0f);

            // store pixel rects for mouse handling
            g_i_PausedResumeBtnX0 = static_cast<int>(f_ResumeX0);
            g_i_PausedResumeBtnY0 = static_cast<int>(f_ResumeY0);
            g_i_PausedResumeBtnX1 = static_cast<int>(f_ResumeX1);
            g_i_PausedResumeBtnY1 = static_cast<int>(f_ResumeY1);

            g_i_PausedDebugBtnX0 = static_cast<int>(f_DebugX0);
            g_i_PausedDebugBtnY0 = static_cast<int>(f_DebugY0);
            g_i_PausedDebugBtnX1 = static_cast<int>(f_DebugX1);
            g_i_PausedDebugBtnY1 = static_cast<int>(f_DebugY1);

            g_i_PausedModalBtnX0 = static_cast<int>(f_MenuX0);
            g_i_PausedModalBtnY0 = static_cast<int>(f_MenuY0);
            g_i_PausedModalBtnX1 = static_cast<int>(f_MenuX1);
            g_i_PausedModalBtnY1 = static_cast<int>(f_MenuY1);

            if (b_BlendWas == GL_FALSE) glDisable(GL_BLEND);
            if (b_DepthWas) glEnable(GL_DEPTH_TEST);
        }

        if (b_SRGBWas) glEnable(GL_FRAMEBUFFER_SRGB);
        if (!b_BlendWas) glDisable(GL_BLEND);
        if (b_DepthWas) glEnable(GL_DEPTH_TEST);
    }

} // namespace UI
} // namespace ScotlandYard
