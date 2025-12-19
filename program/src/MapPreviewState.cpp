#include "MapPreviewState.h"
#include "Application.h"
#include "StateManager.h"
#include <GL/glew.h>
#include <iostream>

namespace ScotlandYard {
namespace States {

MapPreviewState::MapPreviewState(const std::vector<MapGen::Point>& vec_GridPoints,
                                 const std::vector<MapGen::Point>& vec_RiverPath,
                                 const std::vector<MapGen::Park>& vec_Parks,
                                 int i_Width, int i_Height)
    : m_vec_GridPoints(vec_GridPoints)
    , m_vec_RiverPath(vec_RiverPath)
    , m_vec_Parks(vec_Parks)
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
    std::cout << "[MapPreview] Creating map texture..." << std::endl;
    CreateMapTexture();
}

void MapPreviewState::OnExit(Core::Application* p_App) {
    // Cleanup
}

void MapPreviewState::CreateMapTexture() {
    m_pSurface = SDL_CreateRGBSurface(0, m_i_MapWidth, m_i_MapHeight, 24,
                                      0xFF0000, 0x00FF00, 0x0000FF, 0);
    
    if (!m_pSurface) {
        std::cout << "[MapPreview ERROR] Failed to create surface!" << std::endl;
        return;
    }
    
    SDL_LockSurface(m_pSurface);
    
    SDL_FillRect(m_pSurface, nullptr, SDL_MapRGB(m_pSurface->format, 220, 220, 220));
    
    auto SetPixel = [&](int x, int y, Uint8 r, Uint8 g, Uint8 b) {
        if (x >= 0 && x < m_i_MapWidth && y >= 0 && y < m_i_MapHeight) {
            Uint8* pixels = (Uint8*)m_pSurface->pixels;
            int offset = (y * m_pSurface->pitch) + (x * 3);
            pixels[offset + 0] = b; 
            pixels[offset + 1] = g;
            pixels[offset + 2] = r;
        }
    };
    
    std::cout << "[MapPreview] Drawing " << m_vec_Parks.size() << " parks..." << std::endl;
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
    
    std::cout << "[MapPreview] Drawing river..." << std::endl;
    for (const auto& rp : m_vec_RiverPath) {
        int cx = static_cast<int>(rp.x);
        int cy = static_cast<int>(rp.y);
        for (int dx = -MapGen::Config::RIVER_WIDTH/2; dx <= MapGen::Config::RIVER_WIDTH/2; dx++) {
            for (int dy = -MapGen::Config::RIVER_WIDTH/2; dy <= MapGen::Config::RIVER_WIDTH/2; dy++) {
                if (dx*dx + dy*dy <= (MapGen::Config::RIVER_WIDTH/2)*(MapGen::Config::RIVER_WIDTH/2)) {
                    SetPixel(cx + dx, cy + dy, 50, 150, 255);
                }
            }
        }
    }
    
    std::cout << "[MapPreview] Drawing grid points..." << std::endl;
    const int POINT_RADIUS = 3;
    for (const auto& p : m_vec_GridPoints) {
        for (int dx = -POINT_RADIUS; dx <= POINT_RADIUS; dx++) {
            for (int dy = -POINT_RADIUS; dy <= POINT_RADIUS; dy++) {
                if (dx*dx + dy*dy <= POINT_RADIUS * POINT_RADIUS) {
                    SetPixel(static_cast<int>(p.x) + dx, static_cast<int>(p.y) + dy, 255, 255, 255);
                }
            }
        }
    }
    
    SDL_UnlockSurface(m_pSurface);
    
    SDL_SaveBMP(m_pSurface, "generated_map.bmp");
    std::cout << "[MapPreview] Map saved to generated_map.bmp" << std::endl;
    std::cout << "[MapPreview] Map texture created successfully!" << std::endl;
}

void MapPreviewState::Render(Core::Application* p_App) {
    if (p_App->IsTrainingMode()) return;  // Skip rendering in headless mode

    if (!m_pSurface) {
        return;
    }

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    
    std::cout << "[MapPreview] Map displayed. Press ESC or BACKSPACE to return." << std::endl;
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
