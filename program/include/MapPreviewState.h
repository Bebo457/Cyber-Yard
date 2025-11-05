#pragma once

#include "IGameState.h"
#include "MapGenerator.h"
#include <SDL2/SDL.h>
#include <vector>

namespace ScotlandYard { namespace Core { class Application; } }

namespace ScotlandYard {
namespace States {

class MapPreviewState : public Core::IGameState {
public:
    MapPreviewState(const std::vector<MapGen::Point>& vec_GridPoints,
                    const std::vector<MapGen::Point>& vec_RiverPath,
                    const std::vector<MapGen::Park>& vec_Parks,
                    int i_Width, int i_Height);
    ~MapPreviewState() override;

    void OnEnter() override;
    void OnExit() override;
    void OnPause() override {}
    void OnResume() override {}
    void Update(float f_DeltaTime) override {}
    void Render(Core::Application* p_App) override;
    void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;

private:
    std::vector<MapGen::Point> m_vec_GridPoints;
    std::vector<MapGen::Point> m_vec_RiverPath;
    std::vector<MapGen::Park> m_vec_Parks;
    
    SDL_Surface* m_pSurface;
    GLuint m_GLuint_Texture;
    int m_i_MapWidth;
    int m_i_MapHeight;
    
    void CreateMapTexture();
    void SetPixel(int x, int y, Uint8 r, Uint8 g, Uint8 b);
};

} // namespace States
} // namespace ScotlandYard
