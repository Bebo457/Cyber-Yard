#pragma once

#include "IGameState.h"
#include "WaterRenderer.h"
#include "PolygonRenderer.h"
#include "GeneratedMapData.h"
#include "HighwayGenerator.h"
#include "MapGenerator.h"
#include "BuildingGenerator.h"
#include "GraphManager.h"
#include "TreeGenerator.h" // Include tree generator definitions
#include "TreePlacement.h" // Include tree placement definitions

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <vector>
#include <random>

namespace ScotlandYard { namespace Core { class Application; } }

namespace ScotlandYard {
    namespace States {

        class EmptyEnvironmentState : public Core::IGameState {
        public:
            EmptyEnvironmentState();
            ~EmptyEnvironmentState() override;

            // --- Wstrzykiwanie danych z generatora ---
            void InjectMapData(
                const std::vector<CityGen::Point>& vec_Nodes,
                const std::vector<CityGen::Road>& vec_Roads,
                const std::vector<MapGen::Park>& vec_Parks,
                const std::vector<MapGen::Point>& vec_RiverPath,
                const std::vector<CityGen::Highway>& vec_Highways,
                const std::vector<MapGen::BuildingData>& vec_Buildings = std::vector<MapGen::BuildingData>()
            );

            // --- Wstrzykiwanie danych o drzewach ---
            void InjectTreeData(const std::vector<Core::TreeInstance>& vec_Trees);

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
            GLuint m_VAO_Frame = 0;
            GLuint m_VBO_Frame = 0;
            GLuint m_ShaderProgram = 0;
            GLuint m_ShaderBridge = 0;

            GLuint m_TexSidewalk = 0;
            GLuint m_TexGrass = 0;
            GLuint m_TexMask = 0;
            bool   m_b_UseMask = false;
            bool   m_b_RenderTestRoad = false;

            // Road mesh
            GLuint m_VAO_Road = 0;
            GLuint m_VBO_Road = 0;
            GLuint m_EBO_Road = 0;
            GLuint m_ShaderRoad = 0;
            int    m_RoadIndexCount = 0;

            // Road material/texture
            GLuint m_TexRoad = 0;
            GLuint m_TexHighway = 0;

            struct RoadMesh {
                GLuint VAO;
                GLuint VBO;
                GLuint EBO;
                int indexCount;
            };
            std::vector<RoadMesh> m_RoadMeshes;
            std::vector<RoadMesh> m_HighwayMeshes;

            std::vector<RoadMesh> m_ParkPathMeshes;
            void BuildParkPathsFromMapData();

            std::vector<CityGen::Point> m_vec_HighwayNodes;
            std::vector<CityGen::Road>  m_vec_HighwayRoads;
            std::vector<CityGen::Highway> m_vec_Highways;

            // Bridge mesh
            GLuint m_VAO_Bridge = 0;
            GLuint m_VBO_Bridge = 0;
            GLuint m_EBO_Bridge = 0;
            int    m_BridgeIndexCount = 0;
            glm::vec3 m_vec3_BridgePosition{ 11.0f, 0.0f, 8.0f };
            glm::vec3 m_vec3_BridgeBaseScale{ 0.165f, 0.165f, 0.165f };
            glm::vec3 m_vec3_BridgeScale{ 0.165f, 0.165f, 0.165f };
            float m_f_BridgeModelLength = 1.0f;
            GLuint m_TexBridge = 0;
            bool   m_b_BridgeHasTexture = false;
            bool   m_b_DrawBridge = false;

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

            // Transport stations rendering
            struct StationCircle {
                glm::vec2 position;
                std::vector<std::string> transportTypes;
                int stationID;
            };
            std::vector<StationCircle> m_vec_CircleStations;
            struct PlayerToken {
                int i_StationID = -1;
                glm::vec3 vec3_Color{ 1.0f, 1.0f, 1.0f };
                bool b_IsMrX = false;
            };
            std::vector<PlayerToken> m_vec_PlayerTokens;
            GLuint m_VAO_Circle = 0;
            GLuint m_VBO_Circle = 0;
            GLuint m_ShaderCircle = 0;
            int m_i_CircleVertexCount = 0;
            float m_f_GlobalScale = 0.1f;
            GLuint m_VAO_PlayerCylinder = 0;
            GLuint m_VBO_PlayerCylinder = 0;
            int m_i_PlayerCylinderVertexCount = 0;
            GLuint m_VAO_PlayerHemisphere = 0;
            GLuint m_VBO_PlayerHemisphere = 0;
            int m_i_PlayerHemisphereVertexCount = 0;

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
            static constexpr float k_CameraAcceleration = 47.0f;
            static constexpr float k_MaxCameraSpeed = 800.0f;
            static constexpr float k_CameraFriction = 0.90f;
            static constexpr float k_MinCameraAngle = -1.55f;
            static constexpr float k_MaxCameraAngle = -0.2915f;

            // Map data
            MapGen::GeneratedMapData m_MapData;
            bool m_b_MapDataLoaded = false;
            bool m_b_RiverStripLoaded = false;

            struct SurfaceBuffers {
                GLuint vao = 0;
                GLuint vboPos = 0;
                GLuint vboNormal = 0;
                GLuint vboUV = 0;
                GLuint ebo = 0;
            };

            struct BuildingRenderInstance {
                Core::BuildingMesh mesh;
                SurfaceBuffers meshBuffers;
                std::vector<SurfaceBuffers> windowSurfaces;
                std::vector<SurfaceBuffers> doorSurfaces;
                glm::vec3 position{ 11.0f, 0.0f, 8.0f };
                float unitScale = 1.0f;
                bool ready = false;
                GLuint facadeTexture = 0;
                GLuint windowTexture = 0;
                GLuint doorTexture = 0;
            };

            // Tree rendering structures
            struct TreeRenderObj {
                Core::TreeMesh mesh;
                SurfaceBuffers buffers;
                glm::mat4 modelMatrix;
                int materialCount = 0;
            };
            std::vector<Core::TreeInstance> m_vec_TreeData; // Raw data from generator
            std::vector<TreeRenderObj> m_RenderTrees;       // GPU objects
            GLuint m_ShaderTree = 0;
            GLuint m_TexTreeTrunk = 0;
            GLuint m_TexTreeCrown = 0;

            BuildingRenderInstance m_ShowcaseBuilding;
            bool m_b_ShowcaseBuildingVisible = false;
            std::vector<BuildingRenderInstance> m_GeneratedBuildings;
            GLuint m_ShaderBuilding = 0;
            std::vector<GLuint> m_vecFacadeTextures;
            std::vector<GLuint> m_vecWindowTextures;
            std::vector<GLuint> m_vecDoorTextures;
            GLuint m_TexBuildingRoof = 0;
            std::mt19937 m_Rng;

            // Private methods
            void CreatePlane();
            void CreateFrame();
            void CreateShaders();
            void CreateTreeShader(); // New shader for trees
            GLuint Create1x1Texture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255); // Helper texture gen

            void TryLoadGeneratedMap(Core::Application* p_App);
            void LoadPolygonData(Core::Application* p_App);
            void LoadSampleMapData();
            void BuildRiverFromMapData();
            void BuildParksFromMapData();
            void BuildFallbackRiver();
            void RenderMapData(Core::Application* p_App);
            void LoadBridgeModel(Core::Application* p_App);
            void RenderText(const std::string& s_Text, float f_X, float f_Y, float f_Scale,
                float f_R, float f_G, float f_B, Core::Application* p_App);
            void InitializeShowcaseBuilding(Core::Application* p_App);
            void DestroyShowcaseBuilding(Core::Application* p_App);
            void DestroyGeneratedBuildings();
            void RenderShowcaseBuilding(const glm::mat4& mat4_View,
                const glm::mat4& mat4_Projection, const glm::vec3& vec3_CameraPos);
            void RenderGeneratedBuildings(const glm::mat4& mat4_View,
                const glm::mat4& mat4_Projection, const glm::vec3& vec3_CameraPos);
            bool UploadBuildingInstance(BuildingRenderInstance& instance);
            void DestroyBuildingInstance(BuildingRenderInstance& instance);
            void ConvertBuildingMeshForEnvironment(Core::BuildingMesh& mesh);
            void ScaleBuildingMesh(Core::BuildingMesh& mesh, float f_Scale);
            void BuildBuildingsFromMapData();
            void RenderBuildingInstance(const BuildingRenderInstance& instance,
                const glm::mat4& mat4_View, const glm::mat4& mat4_Projection,
                const glm::vec3& vec3_CameraPos) const;
            void ReleaseSurfaceBuffers(SurfaceBuffers& buffers);
            bool UploadSurfaceBuffersFromData(const std::vector<glm::vec3>& positions,
                const std::vector<glm::vec3>& normals,
                const std::vector<glm::vec2>& uvs,
                const std::vector<unsigned int>& indices,
                SurfaceBuffers& outBuffers);
            void UploadWindowSurfaces(const std::vector<Core::BuildingMesh::WindowWall>& walls,
                std::vector<SurfaceBuffers>& outBuffers);
            void UploadDoorSurfaces(const std::vector<Core::BuildingMesh::Door>& doors,
                std::vector<SurfaceBuffers>& outBuffers);
            void ReleaseInstanceBuffers(BuildingRenderInstance& instance);
            GLuint LoadShowcaseTexture(Core::Application* p_App, const std::string& s_RelativePath);
            void LoadBuildingTextures(Core::Application* p_App);
            void LoadTextureSetFromDirectory(Core::Application* p_App,
                const std::string& s_RelativeDir, std::vector<GLuint>& vec_Target);
            GLuint PickRandomTexture(const std::vector<GLuint>& vec_Textures);
            void AssignRandomTextures(BuildingRenderInstance& instance);
            void DestroyBuildingTextures(Core::Application* p_App);

            void AccelerateCameraForward(float f_DeltaTime);
            void AccelerateCameraBackward(float f_DeltaTime);
            void AccelerateCameraLeft(float f_DeltaTime);
            void AccelerateCameraRight(float f_DeltaTime);
            void UpdateCameraPhysics(float f_DeltaTime);
            void CreateTestRoad(Core::Application* p_App);

            void BuildHighwaysFromMapData(Core::Application* p_App);
            void GenerateRoadsFromMapData(Core::Application* p_App);
            void RenderRoads(const glm::mat4& mat4_Projection, const glm::mat4& mat4_View);

            // Tree methods
            void BuildTreesFromMapData();
            void RenderTrees(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection, const glm::vec3& vec3_CameraPos);
            void DestroyTrees();

            // Station rendering
            std::vector<float> generateCircleVertices(float f_Radius, int i_Segments);
            void RenderStations(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection);
            std::vector<float> generateCylinderVertices(float radius, float height, int segments);
            std::vector<float> generateHemisphereVertices(float radius, int segments);
            void InitializePlayerTokenGeometry();
            void DestroyPlayerTokenGeometry();
            void RenderPlayerTokens(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection);
            void InitializeDebugPlayerTokens();
            const StationCircle* FindStationCircle(int stationID) const;

            // Station interaction
            int m_i_SelectedStationID = -1;
            std::vector<int> m_vec_HighlightedStations; // Stations connected to selected
            Core::GraphManager m_graph;
            bool m_b_GraphLoaded = false;

            int FindStationAtScreenPos(int i_ScreenX, int i_ScreenY, const glm::mat4& mat4_View, const glm::mat4& mat4_Projection);
            void RenderStationInfo(Core::Application* p_App);
            void RenderConnectionLines(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection);
            void RenderHighlightedStations(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection);
            void LoadGraphData();
            void LoadGraphDataFromGeneratedMap();
        };

    } // namespace States
} // namespace ScotlandYard