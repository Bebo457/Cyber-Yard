#include "MapPreviewState.h"
#include "Application.h"
#include "StateManager.h"
#include <GL/glew.h>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace ScotlandYard {
    namespace States {

        MapPreviewState::MapPreviewState(const std::vector<MapGen::Point>& vec_GridPoints,
            const std::vector<MapGen::Point>& vec_RiverPath,
            const std::vector<MapGen::Park>& vec_Parks,
            const std::vector<CityGen::Point>& vec_HighwayNodes,
            const std::vector<CityGen::Road>& vec_HighwayRoads,
            int i_Width, int i_Height)
            : m_vec_GridPoints(vec_GridPoints)
            , m_vec_RiverPath(vec_RiverPath)
            , m_vec_Parks(vec_Parks)
            , m_vec_HighwayNodes(vec_HighwayNodes)
            , m_vec_HighwayRoads(vec_HighwayRoads)
            , m_pSurface(nullptr)
            , m_GLuint_Texture(0)
            , m_i_MapWidth(i_Width)
            , m_i_MapHeight(i_Height)
        {
        }

        MapPreviewState::~MapPreviewState() {
            if (m_pSurface) {
                SDL_FreeSurface(m_pSurface);
            }
            if (m_GLuint_Texture) {
                glDeleteTextures(1, &m_GLuint_Texture);
            }
        }

        void MapPreviewState::OnEnter(Core::Application* p_App) {
            std::cout << "[MapPreview] Creating detailed map texture..." << std::endl;
            CreateMapTexture();
        }

        void MapPreviewState::OnExit(Core::Application* p_App) {
            // Cleanup
        }

        void MapPreviewState::CreateMapTexture() {
            m_pSurface = SDL_CreateRGBSurface(0, m_i_MapWidth, m_i_MapHeight, 32,
                0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000); // RGBA

            if (!m_pSurface) {
                std::cout << "[MapPreview ERROR] Failed to create surface!" << std::endl;
                return;
            }

            SDL_FillRect(m_pSurface, nullptr, SDL_MapRGB(m_pSurface->format, 0, 0, 0));

            auto SetPixel = [&](int x, int y, Uint8 r, Uint8 g, Uint8 b) {
                if (x >= 0 && x < m_i_MapWidth && y >= 0 && y < m_i_MapHeight) {
                    Uint32 color = SDL_MapRGBA(m_pSurface->format, r, g, b, 255);
                    Uint32* pixels = (Uint32*)m_pSurface->pixels;
                    pixels[y * m_i_MapWidth + x] = color;
                }
            };

            // 1. Rysuj Parki
            for (const auto& park : m_vec_Parks) {
                int i_MinX = static_cast<int>(park.center.x - park.f_BaseRadius * 1.5f);
                int i_MaxX = static_cast<int>(park.center.x + park.f_BaseRadius * 1.5f);
                int i_MinY = static_cast<int>(park.center.y - park.f_BaseRadius * 1.5f);
                int i_MaxY = static_cast<int>(park.center.y + park.f_BaseRadius * 1.5f);

                i_MinX = std::max(0, i_MinX); i_MinY = std::max(0, i_MinY);
                i_MaxX = std::min(m_i_MapWidth - 1, i_MaxX); i_MaxY = std::min(m_i_MapHeight - 1, i_MaxY);

                for (int y = i_MinY; y <= i_MaxY; ++y) {
                    for (int x = i_MinX; x <= i_MaxX; ++x) {
                        if (park.ContainsPoint(static_cast<float>(x), static_cast<float>(y))) {
                            SetPixel(x, y, 34, 139, 34); // Forest Green
                        }
                    }
                }
            }

            // 2. Rysuj Rzekê
            for (const auto& rp : m_vec_RiverPath) {
                int cx = static_cast<int>(rp.x);
                int cy = static_cast<int>(rp.y);
                for (int dx = -20; dx <= 20; dx++) {
                    for (int dy = -20; dy <= 20; dy++) {
                        if (dx * dx + dy * dy <= 400) {
                            SetPixel(cx + dx, cy + dy, 50, 150, 255); // Blue
                        }
                    }
                }
            }

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

            // 3. Rysuj Drogi
            std::cout << "[MapPreview] Drawing " << m_vec_HighwayRoads.size() << " roads..." << std::endl;
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

            // 4. Rysuj Wêz³y
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

                Uint8 nr = 255, ng = 255, nb = isHighwayNode ? 0 : 255;

                for (int dy = -rad; dy <= rad; ++dy)
                    for (int dx = -rad; dx <= rad; ++dx)
                        if (dx * dx + dy * dy <= rad * rad)
                            SetPixel(cx + dx, cy + dy, nr, ng, nb);
            }

            // Create OpenGL Texture
            if (m_GLuint_Texture) glDeleteTextures(1, &m_GLuint_Texture);
            glGenTextures(1, &m_GLuint_Texture);
            glBindTexture(GL_TEXTURE_2D, m_GLuint_Texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_i_MapWidth, m_i_MapHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pSurface->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            std::cout << "[MapPreview] Map texture created successfully!" << std::endl;
        }

        void MapPreviewState::Render(Core::Application* p_App) {
            if (p_App->IsTrainingMode()) return;

            if (!m_GLuint_Texture) return;

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);


            // U¿ywamy GetHUDTextureShader dla uproszczenia
            GLuint shader = p_App->GetHUDTextureShader();
            if (shader) {
                glUseProgram(shader);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_GLuint_Texture);
                glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
                glUniform4f(glGetUniformLocation(shader, "uColor"), 1, 1, 1, 1);

                // Full screen quad
                float verts[] = {
                    -1, -1, 0, 1,   1, -1, 1, 1,   -1,  1, 0, 0,
                     1, -1, 1, 1,   1,  1, 1, 0,   -1,  1, 0, 0
                };

                glBindVertexArray(p_App->GetHUDTextureVAO());
                glBindBuffer(GL_ARRAY_BUFFER, p_App->GetHUDTextureVBO());
                glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
                glUseProgram(0);
            }


            SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
        }

        void MapPreviewState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) {
                    std::cout << "[MapPreview] Returning to generator..." << std::endl;
                    p_App->GetStateManager()->PopState(p_App);
                }
            }
        }

    } // namespace States
} // namespace ScotlandYard