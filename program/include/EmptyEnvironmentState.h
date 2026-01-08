#pragma once

#include "IGameState.h"
#include "WaterRenderer.h"
#include "PolygonRenderer.h"
#include "GeneratedMapData.h"
// Dodajemy nag��wki potrzebne do typ�w danych z generatora
#include "HighwayGenerator.h"
#include "MapGenerator.h"

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <vector>

namespace ScotlandYard { namespace Core { class Application; } }

namespace ScotlandYard {
    namespace States {

        class EmptyEnvironmentState : public Core::IGameState {
        public:
            EmptyEnvironmentState();
            ~EmptyEnvironmentState() override;

            // --- NOWA METODA: Wstrzykiwanie danych z generatora ---
            void InjectMapData(
                const std::vector<CityGen::Point>& vec_Nodes,
                const std::vector<CityGen::Road>& vec_Roads,
                const std::vector<MapGen::Park>& vec_Parks,
                const std::vector<MapGen::Point>& vec_RiverPath
            );
            // -----------------------------------------------------

            // Bridge utilities
            void SetBridgeLength(float f_LengthWorld);

            void OnEnter(Core::Application* p_App) override;
            void OnExit(Core::Application* p_App) override;
            void OnPause() override;
            void OnResume() override;
            void Update(float f_DeltaTime) override;
            void Render(Core::Application* p_App) override;
            void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;

        private:
            // Rendering
            GLuint m_VAO_Plane = 0;
            GLuint m_VBO_Plane = 0;
            GLuint m_ShaderProgram = 0;
            GLuint m_ShaderBridge = 0;

            GLuint m_TexSidewalk = 0;
            GLuint m_TexGrass = 0;
            GLuint m_TexMask = 0;
            bool   m_b_UseMask = false;

            // Road mesh
            GLuint m_VAO_Road = 0;
            GLuint m_VBO_Road = 0;
            GLuint m_EBO_Road = 0;
            GLuint m_ShaderRoad = 0;
            int    m_RoadIndexCount = 0;

            // Road material/texture
            GLuint m_TexRoad = 0;

            // Bridge mesh
            GLuint m_VAO_Bridge = 0;
            GLuint m_VBO_Bridge = 0;
            GLuint m_EBO_Bridge = 0;
            int    m_BridgeIndexCount = 0;
            glm::vec3 m_vec3_BridgePosition{ 11.0f, 0.0f, 8.0f };
            glm::vec3 m_vec3_BridgeBaseScale{ 0.165f, 0.165f, 0.165f };
            glm::vec3 m_vec3_BridgeScale{ 0.165f, 0.165f, 0.165f };
            float m_f_BridgeModelLength = 1.0f; // original length of bridge model along X (GLB space)
            GLuint m_TexBridge = 0;
            bool   m_b_BridgeHasTexture = false;

            // Water renderer
            std::unique_ptr<Rendering::WaterRenderer> m_p_WaterRenderer;
            float m_f_Time = 0.0f;
            glm::mat4 m_mat4_GlobalScaleMatrix = glm::mat4(1.0f);

            // Polygon renderers for parks and zones
            std::vector<std::unique_ptr<Rendering::PolygonRenderer>> m_vec_ParkRenderers;

            // River polygon renderer
            std::unique_ptr<Rendering::PolygonRenderer> m_p_RiverRenderer;

            // Game state
            bool m_b_GameActive = true;

            // Camera system (mirrors GameState behavior)
            bool m_b_Camera3D = true;
            glm::vec3 m_vec3_CameraPosition{ 0.0f, 2.2f, 6.0f };
            glm::vec3 m_vec3_CameraVelocity{ 0.0f, 0.0f, 0.0f };
            glm::vec3 m_vec3_CameraFront{ 0.0f, -0.3f, -1.0f };
            glm::vec3 m_vec3_CameraUp{ 0.0f, 1.0f, 0.0f };
            float m_f_CameraAngle{ -0.2915f };
            float m_f_CameraAngleVelocity{ 0.0f };
            glm::vec3 m_vec3_Saved3DCameraPosition{ 11.0f, 2.2f, 12.0f };

            // Camera constants
            static constexpr float k_CameraScrollAcceleration = 0.003f;
            static constexpr float k_CameraScrollFriction = 0.90f;
            static constexpr float k_CameraScrollToForwardRatio = 8.0f;
            static constexpr float k_CameraAcceleration = 12.0f;
            static constexpr float k_MaxCameraSpeed = 80.0f;
            static constexpr float k_CameraFriction = 0.90f;
            static constexpr float k_MinCameraAngle = -1.55f;
            static constexpr float k_MaxCameraAngle = -0.2915f;

            // Map data
            MapGen::GeneratedMapData m_MapData;
            bool m_b_MapDataLoaded = false;
            bool m_b_RiverStripLoaded = false;

            // Private methods
            void CreatePlane();
            void CreateShaders();
            void TryLoadGeneratedMap(Core::Application* p_App);
            void LoadPolygonData(Core::Application* p_App);
            void LoadSampleMapData();
            void BuildRiverFromMapData();
            void BuildFallbackRiver();
            void RenderMapData(Core::Application* p_App);
            void LoadBridgeModel(Core::Application* p_App);
            void RenderText(const std::string& s_Text, float f_X, float f_Y, float f_Scale,
                float f_R, float f_G, float f_B, Core::Application* p_App);

            void AccelerateCameraForward(float f_DeltaTime);
            void AccelerateCameraBackward(float f_DeltaTime);
            void AccelerateCameraLeft(float f_DeltaTime);
            void AccelerateCameraRight(float f_DeltaTime);
            void UpdateCameraPhysics(float f_DeltaTime);
            void CreateTestRoad(Core::Application* p_App);
        };

    } // namespace States
} // namespace ScotlandYard