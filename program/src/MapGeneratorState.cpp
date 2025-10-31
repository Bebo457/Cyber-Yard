#include "MapGeneratorState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"

#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace ScotlandYard {
    namespace States {

        using ScotlandYard::UI::DrawRoundedRectScreen;
        using ScotlandYard::UI::DrawTextCenteredPx;
        using ScotlandYard::UI::DrawMenuLikeButton;
        namespace {
            static constexpr bool kDrawCardBg = false;
            const ScotlandYard::UI::Color col_bg{ 0.16f, 0.18f, 0.20f, 1.0f };
            const ScotlandYard::UI::Color col_card{ 0.11f,0.12f,0.16f,1.0f };
            const ScotlandYard::UI::Color col_fld{ 0.08f, 0.08f, 0.08f, 1.0f };
            const ScotlandYard::UI::Color col_txt{ 1,1,1,1 };
            const ScotlandYard::UI::Color col_mut{ 0.8f,0.83f,0.9f,0.85f };
            const ScotlandYard::UI::Color col_accent{ 1.00f, 0.89f, 0.00f, 1.0f };
        } 

        int MapGeneratorState::ValueToXPosition(const Slider& s) const {
            float t = (s.f_Value - s.f_MinValue) / (s.f_MaxValue - s.f_MinValue);
            t = std::max(0.0f, std::min(1.0f, t));
            return s.track.x + (int)std::round(t * s.track.w);
        }

        float MapGeneratorState::XPositionToValue(const Slider& s, int mx) const {
            float t = (float)(mx - s.track.x) / (float)s.track.w;
            t = std::max(0.0f, std::min(1.0f, t));
            float v = s.f_MinValue + t * (s.f_MaxValue - s.f_MinValue);
            if (s.f_Step > 0.0f) {
                v = std::round(v / s.f_Step) * s.f_Step;
            }
            return std::max(s.f_MinValue, std::min(s.f_MaxValue, v));
        }


        void MapGeneratorState::OnEnter() {
            m_s_InfoText.clear();
            m_vec_Fields.clear();
            m_vec_Sliders.clear();

            // text input for Seed
            m_vec_Fields.push_back({ "Seed", "12345", {}, false, true, 16 });

            // Sliders
            m_vec_Sliders.push_back({ "Nodes",         250.0f,  50.0f, 1000.0f, 10.0f });
            m_vec_Sliders.push_back({ "Graph density",   0.35f,  0.05f,   1.00f, 0.01f });
            m_vec_Sliders.push_back({ "Zones",           8.0f,    2.0f,   20.0f, 1.0f });

            // sliders for city route generating (bus dependance etc)
            m_vec_Sliders.push_back({ "Taxi",            0.70f,   0.0f,    1.0f, 0.01f });
            m_vec_Sliders.push_back({ "Bus",             0.50f,   0.0f,    1.0f, 0.01f });
            m_vec_Sliders.push_back({ "Tube",            0.25f,   0.0f,    1.0f, 0.01f });

            m_i_FocusedFieldIndex = -1;
            m_b_HasPreview = false;
            SDL_StartTextInput();
        }

        void MapGeneratorState::LayoutUI(int W, int H, Core::Application* p_App) {
            // Parameters
            int outer = std::max(12, (int)(H * 0.04f));
            int pad = 24;
            int gap = 15;
            int titleH = 44;
            int btnW = 148, btnH = 40, btnGap = 12;
            int fieldH = 48;
            int prevW = 300, prevH = 240;
            int infoH = 24;

            // Card size
            int maxCardW = std::min(1100, (int)(W * 0.96f));
            int maxCardH = (int)(H * 0.96f);

            int cardW = std::min(maxCardW, (int)(W * 0.92f));
            int cardH = std::min(maxCardH, (int)(H * 0.88f));
            int cardX = (W - cardW) / 2;
            int cardY = (H - cardH) / 2;
            int n = (int)m_vec_Fields.size();
            int totalFieldsH = n * fieldH + std::max(0, n - 1) * gap;

            auto neededH = [&](int fH, int g, int pH) {
                int fields = n * fH + std::max(0, n - 1) * g;
                return pad + titleH + pad + pH + pad + fields + pad + infoH + 8 + btnH + pad;
                };

            int wantedH = neededH(fieldH, gap, prevH);

            // Compress
            if (wantedH > cardH) {
                int targetH = std::min(maxCardH, wantedH);
                cardH = std::max(cardH, targetH);
                if (cardH > maxCardH) cardH = maxCardH;

                while (wantedH > cardH && prevH > 160) {
                    prevH -= 1;
                    wantedH = neededH(fieldH, gap, prevH);
                }
                while (wantedH > cardH && (fieldH > 40 || gap > 6)) {
                    if (fieldH > 40) { fieldH -= 1; }
                    if (gap > 6) { gap -= 1; }
                    wantedH = neededH(fieldH, gap, prevH);
                }
            }

            // Card background (optional, now off)
            if (kDrawCardBg) {
                DrawRoundedRectScreen((float)cardX, (float)cardY,
                    (float)(cardX + cardW), (float)(cardY + cardH),
                    col_card, 16, p_App);
            }
            else {
                // without card UI use the whole screen
                cardX = outer; cardW = W - 2 * outer;
                cardY = outer; cardH = H - 2 * outer;
            }

            // Title
            DrawTextCenteredPx("MAP GENERATOR",
                (float)cardX, (float)(cardY + cardH - titleH - 6),
                (float)(cardX + cardW), (float)(cardY + cardH - 6),
                col_txt, p_App, -6.0f);

            // Vertical layout: PREVIEW > FIELDS > INFO > BUTTONS
            int y = cardY + cardH - titleH - pad;

            // Preview
            prevW = std::min(prevW, cardW - 2 * pad);
            int prevX = cardX + (cardW - prevW) / 2;
            int prevY = y - prevH;
            m_rect_PreviewArea = SDL_Rect{ prevX, prevY, prevW, prevH };

            DrawRoundedRectScreen((float)prevX, (float)prevY,
                (float)(prevX + prevW), (float)(prevY + prevH),
                { 0.08f,0.09f,0.12f,1.0f }, 10, p_App);
            y = prevY - pad;

            // Field (seed)
            const int shortW = std::min(cardW - 2 * pad, 520);
            const int seedW = shortW;
            const int seedX = cardX + (cardW - seedW) / 2;

            if (!m_vec_Fields.empty()) {
                int seedY = y - fieldH;
                y = seedY - gap;
                m_vec_Fields[0].rect = SDL_Rect{ seedX, seedY, seedW, fieldH };
            }

            // Sliders
            int yAfterSeed = y;
            LayoutSliders(yAfterSeed, cardX, cardW, pad, gap);


            // Buttons
            int btnTotalW = btnW * 2 + btnGap;
            int btnX0 = cardX + (cardW - btnTotalW) / 2;
            int btnY = cardY - pad/2;

            m_BtnGenerate = SDL_Rect{ btnX0, btnY, btnW, btnH };
            m_BtnBack = SDL_Rect{ btnX0 + btnW + btnGap, btnY, btnW, btnH };
        }

        void MapGeneratorState::LayoutSliders(int startY, int cardX, int cardW, int pad, int gap) {
            const int rowH = 44;
            const int shortW = std::min(cardW - 2 * pad, 520);
            int x = cardX + (cardW - shortW) / 2;
            int y = startY;

            // 3 main sliders
            for (int i = 0; i < 3; ++i) {
                auto& s = m_vec_Sliders[i];
                y -= rowH;
                s.track = SDL_Rect{ x, y + 16, shortW, 12 };
                y -= gap;
            }

            // „Taxi/Bus/Metro"
            y -= rowH;
            int gapX = 12;
            int smallW = (shortW - 2 * gapX) / 3;
            for (int i = 3; i < 6; ++i) {
                int col = i - 3;
                auto& s = m_vec_Sliders[i];
                s.track = SDL_Rect{ x + col * (smallW + gapX), y + 16, smallW, 12 };
            }
        }

        void MapGeneratorState::Render(Core::Application* p_App) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            const int W = p_App->GetWidth();
            const int H = p_App->GetHeight();

            // background
            DrawRoundedRectScreen(0, 0, (float)W, (float)H, col_bg, 0, p_App);

            // layout
            LayoutUI(W, H, p_App);

            // preview (placeholder)
            DrawTextCenteredPx("Preview", (float)m_rect_PreviewArea.x, (float)m_rect_PreviewArea.y - 26,
                (float)(m_rect_PreviewArea.x + m_rect_PreviewArea.w), (float)m_rect_PreviewArea.y - 2, col_mut, p_App, -2.0f);
            DrawRoundedRectScreen((float)m_rect_PreviewArea.x, (float)m_rect_PreviewArea.y,
                (float)(m_rect_PreviewArea.x + m_rect_PreviewArea.w), (float)(m_rect_PreviewArea.y + m_rect_PreviewArea.h),
                col_fld, 10, p_App);
            if (m_b_HasPreview && m_GLuint_PreviewTexture) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);
                glDisable(GL_TEXTURE_2D);
            }
            else {
                // TODO: do rysowania tekstury preview textured quadem
            }

            // fields
            for (size_t i = 0; i < m_vec_Fields.size(); ++i) {
                auto& f = m_vec_Fields[i];
                const bool F = f.b_Focused;
                auto bg = F ? col_card : col_fld;
                DrawRoundedRectScreen((float)f.rect.x, (float)f.rect.y,
                    (float)(f.rect.x + f.rect.w), (float)(f.rect.y + f.rect.h),
                    bg, 8, p_App);

                std::string shown = f.s_Value;
                if (f.b_Focused && (SDL_GetTicks() / 500) % 2 == 0) shown.push_back('|');
                float lblX0 = (float)f.rect.x + 10.0f;
                float lblY0 = (float)f.rect.y + (float)f.rect.h - 18.0f;
                float lblX1 = lblX0 + (float)f.rect.w - 20.0f;
                float lblY1 = lblY0 + 16.0f;
                DrawTextCenteredPx(f.s_Label.c_str(), lblX0, lblY0, lblX1, lblY1, col_txt, p_App, 0.0f);

                float valX0 = (float)f.rect.x + 12.0f;
                float valY0 = (float)f.rect.y + 6.0f;
                float valX1 = (float)f.rect.x + (float)f.rect.w - 12.0f;
                float valY1 = (float)f.rect.y + (float)f.rect.h - 22.0f;
                DrawTextCenteredPx(shown.c_str(), valX0, valY0, valX1, valY1, col_txt, p_App, 0.0f);
            }

            // Info
            /*const char* msg = m_s_InfoText.empty() ? "Ready." : m_s_InfoText.c_str();
            auto color = (m_s_InfoText.rfind("ERROR", 0) == 0) ? col_txt : col_mut;

            const int margin = 20;
            const int infoW = 240;
            const int infoH = 24;
            const int infoX0 = margin;
            const int infoY0 = p_App->GetHeight() - infoH - margin;
            const int infoX1 = infoX0 + infoW;
            const int infoY1 = infoY0 + infoH;

            DrawTextCenteredPx(msg,
                (float)infoX0, (float)infoY0,
                (float)infoX1, (float)infoY1,
                color, p_App, -2.0f);*/


            // Sliders render
            for (size_t i = 0; i < m_vec_Sliders.size(); ++i) {
                const auto& s = m_vec_Sliders[i];

                // top label
                DrawTextCenteredPx(s.s_Label.c_str(),
                    (float)s.track.x, (float)(s.track.y + s.track.h + 2),
                    (float)(s.track.x + s.track.w), (float)(s.track.y + s.track.h + 20),
                    col_mut, p_App, -2.0f);

                // track
                DrawRoundedRectScreen(
                    (float)s.track.x, (float)s.track.y,
                    (float)(s.track.x + s.track.w), (float)(s.track.y + s.track.h),
                    col_fld, 6, p_App);

                // to value
                int fillX = ValueToXPosition(s);
                DrawRoundedRectScreen(
                    (float)s.track.x, (float)s.track.y,
                    (float)fillX, (float)(s.track.y + s.track.h),
                    col_accent, 6, p_App);

                // knob to move
                int knobX = fillX - s.i_KnobWidth / 2;
                SDL_Rect knob = { knobX, s.track.y - 4, s.i_KnobWidth, s.track.h + 8 };
                DrawRoundedRectScreen(
                    (float)knob.x, (float)knob.y,
                    (float)(knob.x + knob.w), (float)(knob.y + knob.h),
                    col_txt, 6, p_App);

                // value under
                char buf[64];
                if (s.f_Step >= 1.0f) std::snprintf(buf, sizeof(buf), "%.0f", s.f_Value);
                else                std::snprintf(buf, sizeof(buf), "%.2f", s.f_Value);

                DrawTextCenteredPx(buf,
                    (float)s.track.x, (float)(s.track.y - 18),
                    (float)(s.track.x + s.track.w), (float)(s.track.y - 2),
                    col_txt, p_App, -2.0f);
            }


            // Buttons
            DrawMenuLikeButton(m_BtnGenerate, "GENERATE", p_App /*, isHovered*/);
            DrawMenuLikeButton(m_BtnBack, "BACK", p_App /*, isHovered*/);



            SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
        }

        void MapGeneratorState::FocusField(int idx) {
            for (auto& f : m_vec_Fields) f.b_Focused = false;
            m_i_FocusedFieldIndex = (idx >= 0 && idx < (int)m_vec_Fields.size()) ? idx : -1;
            if (m_i_FocusedFieldIndex >= 0) m_vec_Fields[m_i_FocusedFieldIndex].b_Focused = true;
        }

        void MapGeneratorState::BlurAllFields() { FocusField(-1); }

        void MapGeneratorState::AppendTextToFocusedField(const char* utf8) {
            if (m_i_FocusedFieldIndex < 0) return;
            auto& f = m_vec_Fields[m_i_FocusedFieldIndex];
            if ((int)f.s_Value.size() >= f.i_MaxLength) return;

            // Filter
            for (const char* p = utf8; *p; ++p) {
                char c = *p;
                if (f.b_Numeric) {
                    if ((c >= '0' && c <= '9') || c == '.' || c == '-') f.s_Value.push_back(c);
                }
                else {
                    // block
                    if ((unsigned char)c >= 32 && (unsigned char)c < 127) f.s_Value.push_back(c);
                }
            }
        }

        void MapGeneratorState::BackspaceInFocusedField() {
            if (m_i_FocusedFieldIndex < 0) return;
            auto& f = m_vec_Fields[m_i_FocusedFieldIndex];
            if (!f.s_Value.empty()) f.s_Value.pop_back();
        }

        void MapGeneratorState::MakeDummyPreview(int W, int H) {
            // Checkerboard
            std::vector<unsigned char> img(W * H * 3, 0);
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
                bool a = ((x / 12) % 2) ^ ((y / 12) % 2);
                img[(y * W + x) * 3 + 0] = a ? 30 : 10;
                img[(y * W + x) * 3 + 1] = a ? 200 : 20;
                img[(y * W + x) * 3 + 2] = a ? 70 : 30;
            }
            if (!m_GLuint_PreviewTexture) glGenTextures(1, &m_GLuint_PreviewTexture);
            glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            m_b_HasPreview = true;
        }

        void MapGeneratorState::TryGenerateMap() {
            // Getting values from sliders/field
            auto valOf = [&](const char* name)->std::string {
                for (auto& f : m_vec_Fields)
                    if (f.s_Label == name)
                        return f.s_Value;
                return "";
                };

            auto getS = [&](const char* name)->float {
                for (auto& s : m_vec_Sliders)
                    if (s.s_Label == name)
                        return s.f_Value;
                return 0.0f;
                };

            // Upload values
            const std::string seedStr = valOf("Seed");
            const int nodes = (int)std::round(getS("Nodes"));
            const double dens = getS("Graph density");
            const int zones = (int)std::round(getS("Zones"));
            const double pTaxi = getS("Taxi");
            const double pBus = getS("Bus");
            const double pTube = getS("Tube");

            // For info field (not visible)
            // Validation
            if (dens <= 0.0 || dens > 1.0) {
                m_s_InfoText = "ERROR: Graph density must be in (0,1].";
                m_b_HasPreview = false;
                return;
            }
            if (nodes > 5000) {
                m_s_InfoText = "ERROR: Nodes too large for preview (<=5000).";
                m_b_HasPreview = false;
                return;
            }

            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "Generating… seed=%s, nodes=%d, dens=%.2f, taxi=%.2f bus=%.2f tube=%.2f",
                seedStr.c_str(), nodes, dens, pTaxi, pBus, pTube);

            m_s_InfoText = buf;
            MakeDummyPreview(
                std::max(32, m_rect_PreviewArea.w - 8),
                std::max(32, m_rect_PreviewArea.h - 8)
            );
            m_s_InfoText = "Generated preview (placeholder). Hook up your generator to produce real image/mesh.";
        }


        void MapGeneratorState::HandleEvent(const SDL_Event& ev, Core::Application* p_App) {
            switch (ev.type) {
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    p_App->GetStateManager()->ChangeState("menu");
                    return;
                }
                if (ev.key.keysym.sym == SDLK_TAB) {
                    int n = (int)m_vec_Fields.size();
                    if (n > 0) FocusField((m_i_FocusedFieldIndex + 1) % n);
                    return;
                }
                if (ev.key.keysym.sym == SDLK_BACKSPACE) {
                    BackspaceInFocusedField();
                    return;
                }
                if (ev.key.keysym.sym == SDLK_RETURN) {
                    // Enter = Generate
                    TryGenerateMap();
                    return;
                }
                break;

            case SDL_TEXTINPUT:
                AppendTextToFocusedField(ev.text.text);
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    int mx = ev.button.x, my = p_App->GetHeight() - ev.button.y;
                    bool focused = false;
                    for (size_t i = 0; i < m_vec_Fields.size(); ++i) {
                        auto& r = m_vec_Fields[i].rect;
                        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                            FocusField((int)i);
                            focused = true;
                            break;
                        }
                    }
                    if (!focused) BlurAllFields();

                    for (auto& s : m_vec_Sliders) {
                        SDL_Rect hit = { s.track.x - 6, s.track.y - 6, s.track.w + 12, s.track.h + 12 };
                        if (mx >= hit.x && mx <= hit.x + hit.w &&
                            my >= hit.y && my <= hit.y + hit.h) {
                            s.b_Dragging = true;
                            s.f_Value = XPositionToValue(s, mx);
                        }
                    }

                    // Generate
                    if (mx >= m_BtnGenerate.x && mx <= m_BtnGenerate.x + m_BtnGenerate.w &&
                        my >= m_BtnGenerate.y && my <= m_BtnGenerate.y + m_BtnGenerate.h) {
                        TryGenerateMap();
                    }
                    // Back
                    if (mx >= m_BtnBack.x && mx <= m_BtnBack.x + m_BtnBack.w &&
                        my >= m_BtnBack.y && my <= m_BtnBack.y + m_BtnBack.h) {
                        p_App->GetStateManager()->ChangeState("menu");
                    }
                }
                break;

            case SDL_MOUSEMOTION:
                if (ev.motion.state & SDL_BUTTON_LMASK) {
                    int mx = ev.motion.x, my = p_App->GetHeight() - ev.motion.y;
                    for (auto& s : m_vec_Sliders) if (s.b_Dragging)
                        s.f_Value = XPositionToValue(s, mx);
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    for (auto& s : m_vec_Sliders) s.b_Dragging = false;
                }
                break;

            default: break;
            }
        }

    }
} // namespaces
