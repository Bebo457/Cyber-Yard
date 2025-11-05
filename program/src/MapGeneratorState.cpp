#include "MapGeneratorState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"
#include "MapGenerator.h"
#include "MapPreviewState.h"

#include<iostream>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>  
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
            m_pApp = p_App;
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
            std::cout << "[DEBUG RENDER] Drawing texture ID: " << m_GLuint_PreviewTexture << std::endl;
            
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, W, H, 0, -1, 1);  // UWAGA: H, 0 dla SDL (Y w dół)
            
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            // Włącz teksturowanie
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

            // Oblicz współrzędne
            float x0 = (float)m_rect_PreviewArea.x;
            float y0 = (float)m_rect_PreviewArea.y;
            float x1 = x0 + (float)m_rect_PreviewArea.w;
            float y1 = y0 + (float)m_rect_PreviewArea.h;

            std::cout << "[DEBUG] Drawing quad at: " << x0 << "," << y0 << " to " << x1 << "," << y1 << std::endl;

            // Narysuj quad
            glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);  // lewy górny
                glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);  // prawy górny
                glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);  // prawy dolny
                glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);  // lewy dolny
            glEnd();

            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                std::cout << "[DEBUG RENDER ERROR] OpenGL error: " << err << std::endl;
            } else {
                std::cout << "[DEBUG RENDER] Success!" << std::endl;
            }

            // Przywróć stan
            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            
            glPopAttrib();
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
        std::cout << "[DEBUG] TryGenerateMap() called" << std::endl;
        
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

        const std::string seedStr = valOf("Seed");
        const int nodes = (int)std::round(getS("Nodes"));
        const double dens = getS("Graph density");

        if (dens <= 0.0 || dens > 1.0) {
            m_s_InfoText = "ERROR: Graph density must be in (0,1].";
            return;
        }

        if (nodes > 5000) {
            m_s_InfoText = "ERROR: Nodes too large for preview (<=5000).";
            return;
        }

        m_s_InfoText = "Generating map...";

        int mapW = 1200;
        int mapH = 900;

        // seed initialisation
        if (!seedStr.empty()) {
            try {
                unsigned int ui_Seed = std::stoul(seedStr);
                srand(ui_Seed);
            } catch (...) {
                srand(static_cast<unsigned>(time(nullptr)));
            }
        }

        // Generate
        m_vec_GridPoints = MapGen::GenerateGridPoints(mapW, mapH);
        m_vec_ControlPoints = MapGen::GenerateRiverControlPoints(&m_i_CurrentCorner, mapW, mapH);
        m_vec_RiverPath = MapGen::GenerateRiverPath(m_vec_ControlPoints, 150);
        m_vec_Parks = MapGen::GenerateParks(m_vec_GridPoints, m_vec_RiverPath,
                                            m_i_CurrentCorner, MapGen::Config::NUM_PARKS);

        ShowMapInNewWindow(mapW, mapH);

        m_s_InfoText = "Map generated and displayed!";
    }


    void MapGeneratorState::ShowMapInNewWindow(int i_Width, int i_Height) {
        std::cout << "[MapPreview] Creating preview window..." << std::endl;
        
        SDL_Surface* surface = SDL_CreateRGBSurface(0, i_Width, i_Height, 24,
                                                    0xFF0000, 0x00FF00, 0x0000FF, 0);
        
        if (!surface) {
            std::cout << "[ERROR] Failed to create surface!" << std::endl;
            return;
        }
        
        SDL_LockSurface(surface);
        
        SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0, 0, 0));
        
        auto SetPixel = [&](int x, int y, Uint8 r, Uint8 g, Uint8 b) {
            if (x >= 0 && x < i_Width && y >= 0 && y < i_Height) {
                Uint8* pixels = (Uint8*)surface->pixels;
                int offset = (y * surface->pitch) + (x * 3);
                pixels[offset + 0] = b;
                pixels[offset + 1] = g;
                pixels[offset + 2] = r;
            }
        };
        
        for (const auto& park : m_vec_Parks) {
            int i_MinX = static_cast<int>(park.center.x - park.f_BaseRadius * 1.5f);
            int i_MaxX = static_cast<int>(park.center.x + park.f_BaseRadius * 1.5f);
            int i_MinY = static_cast<int>(park.center.y - park.f_BaseRadius * 1.5f);
            int i_MaxY = static_cast<int>(park.center.y + park.f_BaseRadius * 1.5f);
            
            for (int y = i_MinY; y <= i_MaxY; ++y) {
                for (int x = i_MinX; x <= i_MaxX; ++x) {
                    if (park.ContainsPoint(static_cast<float>(x), static_cast<float>(y))) {
                        SetPixel(x, y, 34, 139, 34);
                    }
                }
            }
        }
        
        for (const auto& rp : m_vec_RiverPath) {
            int cx = static_cast<int>(rp.x);
            int cy = static_cast<int>(rp.y);
            for (int dx = -20; dx <= 20; dx++) {
                for (int dy = -20; dy <= 20; dy++) {
                    if (dx*dx + dy*dy <= 400) {
                        SetPixel(cx + dx, cy + dy, 50, 150, 255);
                    }
                }
            }
        }
        
        for (const auto& p : m_vec_GridPoints) {
            for (int dx = -3; dx <= 3; dx++) {
                for (int dy = -3; dy <= 3; dy++) {
                    if (dx*dx + dy*dy <= 9) {
                        SetPixel(static_cast<int>(p.x) + dx, static_cast<int>(p.y) + dy, 255, 255, 255);
                    }
                }
            }
        }
        
        SDL_UnlockSurface(surface);
        
        SDL_Window* previewWindow = SDL_CreateWindow(
            "Generated Map Preview",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            i_Width, i_Height,
            SDL_WINDOW_SHOWN
        );
        
        if (!previewWindow) {
            std::cout << "[ERROR] Failed to create window!" << std::endl;
            SDL_FreeSurface(surface);
            return;
        }
        
        SDL_Surface* windowSurface = SDL_GetWindowSurface(previewWindow);
        SDL_BlitSurface(surface, nullptr, windowSurface, nullptr);
        SDL_UpdateWindowSurface(previewWindow);
        
        std::cout << "[MapPreview] Window created." << std::endl;
        std::cout << "[MapPreview] Controls:" << std::endl;
        std::cout << "  - Press S to save map to file" << std::endl;
        std::cout << "  - Press ESC or click X to close" << std::endl;
        
        auto GenerateFilename = []() -> std::string {
            time_t now = time(nullptr);
            struct tm timeinfo;
            
            #ifdef _WIN32
            localtime_s(&timeinfo, &now);
            #else
            localtime_r(&now, &timeinfo);
            #endif
            
            char buffer[100];
            strftime(buffer, sizeof(buffer), "map_%Y%m%d_%H%M%S.bmp", &timeinfo);
            return std::string(buffer);
        };
        
        bool running = true;
        SDL_Event ev;
        while (running) {
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) {
                    running = false;
                }
                else if (ev.type == SDL_WINDOWEVENT) {
                    if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                        if (SDL_GetWindowID(previewWindow) == ev.window.windowID) {
                            running = false;
                        }
                    }
                }
                else if (ev.type == SDL_KEYDOWN) {
                    if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    else if (ev.key.keysym.sym == SDLK_s) {
                        std::string filename = GenerateFilename();
                        if (SDL_SaveBMP(surface, filename.c_str()) == 0) {
                            std::cout << "[MapPreview] Map saved to: " << filename << std::endl;
                            std::string newTitle = "Map saved to " + filename + " - Press S to save again";
                            SDL_SetWindowTitle(previewWindow, newTitle.c_str());
                        } else {
                            std::cout << "[MapPreview ERROR] Failed to save map: " << SDL_GetError() << std::endl;
                        }
                    }
                }
            }
            SDL_Delay(16);
        }
        
        SDL_DestroyWindow(previewWindow);
        SDL_FreeSurface(surface);
        
        std::cout << "[MapPreview] Preview window closed." << std::endl;
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

                    if (mx >= m_BtnGenerate.x && mx <= m_BtnGenerate.x + m_BtnGenerate.w &&
                        my >= m_BtnGenerate.y && my <= m_BtnGenerate.y + m_BtnGenerate.h) {
                        std::cout << "[DEBUG] Generate button clicked!" << std::endl;
                        TryGenerateMap();
                    }
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

    void MapGeneratorState::GenerateMapPreview(int i_Width, int i_Height) {
            std::cout << "[DEBUG] GenerateMapPreview() called with size: " 
              << i_Width << "x" << i_Height << std::endl;
        if (!m_vec_Fields.empty() && !m_vec_Fields[0].s_Value.empty()) {
            try {
                unsigned int ui_Seed = std::stoul(m_vec_Fields[0].s_Value);
                srand(ui_Seed);
            } catch (...) {
                srand(static_cast<unsigned>(time(nullptr)));
            }
        }
        
        m_vec_GridPoints = MapGen::GenerateGridPoints(i_Width, i_Height);
        
        m_vec_ControlPoints = MapGen::GenerateRiverControlPoints(&m_i_CurrentCorner, i_Width, i_Height);
        m_vec_RiverPath = MapGen::GenerateRiverPath(m_vec_ControlPoints, 150);
        
        m_vec_Parks = MapGen::GenerateParks(m_vec_GridPoints, m_vec_RiverPath, 
                                            m_i_CurrentCorner, MapGen::Config::NUM_PARKS);
        
        RenderMapToTexture(i_Width, i_Height);
    }

    void MapGeneratorState::RenderMapToTexture(int i_Width, int i_Height) {
        std::cout << "[DEBUG] RenderMapToTexture() called" << std::endl;
        std::vector<unsigned char> vec_Image(i_Width * i_Height * 3, 0);
        
        for (int i = 0; i < i_Width * i_Height * 3; i += 3) {
            vec_Image[i + 0] = 220; // R
            vec_Image[i + 1] = 220; // G
            vec_Image[i + 2] = 220; // B
        }
        
        auto SetPixel = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b) {
            if (x >= 0 && x < i_Width && y >= 0 && y < i_Height) {
                int idx = (y * i_Width + x) * 3;
                vec_Image[idx + 0] = r;
                vec_Image[idx + 1] = g;
                vec_Image[idx + 2] = b;
            }
        };
        
        std::cout << "[DEBUG] Drawing " << m_vec_Parks.size() << " parks..." << std::endl;
        for (const auto& park : m_vec_Parks) {
            int i_MinX = static_cast<int>(park.center.x - park.f_BaseRadius * 1.5f);
            int i_MaxX = static_cast<int>(park.center.x + park.f_BaseRadius * 1.5f);
            int i_MinY = static_cast<int>(park.center.y - park.f_BaseRadius * 1.5f);
            int i_MaxY = static_cast<int>(park.center.y + park.f_BaseRadius * 1.5f);
            
            for (int y = i_MinY; y <= i_MaxY; ++y) {
                for (int x = i_MinX; x <= i_MaxX; ++x) {
                    if (park.ContainsPoint(static_cast<float>(x), static_cast<float>(y))) {
                        SetPixel(x, y, 34, 139, 34); 
                    }
                }
            }
        }
        
        for (size_t i = 0; i < m_vec_RiverPath.size(); ++i) {
            int i_CenterX = static_cast<int>(m_vec_RiverPath[i].x);
            int i_CenterY = static_cast<int>(m_vec_RiverPath[i].y);
            
            for (int i_OffsetX = -MapGen::Config::RIVER_WIDTH/2; i_OffsetX <= MapGen::Config::RIVER_WIDTH/2; ++i_OffsetX) {
                for (int i_OffsetY = -MapGen::Config::RIVER_WIDTH/2; i_OffsetY <= MapGen::Config::RIVER_WIDTH/2; ++i_OffsetY) {
                    if (i_OffsetX*i_OffsetX + i_OffsetY*i_OffsetY <= (MapGen::Config::RIVER_WIDTH/2)*(MapGen::Config::RIVER_WIDTH/2)) {
                        SetPixel(i_CenterX + i_OffsetX, i_CenterY + i_OffsetY, 50, 150, 255); // Niebieski
                    }
                }
            }
        }
        
        const int POINT_RADIUS = 3;
        for (const auto& p : m_vec_GridPoints) {
            for (int i_Dx = -POINT_RADIUS; i_Dx <= POINT_RADIUS; ++i_Dx) {
                for (int i_Dy = -POINT_RADIUS; i_Dy <= POINT_RADIUS; ++i_Dy) {
                    if (i_Dx*i_Dx + i_Dy*i_Dy <= POINT_RADIUS * POINT_RADIUS) {
                        SetPixel(static_cast<int>(p.x) + i_Dx, 
                            static_cast<int>(p.y) + i_Dy, 
                            255, 255, 255); // Biały
                    }
                }
            }
        }
        
        if (!m_GLuint_PreviewTexture) {
            glGenTextures(1, &m_GLuint_PreviewTexture);
        }
        glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, i_Width, i_Height, 0, GL_RGB, GL_UNSIGNED_BYTE, vec_Image.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        m_b_HasPreview = true;
        std::cout << "[DEBUG] m_b_HasPreview = true, TextureID = " << m_GLuint_PreviewTexture << std::endl;
    }

    }
} // namespaces
