#include "MapGeneratorState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"
#include "MapGenerator.h"
#include "HighwayGenerator.h" 
#include "MapPreviewState.h" 
// Dodano nagłówek środowiska 3D
#include "EmptyEnvironmentState.h"

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
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <set>
#include <memory> // Ważne dla std::make_unique

namespace ScotlandYard
{
    namespace States
    {

        using ScotlandYard::UI::DrawMenuLikeButton;
        using ScotlandYard::UI::DrawRoundedRectScreen;
        using ScotlandYard::UI::DrawTextCenteredPx;

        namespace
        {
            static constexpr bool kDrawCardBg = false;
            const ScotlandYard::UI::Color col_bg{ 0.16f, 0.18f, 0.20f, 1.0f };
            const ScotlandYard::UI::Color col_card{ 0.11f, 0.12f, 0.16f, 1.0f };
            const ScotlandYard::UI::Color col_fld{ 0.08f, 0.08f, 0.08f, 1.0f };
            const ScotlandYard::UI::Color col_txt{ 1, 1, 1, 1 };
            const ScotlandYard::UI::Color col_mut{ 0.8f, 0.83f, 0.9f, 0.85f };
            const ScotlandYard::UI::Color col_accent{ 1.00f, 0.89f, 0.00f, 1.0f };

            static float DistPointToSegment(float px, float py, float ax, float ay, float bx, float by) {
                float vx = bx - ax, vy = by - ay;
                float wx = px - ax, wy = py - ay;
                float c1 = vx * wx + vy * wy;
                if (c1 <= 0.0f) return std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay));
                float c2 = vx * vx + vy * vy;
                if (c2 <= c1) return std::sqrt((px - bx) * (px - bx) + (py - by) * (py - by));
                float t = c1 / c2;
                float projx = ax + t * vx, projy = ay + t * vy;
                return std::sqrt((px - projx) * (px - projx) + (py - projy) * (py - projy));
            }

            static void RasterizeThickSegment(std::vector<uint8_t>& mask, int W, int H,
                float ax, float ay, float bx, float by, float radius) {
                int minx = (int)std::floor(std::min(ax, bx) - radius);
                int maxx = (int)std::ceil(std::max(ax, bx) + radius);
                int miny = (int)std::floor(std::min(ay, by) - radius);
                int maxy = (int)std::ceil(std::max(ay, by) + radius);

                minx = std::max(minx, 0); miny = std::max(miny, 0);
                maxx = std::min(maxx, W - 1); maxy = std::min(maxy, H - 1);

                float r2 = radius * radius;

                for (int y = miny; y <= maxy; ++y) {
                    for (int x = minx; x <= maxx; ++x) {
                        float d = DistPointToSegment((float)x + 0.5f, (float)y + 0.5f, ax, ay, bx, by);
                        if (d * d <= r2) mask[y * W + x] = 1;
                    }
                }
            }

            static void RasterizeCircle(std::vector<uint8_t>& mask, int W, int H,
                float cx, float cy, float radius) {
                int minx = std::max(0, (int)std::floor(cx - radius));
                int maxx = std::min(W - 1, (int)std::ceil(cx + radius));
                int miny = std::max(0, (int)std::floor(cy - radius));
                int maxy = std::min(H - 1, (int)std::ceil(cy + radius));
                float r2 = radius * radius;

                for (int y = miny; y <= maxy; ++y) {
                    for (int x = minx; x <= maxx; ++x) {
                        float dx = ((float)x + 0.5f) - cx;
                        float dy = ((float)y + 0.5f) - cy;
                        if (dx * dx + dy * dy <= r2) mask[y * W + x] = 1;
                    }
                }
            }
        }

        int MapGeneratorState::ValueToXPosition(const Slider& s) const
        {
            float t = (s.f_Value - s.f_MinValue) / (s.f_MaxValue - s.f_MinValue);
            t = std::max(0.0f, std::min(1.0f, t));
            return s.track.x + (int)std::round(t * s.track.w);
        }

        float MapGeneratorState::XPositionToValue(const Slider& s, int mx) const
        {
            float t = (float)(mx - s.track.x) / (float)s.track.w;
            t = std::max(0.0f, std::min(1.0f, t));
            float v = s.f_MinValue + t * (s.f_MaxValue - s.f_MinValue);
            if (s.f_Step > 0.0f)
            {
                v = std::round(v / s.f_Step) * s.f_Step;
            }
            return std::max(s.f_MinValue, std::min(s.f_MaxValue, v));
        }

        void MapGeneratorState::RandomizeSeed()
        {
            unsigned int ui_Seed = static_cast<unsigned int>(time(nullptr));
            std::string s_NewSeed = std::to_string(ui_Seed);

            if (!m_vec_Fields.empty())
            {
                m_vec_Fields[0].s_Value = s_NewSeed;
                std::cout << "[DEBUG] Randomized seed: " << s_NewSeed << std::endl;
            }
        }

        void MapGeneratorState::OnEnter(Core::Application* p_App)
        {
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

            if (!p_App || !p_App->IsTrainingMode()) {
                CreatePreviewQuad();
            }
        }

        void MapGeneratorState::LayoutUI(int W, int H, Core::Application* p_App)
        {
            // Use virtual dimensions for UI layout
            W = p_App->GetVirtualWidth();
            H = p_App->GetVirtualHeight();
            
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

            if (kDrawCardBg)
            {
                DrawRoundedRectScreen((float)cardX, (float)cardY,
                    (float)(cardX + cardW), (float)(cardY + cardH),
                    col_card, 16, p_App);
            }
            else
            {
                cardX = outer;
                cardW = W - 2 * outer;
                cardY = outer;
                cardH = H - 2 * outer;
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

            if (!m_vec_Fields.empty())
            {
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

        void MapGeneratorState::LayoutSliders(int startY, int cardX, int cardW, int pad, int gap)
        {
            const int rowH = 44;
            const int shortW = std::min(cardW - 2 * pad, 520);
            int x = cardX + (cardW - shortW) / 2;
            int y = startY;

            if (m_vec_Sliders.size() >= 4)
            {
                const int smallW = (shortW - 16) / 2;
                const int gapBetween = 16;

                y -= rowH;
                m_vec_Sliders[0].track = SDL_Rect{ x, y + 16, smallW, 12 };

                m_vec_Sliders[3].track = SDL_Rect{ x + smallW + gapBetween, y + 16, smallW, 12 };
                y -= gap;

                for (size_t i = 1; i <= 2; ++i)
                {
                    auto& s = m_vec_Sliders[i];
                    y -= rowH;
                    s.track = SDL_Rect{ x, y + 16, shortW, 12 };
                    y -= gap;
                }
            }
            else
            {
                // Verical layout if there are less then 4 sliders
                for (size_t i = 0; i < m_vec_Sliders.size(); ++i)
                {
                    auto& s = m_vec_Sliders[i];
                    y -= rowH;
                    s.track = SDL_Rect{ x, y + 16, shortW, 12 };
                    y -= gap;
                }
            }
        }

        void MapGeneratorState::Render(Core::Application* p_App)
        {
            if (p_App->IsTrainingMode()) return;  // Skip rendering in headless mode

            m_pApp = p_App;
            
            // Clear the entire framebuffer to prevent black artifacts
            glClearColor(col_bg.r, col_bg.g, col_bg.b, col_bg.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Use virtual dimensions for rendering
            const int W = p_App->GetVirtualWidth();
            const int H = p_App->GetVirtualHeight();

            DrawRoundedRectScreen(0, 0, (float)W, (float)H, col_bg, 0, p_App);
            LayoutUI(W, H, p_App);

            // Dedicated function for texture render
            RenderPreviewTexture(p_App);

            // Render seed field
            for (auto& f : m_vec_Fields)
            {
                auto bg = f.b_Focused ? col_card : col_fld;
                DrawRoundedRectScreen((float)f.rect.x, (float)f.rect.y,
                    (float)(f.rect.x + f.rect.w), (float)(f.rect.y + f.rect.h),
                    bg, 8, p_App);

                std::string shown = f.s_Value;
                if (f.b_Focused && (SDL_GetTicks() / 500) % 2 == 0)
                    shown.push_back('|');

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
            for (const auto& s : m_vec_Sliders)
            {
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
                if (s.f_Step >= 1.0f)
                    std::snprintf(buf, sizeof(buf), "%.0f", s.f_Value);
                else
                    std::snprintf(buf, sizeof(buf), "%.2f", s.f_Value);

                DrawTextCenteredPx(buf, (float)s.track.x, (float)(s.track.y - 18),
                    (float)(s.track.x + s.track.w), (float)(s.track.y - 2),
                    col_txt, p_App, -2.0f);
            }

            // Buttons
            DrawMenuLikeButton(m_BtnGenerate, "GENERATE", p_App);
            DrawMenuLikeButton(m_BtnSave, "PLAY", p_App);
            DrawMenuLikeButton(m_BtnBack, "BACK", p_App);
            DrawMenuLikeButton(m_BtnRandomSeed, "RANDOM", p_App);

            SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
        }

        void MapGeneratorState::FocusField(int idx)
        {
            for (auto& f : m_vec_Fields)
                f.b_Focused = false;
            m_i_FocusedFieldIndex = (idx >= 0 && idx < (int)m_vec_Fields.size()) ? idx : -1;
            if (m_i_FocusedFieldIndex >= 0)
                m_vec_Fields[m_i_FocusedFieldIndex].b_Focused = true;
        }

        void MapGeneratorState::BlurAllFields() { FocusField(-1); }

        void MapGeneratorState::AppendTextToFocusedField(const char* utf8)
        {
            if (m_i_FocusedFieldIndex < 0)
                return;
            auto& f = m_vec_Fields[m_i_FocusedFieldIndex];
            if ((int)f.s_Value.size() >= f.i_MaxLength)
                return;

            for (const char* p = utf8; *p; ++p)
            {
                char c = *p;
                if (f.b_Numeric)
                {
                    if ((c >= '0' && c <= '9') || c == '.' || c == '-')
                        f.s_Value.push_back(c);
                }
                else
                {
                    if ((unsigned char)c >= 32 && (unsigned char)c < 127)
                        f.s_Value.push_back(c);
                }
            }
        }

        void MapGeneratorState::BackspaceInFocusedField()
        {
            if (m_i_FocusedFieldIndex < 0)
                return;
            auto& f = m_vec_Fields[m_i_FocusedFieldIndex];
            if (!f.s_Value.empty())
                f.s_Value.pop_back();
        }

        void MapGeneratorState::TryGenerateMap()
        {
            std::cout << "[MapGen] Generating map..." << std::endl;

            std::string seedStr = m_vec_Fields.empty() ? "" : m_vec_Fields[0].s_Value;
            unsigned int ui_Seed;

            if (seedStr.empty())
            {
                ui_Seed = static_cast<unsigned int>(time(nullptr));
                seedStr = std::to_string(ui_Seed);
                if (!m_vec_Fields.empty())
                {
                    m_vec_Fields[0].s_Value = seedStr;
                }
            }
            else
            {
                try
                {
                    ui_Seed = std::stoul(seedStr);
                }
                catch (...)
                {
                    ui_Seed = static_cast<unsigned int>(time(nullptr));
                }
            }

            srand(ui_Seed);

            int numParks = (int)m_vec_Sliders[0].f_Value;
            float parkMinSize = m_vec_Sliders[1].f_Value;
            float parkMaxSize = m_vec_Sliders[2].f_Value;
            int numBridges = (int)m_vec_Sliders[3].f_Value;

            if (parkMinSize > parkMaxSize)
            {
                std::swap(parkMinSize, parkMaxSize);
                m_vec_Sliders[1].f_Value = parkMinSize;
                m_vec_Sliders[2].f_Value = parkMaxSize;
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

            // Stary generator ulic (dla kompatybilności strukturalnej)
            m_vec_Streets = MapGen::GenerateStreetNetwork(m_vec_GridPoints, m_vec_RiverPath,
                m_vec_Bridges, 5);

            // ----------------------------------------------------
            // INTEGRACJA Z NOWYM HIGHWAY GENERATOR (CityGen)
            // ----------------------------------------------------

            // Przygotowanie maski stref (Rzeka i Parki) dla CityGen
            std::vector<uint8_t> zoneMask((size_t)mapW * (size_t)mapH, 0);

            // Rzeka jako gruba linia w masce
            const float riverHalfWidthPx = 15.0f;
            if (m_vec_RiverPath.size() >= 2) {
                for (size_t i = 1; i < m_vec_RiverPath.size(); ++i) {
                    const auto& a = m_vec_RiverPath[i - 1];
                    const auto& b = m_vec_RiverPath[i];
                    RasterizeThickSegment(zoneMask, mapW, mapH, a.x, a.y, b.x, b.y, riverHalfWidthPx);
                }
            }

            // Parki (zakładamy okrągłe dla uproszczenia maski)
            for (const auto& park : m_vec_Parks) {
                RasterizeCircle(zoneMask, mapW, mapH, park.center.x, park.center.y, park.f_BaseRadius);
            }

            // Uruchomienie HighwayGenerator
            CityGen::HighwayGenerator hg(mapW, mapH);
            hg.SetZoneMask(std::move(zoneMask));
            hg.Generate();

            // Pobranie wyników
            m_vec_HighwayNodes = hg.GetRoadNodes();
            m_vec_HighwayRoads = hg.GetRoads();
            m_vec_Highways = hg.GetHighways();
            m_PopulationDensity = hg.GetPopulationDensity();

            // Zaktualizuj teksturę podglądu natychmiast (w pamięci RAM)
            GeneratePreviewTexture();

            m_s_InfoText = "Map generated!";
            std::cout << "[MapGen] Generation complete!" << std::endl;
        }

        // Nowa metoda rysująca mapę w pamięci i aktualizująca teksturę
        void MapGeneratorState::GeneratePreviewTexture()
        {
            int W = 1200;
            int H = 900;

            // 1. Utwórz powierzchnię SDL (w pamięci)
            SDL_Surface* surface = SDL_CreateRGBSurface(0, W, H, 32,
                0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000); // RGBA

            if (!surface) return;

            SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0, 0, 0));
            

            auto SetPixel = [&](int x, int y, Uint8 r, Uint8 g, Uint8 b)
                {
                    if (x >= 0 && x < W && y >= 0 && y < H)
                    {
                        Uint32 color = SDL_MapRGBA(surface->format, r, g, b, 255);
                        Uint32* pixels = (Uint32*)surface->pixels;
                        pixels[y * W + x] = color;
                    }
                };


            if (!m_PopulationDensity.empty()) {
                for (int y = 0; y < H && y < (int)m_PopulationDensity.size(); ++y) {
                    for (int x = 0; x < W && x < (int)m_PopulationDensity[y].size(); ++x) {
                        float density = m_PopulationDensity[y][x];
                        // Gradient od ciemnego niebieskiego (niska gęstość) do pomarańczowego (wysoka gęstość)
                        Uint8 r = (Uint8)(density * 180.0f + 20.0f);
                        Uint8 g = (Uint8)(density * 80.0f + 10.0f);
                        Uint8 b = (Uint8)(30.0f - density * 20.0f);
                        SetPixel(x, y, r, g, b);
                    }
                }
            } else {
                SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0, 0, 0));
            }

            // 2. Rysuj Parki
            for (const auto& park : m_vec_Parks)
            {
                int minX = (int)(park.center.x - park.f_BaseRadius * 1.5f);
                int maxX = (int)(park.center.x + park.f_BaseRadius * 1.5f);
                int minY = (int)(park.center.y - park.f_BaseRadius * 1.5f);
                int maxY = (int)(park.center.y + park.f_BaseRadius * 1.5f);

                minX = std::max(0, minX); minY = std::max(0, minY);
                maxX = std::min(W - 1, maxX); maxY = std::min(H - 1, maxY);

                for (int y = minY; y <= maxY; ++y)
                {
                    for (int x = minX; x <= maxX; ++x)
                    {
                        if (park.ContainsPoint((float)x, (float)y))
                        {
                            SetPixel(x, y, 34, 139, 34); // Forest Green
                        }
                    }
                }
            }

            // 3. Rysuj Rzekę
            for (const auto& rp : m_vec_RiverPath)
            {
                int cx = (int)rp.x;
                int cy = (int)rp.y;
                for (int dy = -20; dy <= 20; ++dy)
                {
                    for (int dx = -20; dx <= 20; ++dx)
                    {
                        if (dx * dx + dy * dy <= 400)
                        {
                            SetPixel(cx + dx, cy + dy, 50, 150, 255); // Blue
                        }
                    }
                }
            }

            // Funkcja pomocnicza do rysowania linii z grubością
            auto DrawThickLine = [&](int x0, int y0, int x1, int y1, int thickness, Uint8 r, Uint8 g, Uint8 b) {
                int dx = std::abs(x1 - x0);
                int dy = std::abs(y1 - y0);
                int sx = (x0 < x1) ? 1 : -1;
                int sy = (y0 < y1) ? 1 : -1;
                int err = dx - dy;

                int x = x0;
                int y = y0;

                auto DrawDisc = [&](int cx, int cy) {
                    for (int oy = -thickness; oy <= thickness; ++oy) {
                        for (int ox = -thickness; ox <= thickness; ++ox) {
                            if (ox * ox + oy * oy <= thickness * thickness) {
                                SetPixel(cx + ox, cy + oy, r, g, b);
                            }
                        }
                    }
                    };

                while (true) {
                    DrawDisc(x, y);
                    if (x == x1 && y == y1) break;
                    int e2 = 2 * err;
                    if (e2 > -dy) { err -= dy; x += sx; }
                    if (e2 < dx) { err += dx; y += sy; }
                }
                };

            // 4. Rysuj Drogi AI
            for (const auto& road : m_vec_HighwayRoads) {
                if (road.isDeleted) continue;

                if (road.startNodeIdx < 0 || road.startNodeIdx >= m_vec_HighwayNodes.size()) continue;
                if (road.endNodeIdx < 0 || road.endNodeIdx >= m_vec_HighwayNodes.size()) continue;

                const auto& a = m_vec_HighwayNodes[road.startNodeIdx];
                const auto& b = m_vec_HighwayNodes[road.endNodeIdx];

                if (road.type == CityGen::RoadType::HIGHWAY) {
                    DrawThickLine((int)a.x, (int)a.y, (int)b.x, (int)b.y, 2, 255, 140, 0); // Orange
                }
                else {
                    DrawThickLine((int)a.x, (int)a.y, (int)b.x, (int)b.y, 0, 200, 200, 200); // White/Gray
                }
            }

            // 5. Rysuj węzły
            for (const auto& n : m_vec_HighwayNodes) {
                if (!n.isIntersection && n.connectedRoadIndices.size() < 2) continue;

                int cx = (int)n.x, cy = (int)n.y;
                int rad = n.isIntersection ? 3 : 2;

                bool isHighwayNode = false;
                for (int rIdx : n.connectedRoadIndices) {
                    if (rIdx >= 0 && rIdx < m_vec_HighwayRoads.size()) {
                        if (m_vec_HighwayRoads[rIdx].type == CityGen::RoadType::HIGHWAY) {
                            isHighwayNode = true;
                            break;
                        }
                    }
                }

                Uint8 nr = 255;
                Uint8 ng = 255;
                Uint8 nb = isHighwayNode ? 0 : 255; // Yellow for Bus, White for Taxi

                for (int dy = -rad; dy <= rad; ++dy)
                    for (int dx = -rad; dx <= rad; ++dx)
                        if (dx * dx + dy * dy <= rad * rad)
                            SetPixel(cx + dx, cy + dy, nr, ng, nb);
            }

            // 6. Prześlij do tekstury OpenGL
            if (m_GLuint_PreviewTexture) {
                glDeleteTextures(1, &m_GLuint_PreviewTexture);
            }
            glGenTextures(1, &m_GLuint_PreviewTexture);
            glBindTexture(GL_TEXTURE_2D, m_GLuint_PreviewTexture);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glBindTexture(GL_TEXTURE_2D, 0);

            m_i_PreviewWidth = W;
            m_i_PreviewHeight = H;
            m_b_HasPreview = true;

            SDL_FreeSurface(surface);
            std::cout << "[MapGen] Preview texture updated (" << W << "x" << H << ")" << std::endl;
        }

        void MapGeneratorState::SaveMapToFile()
        {
            // Metoda zachowana dla przycisku EXPORT, zapisuje fizyczny plik BMP
            std::string filename = "generated_map.bmp";

            SDL_Surface* surface = SDL_CreateRGBSurface(0, 1200, 900, 24,
                0xFF0000, 0x00FF00, 0x0000FF, 0);

            if (!surface) return;

            SDL_LockSurface(surface);
            SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0, 0, 0));

            // Logika rysowania powtórzona dla pliku (można by zrefaktoryzować, ale trzymamy niezależnie)
            auto SetPixel = [&](int x, int y, Uint8 r, Uint8 g, Uint8 b)
                {
                    if (x >= 0 && x < 1200 && y >= 0 && y < 900) {
                        Uint8* pixels = (Uint8*)surface->pixels;
                        int offset = (y * surface->pitch) + (x * 3);
                        pixels[offset + 0] = b;
                        pixels[offset + 1] = g;
                        pixels[offset + 2] = r;
                    }
                };

            // Rysuj parki
            for (const auto& park : m_vec_Parks) {
                int minX = (int)(park.center.x - park.f_BaseRadius * 1.5f);
                int maxX = (int)(park.center.x + park.f_BaseRadius * 1.5f);
                int minY = (int)(park.center.y - park.f_BaseRadius * 1.5f);
                int maxY = (int)(park.center.y + park.f_BaseRadius * 1.5f);
                for (int y = minY; y <= maxY; ++y) {
                    for (int x = minX; x <= maxX; ++x) {
                        if (park.ContainsPoint((float)x, (float)y)) SetPixel(x, y, 34, 139, 34);
                    }
                }
            }

            // Rysuj rzekę
            for (const auto& rp : m_vec_RiverPath) {
                int cx = (int)rp.x; int cy = (int)rp.y;
                for (int dy = -20; dy <= 20; ++dy) {
                    for (int dx = -20; dx <= 20; ++dx) {
                        if (dx * dx + dy * dy <= 400) SetPixel(cx + dx, cy + dy, 50, 150, 255);
                    }
                }
            }

            // Helper linii
            auto DrawThickLine = [&](int x0, int y0, int x1, int y1, int thickness, Uint8 r, Uint8 g, Uint8 b) {
                int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
                int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
                int err = dx - dy;
                int x = x0, y = y0;
                auto DrawDisc = [&](int cx, int cy) {
                    for (int oy = -thickness; oy <= thickness; ++oy)
                        for (int ox = -thickness; ox <= thickness; ++ox)
                            if (ox * ox + oy * oy <= thickness * thickness) SetPixel(cx + ox, cy + oy, r, g, b);
                    };
                while (true) {
                    DrawDisc(x, y);
                    if (x == x1 && y == y1) break;
                    int e2 = 2 * err;
                    if (e2 > -dy) { err -= dy; x += sx; }
                    if (e2 < dx) { err += dx; y += sy; }
                }
                };

            // Rysuj drogi
            for (const auto& road : m_vec_HighwayRoads) {
                if (road.isDeleted) continue;
                const auto& a = m_vec_HighwayNodes[road.startNodeIdx];
                const auto& b = m_vec_HighwayNodes[road.endNodeIdx];
                if (road.type == CityGen::RoadType::HIGHWAY) DrawThickLine((int)a.x, (int)a.y, (int)b.x, (int)b.y, 3, 255, 140, 0);
                else DrawThickLine((int)a.x, (int)a.y, (int)b.x, (int)b.y, 1, 200, 200, 200);
            }

            // Rysuj węzły
            for (const auto& n : m_vec_HighwayNodes) {
                if (!n.isIntersection && n.connectedRoadIndices.size() < 2) continue;
                int cx = (int)n.x, cy = (int)n.y;
                int rad = n.isIntersection ? 4 : 2;
                bool isHighway = false;
                for (int rIdx : n.connectedRoadIndices)
                    if (m_vec_HighwayRoads[rIdx].type == CityGen::RoadType::HIGHWAY) isHighway = true;

                Uint8 nr = 255, ng = 255, nb = isHighway ? 0 : 255;
                for (int dy = -rad; dy <= rad; ++dy)
                    for (int dx = -rad; dx <= rad; ++dx)
                        if (dx * dx + dy * dy <= rad * rad) SetPixel(cx + dx, cy + dy, nr, ng, nb);
            }

            SDL_UnlockSurface(surface);
            if (SDL_SaveBMP(surface, filename.c_str()) == 0) std::cout << "[MapGen] Map saved to: " << filename << std::endl;
            else std::cout << "[MapGen ERROR] Failed to save: " << SDL_GetError() << std::endl;
            SDL_FreeSurface(surface);
        }

        void MapGeneratorState::RenderMapToTexture()
        {
        }

        void MapGeneratorState::ExportNodesToCSV(const std::string& s_Filename)
        {
            std::ofstream file(s_Filename);

            if (!file.is_open())
            {
                std::cout << "[Export ERROR] Failed to create " << s_Filename << std::endl;
                return;
            }

            file << "id,pos_x,pos_y,station_type\n";

            int i_NodeId = 1; // ID w grze (1-based)

            for (size_t i = 0; i < m_vec_HighwayNodes.size(); ++i)
            {
                const auto& node = m_vec_HighwayNodes[i];

                if (node.connectedRoadIndices.empty()) continue;

                int i_X = static_cast<int>(std::round(node.x));
                int i_Y = static_cast<int>(std::round(node.y));

                std::string stationType = "taxi";
                bool hasHighwayConnection = false;

                for (int rIdx : node.connectedRoadIndices) {
                    if (rIdx >= 0 && rIdx < m_vec_HighwayRoads.size()) {
                        if (m_vec_HighwayRoads[rIdx].type == CityGen::RoadType::HIGHWAY) {
                            hasHighwayConnection = true;
                            break;
                        }
                    }
                }

                if (hasHighwayConnection) {
                    stationType = "bus";
                }

                file << i_NodeId << "," << i_X << "," << i_Y << "," << stationType << "\n";
                i_NodeId++;
            }

            file.close();
            std::cout << "[Export] Exported " << (i_NodeId - 1) << " nodes to " << s_Filename << std::endl;
        }

        void MapGeneratorState::RenderPreviewTexture(Core::Application* p_App)
        {
            if (!m_b_HasPreview || !m_GLuint_PreviewTexture)
            {
                return;
            }

            GLboolean b_DepthWas = glIsEnabled(GL_DEPTH_TEST);
            if (b_DepthWas)
                glDisable(GL_DEPTH_TEST);

            GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
            if (!b_BlendWas)
                glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(m_ShaderProgram_Preview);

            // Use virtual dimensions for orthographic projection
            int i_VirtualWidth = p_App->GetVirtualWidth();
            int i_VirtualHeight = p_App->GetVirtualHeight();

            glm::mat4 projection = glm::ortho(
                0.0f, (float)i_VirtualWidth,
                0.0f, (float)i_VirtualHeight,
                -1.0f, 1.0f);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(
                (float)m_rect_PreviewArea.x,
                (float)m_rect_PreviewArea.y,
                0.0f));
            model = glm::scale(model, glm::vec3(
                (float)m_rect_PreviewArea.w,
                (float)m_rect_PreviewArea.h,
                1.0f));

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

            if (!b_BlendWas)
                glDisable(GL_BLEND);
            if (b_DepthWas)
                glEnable(GL_DEPTH_TEST);
        }

        void MapGeneratorState::CreatePreviewQuad()
        {
            float quadVertices[] = {
                0.0f, 0.0f, 0.0f, 1.0f,
                1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 0.0f,

                0.0f, 0.0f, 0.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f };

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

        void MapGeneratorState::HandleEvent(const SDL_Event& ev, Core::Application* p_App)
        {
            switch (ev.type)
            {
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    ev.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    // Force UI recalculation on window resize (including fullscreen toggle)
                    // The next Render() call will automatically use updated virtual dimensions
                }
                break;

            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_ESCAPE)
                {
                    p_App->GetStateManager()->ChangeState("menu", p_App);
                    return;
                }
                if (ev.key.keysym.sym == SDLK_TAB)
                {
                    int n = (int)m_vec_Fields.size();
                    if (n > 0)
                        FocusField((m_i_FocusedFieldIndex + 1) % n);
                    return;
                }
                if (ev.key.keysym.sym == SDLK_BACKSPACE)
                {
                    BackspaceInFocusedField();
                    return;
                }
                if (ev.key.keysym.sym == SDLK_RETURN)
                {
                    TryGenerateMap();
                    return;
                }
                break;

            case SDL_TEXTINPUT:
                AppendTextToFocusedField(ev.text.text);
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT)
                {
                    // Transform mouse coordinates to virtual space
                    float virtualX, virtualY;
                    p_App->TransformMouseToVirtual(ev.button.x, ev.button.y, virtualX, virtualY);
                    
                    int mx = (int)virtualX;
                    int my = (int)virtualY;
                    
                    // Flip Y coordinate (SDL uses top-left, OpenGL uses bottom-left)
                    my = p_App->GetVirtualHeight() - my;

                    // Check field focus
                    bool focused = false;
                    for (size_t i = 0; i < m_vec_Fields.size(); ++i)
                    {
                        auto& r = m_vec_Fields[i].rect;
                        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h)
                        {
                            FocusField((int)i);
                            focused = true;
                            break;
                        }
                    }
                    if (!focused)
                        BlurAllFields();

                    // Check sliders
                    for (auto& s : m_vec_Sliders)
                    {
                        SDL_Rect hit = { s.track.x - 6, s.track.y - 6, s.track.w + 12, s.track.h + 12 };
                        if (mx >= hit.x && mx <= hit.x + hit.w &&
                            my >= hit.y && my <= hit.y + hit.h)
                        {
                            s.b_Dragging = true;
                            s.f_Value = XPositionToValue(s, mx);
                        }
                    }

                    // Check buttons
                    if (mx >= m_BtnGenerate.x && mx <= m_BtnGenerate.x + m_BtnGenerate.w &&
                        my >= m_BtnGenerate.y && my <= m_BtnGenerate.y + m_BtnGenerate.h)
                    {
                        TryGenerateMap();
                    }
                    if (mx >= m_BtnSave.x && mx <= m_BtnSave.x + m_BtnSave.w &&
                        my >= m_BtnSave.y && my <= m_BtnSave.y + m_BtnSave.h)
                    {
                        std::cout << "[MapGen] PLAY clicked. Generating and switching..." << std::endl;

                        // 1. Generuj (aby dane były świeże)
                        TryGenerateMap();

                        // 2. Zapisz bitmapę (potrzebne dla shadera terenu w EmptyEnvironmentState)
                        SaveMapToFile();

                        // 3. Utwórz nowy stan środowiska
                        auto pGameEnv = std::make_unique<ScotlandYard::States::EmptyEnvironmentState>();

                        // 4. Wstrzyknij dane bezpośrednio do obiektu
                        pGameEnv->InjectMapData(
                            m_vec_HighwayNodes,
                            m_vec_HighwayRoads,
                            m_vec_Parks,
                            m_vec_RiverPath
                        );

                        // 5. Zarejestruj stan i przełącz
                        // Uwaga: używamy move, bo unique_ptr nie może być kopiowany
                        p_App->GetStateManager()->RegisterState("emptyenv", std::move(pGameEnv));
                        p_App->GetStateManager()->ChangeState("emptyenv", p_App);

                        return;
                    }
                    if (mx >= m_BtnBack.x && mx <= m_BtnBack.x + m_BtnBack.w &&
                        my >= m_BtnBack.y && my <= m_BtnBack.y + m_BtnBack.h)
                    {
                        p_App->GetStateManager()->ChangeState("menu", p_App);
                    }
                    if (mx >= m_BtnRandomSeed.x && mx <= m_BtnRandomSeed.x + m_BtnRandomSeed.w &&
                        my >= m_BtnRandomSeed.y && my <= m_BtnRandomSeed.y + m_BtnRandomSeed.h)
                    {
                        RandomizeSeed();
                    }
                }
                break;

            case SDL_MOUSEMOTION:
                if (ev.motion.state & SDL_BUTTON_LMASK)
                {
                    // Transform mouse coordinates to virtual space
                    float virtualX, virtualY;
                    p_App->TransformMouseToVirtual(ev.motion.x, ev.motion.y, virtualX, virtualY);
                    
                    int mx = (int)virtualX;
                    int my = p_App->GetVirtualHeight() - (int)virtualY;
                    
                    for (auto& s : m_vec_Sliders)
                    {
                        if (s.b_Dragging)
                        {
                            s.f_Value = XPositionToValue(s, mx);
                        }
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT)
                {
                    for (auto& s : m_vec_Sliders)
                        s.b_Dragging = false;
                }
                break;

            default:
                break;
            }
        }

        void MapGeneratorState::ExportMapInfoToCSV(const std::string& s_Filename)
        {
            std::ofstream file(s_Filename);

            if (!file.is_open())
            {
                std::cout << "[Export ERROR] Failed to create " << s_Filename << std::endl;
                return;
            }

            file << "# Generated Map Information\n";
            file << "# Seed: " << (m_vec_Fields.empty() ? "none" : m_vec_Fields[0].s_Value) << "\n";
            file << "# Map Size: " << "1200x900" << "\n";
            file << "\n";

            file << "# RIVER INFORMATION\n";
            file << "river_type,";
            if (m_i_CurrentCorner == 4)
            {
                file << "center\n";
            }
            else
            {
                file << "corner_" << m_i_CurrentCorner << "\n";
            }
            file << "river_width," << MapGen::Config::RIVER_WIDTH << "\n";
            file << "river_points," << m_vec_RiverPath.size() << "\n";
            file << "\n";

            file << "# River Path (x,y coordinates)\n";
            file << "river_point_id,x,y\n";
            for (size_t i = 0; i < m_vec_RiverPath.size(); ++i)
            {
                file << i << ","
                    << static_cast<int>(m_vec_RiverPath[i].x) << ","
                    << static_cast<int>(m_vec_RiverPath[i].y) << "\n";
            }
            file << "\n";

            file << "# PARKS INFORMATION\n";
            file << "total_parks," << m_vec_Parks.size() << "\n";
            file << "\n";

            file << "# Park Details\n";
            file << "park_id,center_x,center_y,radius,num_sides\n";
            for (size_t i = 0; i < m_vec_Parks.size(); ++i)
            {
                const auto& park = m_vec_Parks[i];
                file << i << ","
                    << static_cast<int>(park.center.x) << ","
                    << static_cast<int>(park.center.y) << ","
                    << static_cast<int>(park.f_BaseRadius) << ","
                    << park.i_NumSides << "\n";
            }
            file << "\n";

            file << "# Park Vertices (detailed polygons)\n";
            file << "park_id,vertex_id,x,y\n";
            for (size_t i = 0; i < m_vec_Parks.size(); ++i)
            {
                const auto& park = m_vec_Parks[i];
                for (size_t v = 0; v < park.vec_Vertices.size(); ++v)
                {
                    file << i << "," << v << ","
                        << static_cast<int>(park.vec_Vertices[v].x) << ","
                        << static_cast<int>(park.vec_Vertices[v].y) << "\n";
                }
            }
            file << "\n";

            if (!m_vec_Bridges.empty())
            {
                file << "# BRIDGES INFORMATION\n";
                file << "total_bridges," << m_vec_Bridges.size() << "\n";
                file << "\n";

                file << "# Bridge Endpoints\n";
                file << "bridge_id,node1_x,node1_y,node2_x,node2_y\n";
                for (size_t i = 0; i < m_vec_Bridges.size(); ++i)
                {
                    const auto& bridge = m_vec_Bridges[i];
                    file << i << ","
                        << static_cast<int>(bridge.first.x) << ","
                        << static_cast<int>(bridge.first.y) << ","
                        << static_cast<int>(bridge.second.x) << ","
                        << static_cast<int>(bridge.second.y) << "\n";
                }
            }

            file.close();
            std::cout << "[Export] Map information exported to " << s_Filename << std::endl;
        }

    }
} // namespaces