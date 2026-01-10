#pragma once

#include "IGameState.h"
#include "MapGenerator.h"
#include "HighwayGenerator.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <GL/glew.h>

namespace ScotlandYard { namespace Core { class Application; } }

namespace ScotlandYard {
    namespace States {

        class MapGeneratorState : public Core::IGameState {
        public:
            MapGeneratorState() = default;
            ~MapGeneratorState() override = default;

            void OnEnter(Core::Application* p_App) override;
            void OnExit(Core::Application* p_App) override {}
            void OnPause() override {}
            void OnResume() override {}
            void Update(float) override {}
            void Render(Core::Application* p_App) override;
            void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;

        private:
            struct Field {
                std::string s_Label;
                std::string s_Value;
                SDL_Rect rect{};
                bool b_Focused = false;
                bool b_Numeric = false;
                int i_MaxLength = 16;
            };

            struct Slider {
                std::string s_Label;
                float f_Value;
                float f_MinValue, f_MaxValue;
                float f_Step;
                SDL_Rect track{};
                bool b_Dragging = false;
                int i_KnobWidth = 14;
            };

            struct BuildingFootprint
            {
                std::vector<MapGen::Point> basePoints;// 4 points in order
                float height;
                bool hasGableRoof;

                BuildingFootprint() : height(0.0f), hasGableRoof(false) {}
            };

            Core::Application* m_pApp;

            std::vector<Field> m_vec_Fields;
            std::vector<Slider> m_vec_Sliders;
            int m_i_FocusedFieldIndex = -1;

            SDL_Rect m_BtnGenerate{};
            SDL_Rect m_BtnSave{};
            SDL_Rect m_BtnBack{};
            SDL_Rect m_BtnRandomSeed{};

            std::string m_s_InfoText;

            GLuint m_GLuint_PreviewTexture = 0;
            SDL_Rect m_rect_PreviewArea{};
            bool m_b_HasPreview = false;
            int m_i_PreviewWidth = 0;
            int m_i_PreviewHeight = 0;

            std::vector<MapGen::Point> m_vec_GridPoints;
            std::vector<MapGen::Point> m_vec_RiverPath;
            std::vector<MapGen::Point> m_vec_ControlPoints;
            std::vector<MapGen::Park> m_vec_Parks;
            int m_i_CurrentCorner = 0;
            std::vector<std::pair<MapGen::Point, MapGen::Point>> m_vec_Bridges;

            std::vector<MapGen::Edge> m_vec_Streets;

            std::vector<CityGen::Point> m_vec_HighwayNodes;
            std::vector<CityGen::Road>  m_vec_HighwayRoads;
            std::vector<CityGen::Highway> m_vec_Highways;
            std::vector<int> m_vec_StreetBranchNodes; // Indeksy węzłów rozgałęzień Streets (czerwone)
            std::vector<int> m_vec_HighwayEndpointNodes; // Indeksy węzłów końców Highways (zielone)
            std::vector<std::vector<float>> m_PopulationDensity;
            std::vector<CityGen::GameConnection> m_vec_GameConnections; // Sieć połączeń gry
            bool m_bShowGameConnections = false; // Czy wyświetlać połączenia gry

            std::vector<BuildingFootprint> m_vec_Buildings;
            std::vector<uint8_t> m_buildingMask;


            GLuint m_VAO_PreviewQuad = 0;
            GLuint m_VBO_PreviewQuad = 0;
            GLuint m_ShaderProgram_Preview = 0;

        private:
            void LayoutUI(int i_Width, int i_Height, Core::Application* p_App);
            void LayoutSliders(int i_StartY, int i_CardX, int i_CardWidth, int i_Padding, int i_Gap);
            float XPositionToValue(const Slider& slider, int i_MouseX) const;
            int ValueToXPosition(const Slider& slider) const;
            void FocusField(int i_Index);
            void BlurAllFields();
            void AppendTextToFocusedField(const char* p_Utf8Text);
            void BackspaceInFocusedField();

            void TryGenerateMap();
            void RandomizeSeed();

            void GenerateBuildingFootprints(int mapW, int mapH);

            // Metody do obs�ugi tekstury i zapisu
            void GeneratePreviewTexture();
            void RenderPreviewTexture(Core::Application* p_App);
            void CreatePreviewQuad();

            void SaveMapToFile();
            void RenderMapToTexture();

            void ExportNodesToCSV(const std::string& s_Filename);
            void ExportMapInfoToCSV(const std::string& s_Filename);
        };

    } // namespace States
} // namespace ScotlandYard