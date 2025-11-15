#include "MapGeneratorState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"
#include "MapGenerator.h"

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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

        void MapGeneratorState::RandomizeSeed() {
            unsigned int ui_Seed = static_cast<unsigned int>(time(nullptr));
            std::string s_NewSeed = std::to_string(ui_Seed);
            
            if (!m_vec_Fields.empty()) {
                m_vec_Fields[0].s_Value = s_NewSeed;
                std::cout << "[DEBUG] Randomized seed: " << s_NewSeed << std::endl;
            }
        }

        void MapGeneratorState::OnEnter() {
            m_s_InfoText.clear();
            m_vec_Fields.clear();
            m_vec_Sliders.clear();

            m_vec_Fields.push_back({ "Seed", "", {}, false, true, 16 });

            m_vec_Sliders.push_back({ "Num Parks", 3.0f, 1.0f, 10.0f, 1.0f });
            m_vec_Sliders.push_back({ "Park Min Size", 60.0f, 30.0f, 150.0f, 5.0f });
            m_vec_Sliders.push_back({ "Park Max Size", 100.0f, 50.0f, 200.0f, 5.0f });
            m_vec_Sliders.push_back({ "Num Bridges", 2.0f, 0.0f, 5.0f, 1.0f });

            m_i_FocusedFieldIndex = -1;
            m_b_HasPreview = false;
            SDL_StartTextInput();
            
            CreatePreviewQuad();
        }

        void MapGeneratorState::LayoutUI(int W, int H, Core::Application* p_App) {
            int outer = std::max(12, (int)(H * 0.04f));
            int pad = 24;
            int gap = 15;
            int titleH = 44;
            int btnW = 120, btnH = 40, btnGap = 12;
            int fieldH = 48;
            int prevW = 400, prevH = 300;

            int maxCardW = std::min(1200, (int)(W * 0.96f));
            int maxCardH = (int)(H * 0.96f);

            int cardW = std::min(maxCardW, (int)(W * 0.92f));
            int cardH = std::min(maxCardH, (int)(H * 0.88f));
            int cardX = (W - cardW) / 2;
            int cardY = (H - cardH) / 2;

            if (kDrawCardBg) {
                DrawRoundedRectScreen((float)cardX, (float)cardY,
                    (float)(cardX + cardW), (float)(cardY + cardH),
                    col_card, 16, p_App);
            } else {
                cardX = outer; cardW = W - 2 * outer;
                cardY = outer; cardH = H - 2 * outer;
            }

            // Title
            DrawTextCenteredPx("MAP GENERATOR",
                (float)cardX, (float)(cardY + cardH - titleH - 6),
                (float)(cardX + cardW), (float)(cardY + cardH - 6),
                col_txt, p_App, -6.0f);

            int y = cardY + cardH - titleH - pad;

            // Preview area
            prevW = std::min(prevW, cardW - 2 * pad);
            prevH = std::min(prevH, (int)((cardH - titleH - 3 * pad) * 0.5f));
            int prevX = cardX + (cardW - prevW) / 2;
            int prevY = y - prevH;
            m_rect_PreviewArea = SDL_Rect{ prevX, prevY, prevW, prevH };

            DrawTextCenteredPx("Preview", (float)prevX, (float)prevY - 26,
                (float)(prevX + prevW), (float)prevY - 2, col_mut, p_App, -2.0f);
            DrawRoundedRectScreen((float)prevX, (float)prevY,
                (float)(prevX + prevW), (float)(prevY + prevH),
                col_fld, 10, p_App);

            y = prevY - pad;

            // Seed field
            const int shortW = std::min(cardW - 2 * pad, 520);
            const int randomBtnW = 80;
            const int seedFieldW = shortW - randomBtnW - 8;
            const int seedX = cardX + (cardW - shortW) / 2;

            if (!m_vec_Fields.empty()) {
                int seedY = y - fieldH;
                y = seedY - gap;
                m_vec_Fields[0].rect = SDL_Rect{ seedX, seedY, seedFieldW, fieldH };
                m_BtnRandomSeed = SDL_Rect{ seedX + seedFieldW + 8, seedY + (fieldH - 32) / 2, randomBtnW, 32 };
            }

            // Sliders
            LayoutSliders(y, cardX, cardW, pad, gap);

            // Buttons at bottom
            int btnTotalW = btnW * 3 + btnGap * 2;
            int btnX0 = cardX + (cardW - btnTotalW) / 2;
            int btnY = cardY + pad / 2;

            m_BtnGenerate = SDL_Rect{ btnX0, btnY, btnW, btnH };
            m_BtnSave = SDL_Rect{ btnX0 + btnW + btnGap, btnY, btnW, btnH };
            m_BtnBack = SDL_Rect{ btnX0 + 2 * (btnW + btnGap), btnY, btnW, btnH };
        }

        void MapGeneratorState::LayoutSliders(int startY, int cardX, int cardW, int pad, int gap) {
            const int rowH = 44;
            const int shortW = std::min(cardW - 2 * pad, 520);
            int x = cardX + (cardW - shortW) / 2;
            int y = startY;

            if (m_vec_Sliders.size() >= 4) {
                const int smallW = (shortW - 16) / 2;
                const int gapBetween = 16;
                
                y -= rowH;
                m_vec_Sliders[0].track = SDL_Rect{ x, y + 16, smallW, 12 };
                
                m_vec_Sliders[3].track = SDL_Rect{ x + smallW + gapBetween, y + 16, smallW, 12 };
                y -= gap;
                
                for (size_t i = 1; i <= 2; ++i) {
                    auto& s = m_vec_Sliders[i];
                    y -= rowH;
                    s.track = SDL_Rect{ x, y + 16, shortW, 12 };
                    y -= gap;
                }
            } else {
                // Verical layout if there are less then 4 sliders
                for (size_t i = 0; i < m_vec_Sliders.size(); ++i) {
                    auto& s = m_vec_Sliders[i];
                    y -= rowH;
                    s.track = SDL_Rect{ x, y + 16, shortW, 12 };
                    y -= gap;
                }
            }
        }

        void MapGeneratorState::Render(Core::Application* p_App) {
            m_pApp = p_App;
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            const int W = p_App->GetWidth();
            const int H = p_App->GetHeight();

            DrawRoundedRectScreen(0, 0, (float)W, (float)H, col_bg, 0, p_App);
            LayoutUI(W, H, p_App);

            //Dedicated function for texture render
            RenderPreviewTexture(p_App);

            // Render seed field
            for (auto& f : m_vec_Fields) {
                auto bg = f.b_Focused ? col_card : col_fld;
                DrawRoundedRectScreen((float)f.rect.x, (float)f.rect.y,
                    (float)(f.rect.x + f.rect.w), (float)(f.rect.y + f.rect.h),
                    bg, 8, p_App);

                std::string shown = f.s_Value;
                if (f.b_Focused && (SDL_GetTicks() / 500) % 2 == 0) shown.push_back('|');
                
                DrawTextCenteredPx(f.s_Label.c_str(),
                    (float)f.rect.x + 10.0f, (float)f.rect.y + (float)f.rect.h - 18.0f,
                    (float)f.rect.x + (float)f.rect.w - 10.0f, (float)f.rect.y + (float)f.rect.h - 2.0f,
                    col_txt, p_App, 0.0f);

                DrawTextCenteredPx(shown.c_str(),
                    (float)f.rect.x + 12.0f, (float)f.rect.y + 6.0f,
                    (float)f.rect.x + (float)f.rect.w - 12.0f, (float)f.rect.y + (float)f.rect.h - 22.0f,
                    col_txt, p_App, 0.0f);
            }

            // Render sliders
            for (const auto& s : m_vec_Sliders) {
                DrawTextCenteredPx(s.s_Label.c_str(),
                    (float)s.track.x, (float)(s.track.y + s.track.h + 2),
                    (float)(s.track.x + s.track.w), (float)(s.track.y + s.track.h + 20),
                    col_mut, p_App, -2.0f);

                DrawRoundedRectScreen((float)s.track.x, (float)s.track.y,
                    (float)(s.track.x + s.track.w), (float)(s.track.y + s.track.h),
                    col_fld, 6, p_App);

                int fillX = ValueToXPosition(s);
                DrawRoundedRectScreen((float)s.track.x, (float)s.track.y,
                    (float)fillX, (float)(s.track.y + s.track.h),
                    col_accent, 6, p_App);

                int knobX = fillX - s.i_KnobWidth / 2;
                SDL_Rect knob = { knobX, s.track.y - 4, s.i_KnobWidth, s.track.h + 8 };
                DrawRoundedRectScreen((float)knob.x, (float)knob.y,
                    (float)(knob.x + knob.w), (float)(knob.y + knob.h),
                    col_txt, 6, p_App);

                char buf[64];
                if (s.f_Step >= 1.0f) std::snprintf(buf, sizeof(buf), "%.0f", s.f_Value);
                else std::snprintf(buf, sizeof(buf), "%.2f", s.f_Value);

                DrawTextCenteredPx(buf, (float)s.track.x, (float)(s.track.y - 18),
                    (float)(s.track.x + s.track.w), (float)(s.track.y - 2),
                    col_txt, p_App, -2.0f);
            }

            // Buttons
            DrawMenuLikeButton(m_BtnGenerate, "GENERATE", p_App);
            DrawMenuLikeButton(m_BtnSave, "SAVE", p_App);
            DrawMenuLikeButton(m_BtnBack, "BACK", p_App);
            DrawMenuLikeButton(m_BtnRandomSeed, "RANDOM", p_App);

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

            for (const char* p = utf8; *p; ++p) {
                char c = *p;
                if (f.b_Numeric) {
                    if ((c >= '0' && c <= '9') || c == '.' || c == '-') f.s_Value.push_back(c);
                } else {
                    if ((unsigned char)c >= 32 && (unsigned char)c < 127) f.s_Value.push_back(c);
                }
            }
        }

        void MapGeneratorState::BackspaceInFocusedField() {
            if (m_i_FocusedFieldIndex < 0) return;
            auto& f = m_vec_Fields[m_i_FocusedFieldIndex];
            if (!f.s_Value.empty()) f.s_Value.pop_back();
        }

        void MapGeneratorState::TryGenerateMap() {
            std::cout << "[MapGen] Generating map..." << std::endl;
            
            std::string seedStr = m_vec_Fields.empty() ? "" : m_vec_Fields[0].s_Value;
            unsigned int ui_Seed;
            
            if (seedStr.empty()) {
                ui_Seed = static_cast<unsigned int>(time(nullptr));
                seedStr = std::to_string(ui_Seed);
                if (!m_vec_Fields.empty()) {
                    m_vec_Fields[0].s_Value = seedStr;
                }
            } else {
                try {
                    ui_Seed = std::stoul(seedStr);
                } catch (...) {
                    ui_Seed = static_cast<unsigned int>(time(nullptr));
                }
            }
            
            srand(ui_Seed);

            int numParks = (int)m_vec_Sliders[0].f_Value;
            float parkMinSize = m_vec_Sliders[1].f_Value;
            float parkMaxSize = m_vec_Sliders[2].f_Value;
            int numBridges = (int)m_vec_Sliders[3].f_Value;
            
            if (parkMinSize > parkMaxSize) {
                std::swap(parkMinSize, parkMaxSize);
                m_vec_Sliders[1].f_Value = parkMinSize;
                m_vec_Sliders[2].f_Value = parkMaxSize;
                std::cout << "[MapGen] Swapped min/max sizes: min=" << parkMinSize 
                         << ", max=" << parkMaxSize << std::endl;
            }

            std::cout << "[MapGen] Parks: " << numParks << ", Size: " 
                      << parkMinSize << "-" << parkMaxSize << std::endl;

            int mapW = 1200;
            int mapH = 900;

            m_vec_GridPoints = MapGen::GenerateGridPoints(mapW, mapH);
            m_vec_ControlPoints = MapGen::GenerateRiverControlPoints(&m_i_CurrentCorner, mapW, mapH);
            m_vec_RiverPath = MapGen::GenerateRiverPath(m_vec_ControlPoints, 150);
            m_vec_GridPoints = MapGen::RemoveNodesOnRiver(m_vec_GridPoints, m_vec_RiverPath);
            
            m_vec_Parks = MapGen::GenerateParks(m_vec_GridPoints, m_vec_RiverPath,
                                                m_i_CurrentCorner, numParks, 
                                                parkMinSize, parkMaxSize);
            m_vec_Bridges = MapGen::GenerateBridges(m_vec_GridPoints, m_vec_RiverPath,
                                            numBridges, m_i_CurrentCorner);

            m_vec_GridPoints = MapGen::GenerateNodePositions(
                mapW, mapH,
                m_vec_RiverPath,
                m_vec_Parks,
                120, // or any number of nodes you want
                ui_Seed
            );

            SaveMapToFile();
            UpdatePreviewTexture();
            
            m_s_InfoText = "Map generated!";
            std::cout << "[MapGen] Generation complete!" << std::endl;
        }

        void MapGeneratorState::SaveMapToFile() {
            std::string filename = "generated_map.bmp";

            SDL_Surface* surface = SDL_CreateRGBSurface(0, 1200, 900, 24,
                                                        0xFF0000, 0x00FF00, 0x0000FF, 0);
            
            if (!surface) {
                std::cout << "[MapGen ERROR] Failed to create surface!" << std::endl;
                return;
            }
            
            SDL_LockSurface(surface);
            SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0, 0, 0));
            
            auto SetPixel = [&](int x, int y, Uint8 r, Uint8 g, Uint8 b) {
                if (x >= 0 && x < 1200 && y >= 0 && y < 900) {
                    Uint8* pixels = (Uint8*)surface->pixels;
                    int offset = (y * surface->pitch) + (x * 3);
                    pixels[offset + 0] = b;
                    pixels[offset + 1] = g;
                    pixels[offset + 2] = r;
                }
            };
            
            for (const auto& park : m_vec_Parks) {
                int minX = (int)(park.center.x - park.f_BaseRadius * 1.5f);
                int maxX = (int)(park.center.x + park.f_BaseRadius * 1.5f);
                int minY = (int)(park.center.y - park.f_BaseRadius * 1.5f);
                int maxY = (int)(park.center.y + park.f_BaseRadius * 1.5f);
                
                for (int y = minY; y <= maxY; ++y) {
                    for (int x = minX; x <= maxX; ++x) {
                        if (park.ContainsPoint((float)x, (float)y)) {
                            SetPixel(x, y, 34, 139, 34);
                        }
                    }
                }
            }
            
            for (const auto& rp : m_vec_RiverPath) {
                int cx = (int)rp.x;
                int cy = (int)rp.y;
                for (int dy = -20; dy <= 20; ++dy) {
                    for (int dx = -20; dx <= 20; ++dx) {
                        if (dx*dx + dy*dy <= 400) {
                            SetPixel(cx + dx, cy + dy, 50, 150, 255);
                        }
                    }
                }
            }
            
            for (const auto& p : m_vec_GridPoints) {
                bool b_IsBridgeEnd = false;
                for (const auto& bridge : m_vec_Bridges) {
                    float f_Dist1 = sqrt((p.x - bridge.first.x) * (p.x - bridge.first.x) + 
                                    (p.y - bridge.first.y) * (p.y - bridge.first.y));
                    float f_Dist2 = sqrt((p.x - bridge.second.x) * (p.x - bridge.second.x) + 
                                    (p.y - bridge.second.y) * (p.y - bridge.second.y));
                    
                    if (f_Dist1 < 1.0f || f_Dist2 < 1.0f) {
                        b_IsBridgeEnd = true;
                        break;
                    }
                }
                
                Uint8 r = b_IsBridgeEnd ? 255 : 255;
                Uint8 g = b_IsBridgeEnd ? 0 : 255;
                Uint8 b = b_IsBridgeEnd ? 0 : 255;
                
                for (int dy = -3; dy <= 3; ++dy) {
                    for (int dx = -3; dx <= 3; ++dx) {
                        if (dx*dx + dy*dy <= 9) {
                            SetPixel((int)p.x + dx, (int)p.y + dy, r, g, b);
                        }
                    }
                }
            }
            
            SDL_UnlockSurface(surface);
            
            if (SDL_SaveBMP(surface, filename.c_str()) == 0) {
                std::cout << "[MapGen] Map saved to: " << filename << std::endl;
            } else {
                std::cout << "[MapGen ERROR] Failed to save: " << SDL_GetError() << std::endl;
            }
            
            SDL_FreeSurface(surface);
        }

        void MapGeneratorState::UpdatePreviewTexture() {
            std::string s_PreviewPath = "generated_map.bmp";
            
            if (m_GLuint_PreviewTexture) {
                glDeleteTextures(1, &m_GLuint_PreviewTexture);
                m_GLuint_PreviewTexture = 0;
            }
            
            SDL_Surface* p_Surface = SDL_LoadBMP(s_PreviewPath.c_str());
            
            if (!p_Surface) {
                std::cerr << "[MapGen] Failed to load " << s_PreviewPath << ": "
                         << SDL_GetError() << std::endl;
                m_b_HasPreview = false;
                return;
            }
            
            glGenTextures(1, &m_GLuint_PreviewTexture);
            glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);
            
            GLenum format = (p_Surface->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
            
            glTexImage2D(GL_TEXTURE_2D, 0, format,
                         p_Surface->w, p_Surface->h, 0,
                         format, GL_UNSIGNED_BYTE, p_Surface->pixels);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            glBindTexture(GL_TEXTURE_2D, 0);
            
            m_i_PreviewWidth = p_Surface->w;
            m_i_PreviewHeight = p_Surface->h;
            m_b_HasPreview = true;
            
            SDL_FreeSurface(p_Surface);
            
            std::cout << "[MapGen] Preview texture loaded: " << m_i_PreviewWidth 
                      << "x" << m_i_PreviewHeight << std::endl;
        }

        void MapGeneratorState::RenderPreviewTexture(Core::Application* p_App) {
            if (!m_b_HasPreview || !m_GLuint_PreviewTexture) {
                return;
            }
            
            GLboolean b_DepthWas = glIsEnabled(GL_DEPTH_TEST);
            if (b_DepthWas) glDisable(GL_DEPTH_TEST);
            
            GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
            if (!b_BlendWas) glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            glUseProgram(m_ShaderProgram_Preview);
            
            int i_WindowWidth = p_App->GetWidth();
            int i_WindowHeight = p_App->GetHeight();
            
            glm::mat4 projection = glm::ortho(
                0.0f, (float)i_WindowWidth,
                0.0f, (float)i_WindowHeight,
                -1.0f, 1.0f
            );
            
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(
                (float)m_rect_PreviewArea.x,
                (float)m_rect_PreviewArea.y,
                0.0f
            ));
            model = glm::scale(model, glm::vec3(
                (float)m_rect_PreviewArea.w,
                (float)m_rect_PreviewArea.h,
                1.0f
            ));
            
            glm::mat4 mvp = projection * model;
            
            GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Preview, "MVP");
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);
            
            GLuint texLoc = glGetUniformLocation(m_ShaderProgram_Preview, "ourTexture");
            glUniform1i(texLoc, 0);
            
            glBindVertexArray(m_VAO_PreviewQuad);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            
            glBindTexture(GL_TEXTURE_2D, 0);
            
            if (!b_BlendWas) glDisable(GL_BLEND);
            if (b_DepthWas) glEnable(GL_DEPTH_TEST);
        }

        void MapGeneratorState::CreatePreviewQuad() {
            float quadVertices[] = {
                0.0f, 0.0f,  0.0f, 1.0f,
                1.0f, 0.0f,  1.0f, 1.0f,
                1.0f, 1.0f,  1.0f, 0.0f,
                
                0.0f, 0.0f,  0.0f, 1.0f,
                1.0f, 1.0f,  1.0f, 0.0f,
                0.0f, 1.0f,  0.0f, 0.0f
            };
            
            glGenVertexArrays(1, &m_VAO_PreviewQuad);
            glGenBuffers(1, &m_VBO_PreviewQuad);
            
            glBindVertexArray(m_VAO_PreviewQuad);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO_PreviewQuad);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
            
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glEnableVertexAttribArray(1);
            
            glBindVertexArray(0);
            
            const char* vertexShaderSrc = R"(
                #version 330 core
                layout(location = 0) in vec2 aPos;
                layout(location = 1) in vec2 aTexCoord;
                
                uniform mat4 MVP;
                
                out vec2 TexCoord;
                
                void main() {
                    TexCoord = aTexCoord;
                    gl_Position = MVP * vec4(aPos, 0.0, 1.0);
                }
            )";
            
            const char* fragmentShaderSrc = R"(
                #version 330 core
                in vec2 TexCoord;
                
                uniform sampler2D ourTexture;
                
                out vec4 FragColor;
                
                void main() {
                    FragColor = texture(ourTexture, TexCoord);
                }
            )";
            
            GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertexShader, 1, &vertexShaderSrc, nullptr);
            glCompileShader(vertexShader);
            
            GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragmentShader, 1, &fragmentShaderSrc, nullptr);
            glCompileShader(fragmentShader);
            
            m_ShaderProgram_Preview = glCreateProgram();
            glAttachShader(m_ShaderProgram_Preview, vertexShader);
            glAttachShader(m_ShaderProgram_Preview, fragmentShader);
            glLinkProgram(m_ShaderProgram_Preview);
            
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            
            std::cout << "[MapGen] Preview quad and shaders created" << std::endl;
        }

        void MapGeneratorState::RenderMapToTexture() {
            // Ta funkcja nie jest już używana - usunięto zbędny kod
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
                    
                    // Check field focus
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

                    // Check sliders
                    for (auto& s : m_vec_Sliders) {
                        SDL_Rect hit = { s.track.x - 6, s.track.y - 6, s.track.w + 12, s.track.h + 12 };
                        if (mx >= hit.x && mx <= hit.x + hit.w &&
                            my >= hit.y && my <= hit.y + hit.h) {
                            s.b_Dragging = true;
                            s.f_Value = XPositionToValue(s, mx);
                        }
                    }

                    // Check buttons
                    if (mx >= m_BtnGenerate.x && mx <= m_BtnGenerate.x + m_BtnGenerate.w &&
                        my >= m_BtnGenerate.y && my <= m_BtnGenerate.y + m_BtnGenerate.h) {
                        TryGenerateMap();
                    }
                    if (mx >= m_BtnSave.x && mx <= m_BtnSave.x + m_BtnSave.w &&
                        my >= m_BtnSave.y && my <= m_BtnSave.y + m_BtnSave.h) {
                        SaveMapToFile();
                    }
                    if (mx >= m_BtnBack.x && mx <= m_BtnBack.x + m_BtnBack.w &&
                        my >= m_BtnBack.y && my <= m_BtnBack.y + m_BtnBack.h) {
                        p_App->GetStateManager()->ChangeState("menu");
                    }
                    if (mx >= m_BtnRandomSeed.x && mx <= m_BtnRandomSeed.x + m_BtnRandomSeed.w &&
                        my >= m_BtnRandomSeed.y && my <= m_BtnRandomSeed.y + m_BtnRandomSeed.h) {
                        RandomizeSeed();
                    }
                }
                break;

            case SDL_MOUSEMOTION:
                if (ev.motion.state & SDL_BUTTON_LMASK) {
                    int mx = ev.motion.x, my = p_App->GetHeight() - ev.motion.y;
                    for (auto& s : m_vec_Sliders) {
                        if (s.b_Dragging) {
                            s.f_Value = XPositionToValue(s, mx);
                        }
                    }
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
