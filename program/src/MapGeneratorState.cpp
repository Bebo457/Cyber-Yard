#include "MapGeneratorState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <iostream>

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
            // CLEAR existing elements before adding new ones
            m_vec_Fields.clear();
            m_vec_Sliders.clear();
            
            // Initialize fields for parameters
            m_vec_Fields.push_back({"Map Width", "1200", {}, false, true, 5});
            m_vec_Fields.push_back({"Map Height", "900", {}, false, true, 5});
            m_vec_Fields.push_back({"Num Parks", "3", {}, false, true, 2});
            m_vec_Fields.push_back({"Ferry Stops", "4", {}, false, true, 2}); // Changed label
            
            // Initialize sliders - parks
            m_vec_Sliders.push_back({"Park Min Size", 60.0f, 30.0f, 150.0f, 5.0f, {}, false});
            m_vec_Sliders.push_back({"Park Max Size", 100.0f, 50.0f, 200.0f, 5.0f, {}, false});
            m_vec_Sliders.push_back({"Min Park Distance", 150.0f, 50.0f, 300.0f, 10.0f, {}, false});
            
            // River curviness
            m_vec_Sliders.push_back({"River Curviness", 0.5f, 0.0f, 1.0f, 0.1f, {}, false});
            
            // Transport densities
            m_vec_Sliders.push_back({"Taxi Density", 1.0f, 0.5f, 2.0f, 0.1f, {}, false});
            m_vec_Sliders.push_back({"Bus Density", 0.6f, 0.3f, 1.5f, 0.1f, {}, false});
            m_vec_Sliders.push_back({"Metro Density", 0.3f, 0.1f, 1.0f, 0.1f, {}, false});
            
            m_s_InfoText = "Configure map generation parameters";

            m_i_FocusedFieldIndex = -1;
            m_b_HasPreview = false;
            SDL_StartTextInput();

            CreatePreviewQuad();
    
            m_s_InfoText = "Configure map generation parameters";
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
            for (int i = 0; i < 3 && i < (int)m_vec_Sliders.size(); ++i) {
                auto& s = m_vec_Sliders[i];
                y -= rowH;
                s.track = SDL_Rect{ x, y + 16, shortW, 12 };
                y -= gap;
            }

            // Additional sliders if any
            if (m_vec_Sliders.size() > 3) {
                y -= rowH;
                int gapX = 12;
                int remaining = (int)m_vec_Sliders.size() - 3;
                int smallW = (shortW - (remaining - 1) * gapX) / remaining;
                for (int i = 3; i < (int)m_vec_Sliders.size(); ++i) {
                    int col = i - 3;
                    auto& s = m_vec_Sliders[i];
                    s.track = SDL_Rect{ x + col * (smallW + gapX), y + 16, smallW, 12 };
                }
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

            // preview background
            DrawTextCenteredPx("Preview", (float)m_rect_PreviewArea.x, (float)m_rect_PreviewArea.y - 26,
                (float)(m_rect_PreviewArea.x + m_rect_PreviewArea.w), (float)m_rect_PreviewArea.y - 2, col_mut, p_App, -2.0f);
            DrawRoundedRectScreen((float)m_rect_PreviewArea.x, (float)m_rect_PreviewArea.y,
                (float)(m_rect_PreviewArea.x + m_rect_PreviewArea.w), (float)(m_rect_PreviewArea.y + m_rect_PreviewArea.h),
                col_fld, 10, p_App);
            // preview texture
            RenderPreviewTexture(p_App);

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
            DrawMenuLikeButton(m_BtnGenerate, "GENERATE", p_App);
            DrawMenuLikeButton(m_BtnBack, "BACK", p_App);
            SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
        }

        void MapGeneratorState::FocusField(int idx) {
            for (auto& f : m_vec_Fields) f.b_Focused = false;
            m_i_FocusedFieldIndex = (idx >= 0 && idx < (int)m_vec_Fields.size()) ? idx : -1;
            if (m_i_FocusedFieldIndex >= 0) m_vec_Fields[m_i_FocusedFieldIndex].b_Focused = true;
        }

        void MapGeneratorState::BlurAllFields() { 
            FocusField(-1); 
        }

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
            try {
                m_GenerationParams.i_MapWidth = std::stoi(m_vec_Fields[0].s_Value);
                m_GenerationParams.i_MapHeight = std::stoi(m_vec_Fields[1].s_Value);
                m_GenerationParams.i_NumParks = std::stoi(m_vec_Fields[2].s_Value);
                m_GenerationParams.i_NumFerries = std::stoi(m_vec_Fields[3].s_Value);
                
                m_GenerationParams.f_ParkMinSize = m_vec_Sliders[0].f_Value;
                m_GenerationParams.f_ParkMaxSize = m_vec_Sliders[1].f_Value;
                m_GenerationParams.f_MinParkDistance = m_vec_Sliders[2].f_Value;
                m_GenerationParams.f_RiverCurviness = m_vec_Sliders[3].f_Value;
                m_GenerationParams.f_TaxiDensity = m_vec_Sliders[4].f_Value;
                m_GenerationParams.f_BusDensity = m_vec_Sliders[5].f_Value;
                m_GenerationParams.f_MetroDensity = m_vec_Sliders[6].f_Value;
                
                std::cout << "Generating map: " << m_GenerationParams.i_MapWidth << "x" 
                          << m_GenerationParams.i_MapHeight 
                          << ", parks: " << m_GenerationParams.i_NumParks 
                          << ", ferries: " << m_GenerationParams.i_NumFerries << std::endl;
                
                GenerateAndRenderMap();
            }
            catch (const std::exception& e) {
                std::cout << "Error parsing parameters: " << e.what() << std::endl;
                m_s_InfoText = "ERROR: Invalid parameters";
            }
        }

        void MapGeneratorState::GenerateAndRenderMap() {
            try {
                std::cout << "Generating grid points..." << std::endl;
                // Generate grid
                m_vec_GridPoints = MapGen::GenerateGridPoints(m_GenerationParams.i_MapWidth, 
                                                              m_GenerationParams.i_MapHeight);
                
                std::cout << "Generating river..." << std::endl;
                // Generate river with curviness
                std::vector<MapGen::Point> controlPoints = 
                    MapGen::GenerateRiverControlPoints(&m_i_CurrentCorner, 
                                                       m_GenerationParams.i_MapWidth, 
                                                       m_GenerationParams.i_MapHeight,
                                                       m_GenerationParams.f_RiverCurviness);
                m_vec_RiverPath = MapGen::GenerateRiverPath(controlPoints, 150);
                
                std::cout << "Generating parks..." << std::endl;
                // Generate parks
                m_vec_Parks = MapGen::GenerateParks(m_vec_GridPoints, m_vec_RiverPath, 
                                                    m_i_CurrentCorner, m_GenerationParams);
                
                std::cout << "Generating graph nodes..." << std::endl;
                // Generate graph with connections
                m_vec_GraphNodes = MapGen::GenerateGraph(m_vec_GridPoints, m_vec_RiverPath,
                                                         m_vec_Parks, m_GenerationParams);
                
                // Count connections
                std::cout << "Counting connections..." << std::endl;
                int totalConnections = 0;
                for (const auto& node : m_vec_GraphNodes) {
                    totalConnections += (int)(node.set_TaxiConnections.size() 
                                     + node.set_BusConnections.size()
                                     + node.set_MetroConnections.size()
                                     + node.set_FerryConnections.size());
                }
                
                m_s_InfoText = "Generated: " + std::to_string(m_vec_Parks.size()) + " parks, "
                             + std::to_string(m_vec_GraphNodes.size()) + " nodes, "
                             + std::to_string(totalConnections / 2) + " connections";
                
                std::cout << m_s_InfoText << std::endl;
                
                // Export to files
                std::cout << "Exporting files..." << std::endl;
                ExportMapToFile();
                
                // Load the generated image as preview
                std::cout << "Loading preview..." << std::endl;
                UpdatePreviewTexture();
                
                std::cout << "Generation complete!" << std::endl;
            }
            catch (const std::exception& e) {
                std::cout << "ERROR in GenerateAndRenderMap: " << e.what() << std::endl;
                m_s_InfoText = "ERROR: " + std::string(e.what());
            }
            catch (...) {
                std::cout << "UNKNOWN ERROR in GenerateAndRenderMap" << std::endl;
                m_s_InfoText = "ERROR: Unknown exception";
            }
        }

        void MapGeneratorState::ExportMapToFile() {
            // Export PNG with connections
            std::string pngFile = "generated_map.bmp";
            bool success = MapGen::ExportMapToPNG(pngFile, 
                                                  m_GenerationParams.i_MapWidth, 
                                                  m_GenerationParams.i_MapHeight,
                                                  m_vec_GridPoints, m_vec_RiverPath, 
                                                  m_vec_Parks, &m_vec_GraphNodes);
            if (success) {
                std::cout << "Map exported to: " << pngFile << std::endl;
            }
            
            // Export JSON data
            std::string jsonFile = "generated_map.json";
            success = MapGen::ExportMapToJSON(jsonFile, m_GenerationParams,
                                             m_vec_GraphNodes, m_vec_RiverPath, m_vec_Parks);
            if (success) {
                std::cout << "Data exported to: " << jsonFile << std::endl;
            }
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

                    // Generate button
                    if (mx >= m_BtnGenerate.x && mx <= m_BtnGenerate.x + m_BtnGenerate.w &&
                        my >= m_BtnGenerate.y && my <= m_BtnGenerate.y + m_BtnGenerate.h) {
                        TryGenerateMap();
                    }
                    // Back button
                    if (mx >= m_BtnBack.x && mx <= m_BtnBack.x + m_BtnBack.w &&
                        my >= m_BtnBack.y && my <= m_BtnBack.y + m_BtnBack.h) {
                        p_App->GetStateManager()->ChangeState("menu");
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
        void MapGeneratorState::UpdatePreviewTexture() {
            std::string s_PreviewPath = "generated_map.bmp";
            
            // Delete old texture if exists
            if (m_GLuint_PreviewTexture) {
                glDeleteTextures(1, &m_GLuint_PreviewTexture);
                m_GLuint_PreviewTexture = 0;
            }
            
            // Load BMP file
            SDL_Surface* p_Surface = SDL_LoadBMP(s_PreviewPath.c_str());
            
            if (!p_Surface) {
                std::cerr << "[MapGeneratorState] Failed to load " << s_PreviewPath << ": " 
                        << SDL_GetError() << std::endl;
                m_b_HasPreview = false;
                return;
            }
            
            // Create OpenGL texture
            glGenTextures(1, &m_GLuint_PreviewTexture);
            glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);
            
            GLenum format = (p_Surface->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
            
            glTexImage2D(GL_TEXTURE_2D, 0, format, 
                        p_Surface->w, p_Surface->h, 0, 
                        format, GL_UNSIGNED_BYTE, p_Surface->pixels);
            
            // Texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            glBindTexture(GL_TEXTURE_2D, 0);
            
            m_i_PreviewWidth = p_Surface->w;
            m_i_PreviewHeight = p_Surface->h;
            m_b_HasPreview = true;
            
            SDL_FreeSurface(p_Surface);   
        }

        void MapGeneratorState::RenderPreviewTexture(Core::Application* p_App) {
            if (!m_b_HasPreview || !m_GLuint_PreviewTexture) {
                return;
            }
            
            // Save and setup render state
            GLboolean b_DepthWas = glIsEnabled(GL_DEPTH_TEST);
            if (b_DepthWas) glDisable(GL_DEPTH_TEST);
            
            GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
            if (!b_BlendWas) glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            glUseProgram(m_ShaderProgram_Preview);
            
            // Orthographic projection in screen pixel coordinates
            int i_WindowWidth = p_App->GetWidth();
            int i_WindowHeight = p_App->GetHeight();
            
            glm::mat4 projection = glm::ortho(
                0.0f, (float)i_WindowWidth,
                0.0f, (float)i_WindowHeight,
                -1.0f, 1.0f
            );
            
            // Model matrix: position and scale quad to preview area
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
            
            // Set uniforms
            GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Preview, "MVP");
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);
            
            GLuint texLoc = glGetUniformLocation(m_ShaderProgram_Preview, "ourTexture");
            glUniform1i(texLoc, 0);
            
            // Draw quad
            glBindVertexArray(m_VAO_PreviewQuad);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            
            glBindTexture(GL_TEXTURE_2D, 0);
            
            // Restore state
            if (!b_BlendWas) glDisable(GL_BLEND);
            if (b_DepthWas) glEnable(GL_DEPTH_TEST);
        }

        void MapGeneratorState::CreatePreviewQuad() {
            // Quad geometry with UV coordinates
            float quadVertices[] = {
                // Position (x, y)    // UV (u, v)
                0.0f, 0.0f,          0.0f, 1.0f,
                1.0f, 0.0f,          1.0f, 1.0f,
                1.0f, 1.0f,          1.0f, 0.0f,
                
                0.0f, 0.0f,          0.0f, 1.0f,
                1.0f, 1.0f,          1.0f, 0.0f,
                0.0f, 1.0f,          0.0f, 0.0f
            };
            
            glGenVertexArrays(1, &m_VAO_PreviewQuad);
            glGenBuffers(1, &m_VBO_PreviewQuad);
            
            glBindVertexArray(m_VAO_PreviewQuad);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO_PreviewQuad);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
            
            // Attribute 0: Position
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            
            // Attribute 1: UV
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glEnableVertexAttribArray(1);
            
            glBindVertexArray(0);
            
            // Compile shaders
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
        }

        void MapGeneratorState::OnExit() {
            if (m_VAO_PreviewQuad) {
                glDeleteVertexArrays(1, &m_VAO_PreviewQuad);
                m_VAO_PreviewQuad = 0;
            }
            if (m_VBO_PreviewQuad) {
                glDeleteBuffers(1, &m_VBO_PreviewQuad);
                m_VBO_PreviewQuad = 0;
            }
            if (m_ShaderProgram_Preview) {
                glDeleteProgram(m_ShaderProgram_Preview);
                m_ShaderProgram_Preview = 0;
            }
            if (m_GLuint_PreviewTexture) {
                glDeleteTextures(1, &m_GLuint_PreviewTexture);
                m_GLuint_PreviewTexture = 0;
            }
            
            SDL_StopTextInput();
        }

    }
} // namespaces
