#pragma once

#include "IGameState.h"
#include "MapGenerator.h"
#include "HighwayGenerator.h" 
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
                const std::vector<CityGen::Point>& vec_HighwayNodes,
                const std::vector<CityGen::Road>& vec_HighwayRoads,
                int i_Width, int i_Height);
            ~MapPreviewState() override;

            void OnEnter(Core::Application* p_App) override;
            void OnExit(Core::Application* p_App) override;
            void OnPause() override {}
            void OnResume() override {}
            void Update(float f_DeltaTime) override {}
            void Render(Core::Application* p_App) override;
            void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;

        private:
            std::vector<MapGen::Point> m_vec_GridPoints;
            std::vector<MapGen::Point> m_vec_RiverPath;
            std::vector<MapGen::Park> m_vec_Parks;

            std::vector<CityGen::Point> m_vec_HighwayNodes;
            std::vector<CityGen::Road> m_vec_HighwayRoads;

            SDL_Surface* m_pSurface;
            GLuint m_GLuint_Texture;
            int m_i_MapWidth;
            int m_i_MapHeight;

            void CreateMapTexture();
        };

    } // namespace States
} // namespace ScotlandYard