#ifndef SCOTLANDYARD_STATES_GAMESTATE_H
#define SCOTLANDYARD_STATES_GAMESTATE_H

#include "IGameState.h"
#include "GameConstants.h"
#include "MapDataLoader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <map>
#include <SDL2/SDL.h>
#include "Player.h"
#include "../../Graphs/graph_manage.h"
#include <thread>
#include <mutex>
#include <atomic>

namespace ScotlandYard {
namespace Core {
    class Application;
}

namespace States {

class GameState : public Core::IGameState {
public:
    GameState();
    ~GameState() override;

    void OnEnter() override;
    void OnExit() override;
    void OnPause() override;
    void OnResume() override;
    void Update(float f_DeltaTime) override;
    void Render(Core::Application* p_App) override;
    void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;

private:
    bool m_b_GameActive;
    bool m_b_Camera3D;
    bool m_b_TexturesLoaded;

    // Game round state - protected by m_mtx_GameState for thread safety
    std::atomic<int> m_i_Round{1};
    std::vector<bool> m_vec_MovedThisRound;
    std::atomic<int> m_i_PlayersRemainingThisRound{0};
    std::mutex m_mtx_GameState;

    GLuint m_VAO_Plane;
    GLuint m_VBO_Plane;
    GLuint m_ShaderProgram_Plane;

    GLuint m_ShaderProgram_Circle;
    GLuint m_VAO_Circle;
    GLuint m_VBO_Circle;
    int m_i_CircleVertexCount;
    std::vector<float> generateCircleVertices(float f_Radius, int i_Segments);
    struct StationCircle {
        glm::vec2 position;
        std::vector<std::string> transportTypes; // np. {"taxi", "bus"}
        int stationID;
    };
    std::vector<StationCircle> m_vec_CircleStations;

    SDL_Window* m_p_Window;

    float m_f_Rotation;

    int m_i_Width;
    int m_i_Height;

    GLuint m_TextureID;

    // Camera system
    glm::vec3 m_vec3_CameraPosition;
    glm::vec3 m_vec3_CameraVelocity;
    glm::vec3 m_vec3_CameraFront;
    glm::vec3 m_vec3_CameraUp;
    float m_f_CameraAngle;
    float m_f_CameraAngleVelocity;

    glm::vec3 m_vec3_Saved3DCameraPosition;

    static constexpr float k_CameraYawSensitivity = 0.02f;
    static constexpr float k_CameraScrollAcceleration = 0.003f;
    static constexpr float k_CameraScrollFriction = 0.97f;
    static constexpr float k_CameraScrollToForwardRatio = 12.0f;
    static constexpr float k_CameraAcceleration = 12.0f;
    static constexpr float k_MaxCameraSpeed = 150.0f;
    static constexpr float k_CameraFriction = 0.97f;
    static constexpr float k_MinCameraAngle = -1.55f;  // -90 degrees
    static constexpr float k_MaxCameraAngle = -0.2915f;  // ~-16.7 degrees

   

    void LoadTextures(Core::Application* p_App);

    // Graph manager used by the game state to query connections
    GraphManager m_graph;

    std::thread m_t_ConsoleThread;
    std::atomic_bool m_b_ConsoleThreadRunning{false};
    std::mutex m_mtx_Players;  // Protects m_vec_Players

    std::vector<Core::Player> m_vec_Players;
    std::atomic_bool m_b_RequestMenuChange{false};
    struct PlayerToken {
        glm::vec3 color;
        float radius;
    };
    std::vector<PlayerToken> m_vec_PlayerTokens;
    std::vector<float> generateCylinderVertices(float radius, float height, int segments);    
    std::vector<float> generateHemisphereVertices(float radius, int segments);

    // Player token 
    GLuint m_VAO_Cylinder = 0;
    GLuint m_VBO_Cylinder = 0;
    int m_i_CylinderVertexCount = 0;
    GLuint m_VAO_Hemisphere = 0;
    GLuint m_VBO_Hemisphere = 0;
    int m_i_HemisphereVertexCount = 0;

    void AccelerateCameraForward(float f_DeltaTime);
    void AccelerateCameraBackward(float f_DeltaTime);
    void AccelerateCameraLeft(float f_DeltaTime);
    void AccelerateCameraRight(float f_DeltaTime);
    void UpdateCameraPhysics(float f_DeltaTime);
    enum class Winner {
        None = 0,
        Detectives,
        MisterX
    };

    std::atomic_bool m_b_ShowEndGameModal{false};
    Winner m_EndGameWinner{Winner::None};
    int m_i_EndModalBtnX0 = 0;
    int m_i_EndModalBtnY0 = 0;
    int m_i_EndModalBtnX1 = 0;
    int m_i_EndModalBtnY1 = 0;
    std::atomic_bool m_b_EndModalBtnHover{false};

    std::atomic_bool m_b_ShowPausedModal{false};

    std::atomic_bool m_b_DebuggingMode{false};
    std::atomic_bool m_b_ShowPickingBuffer{false};
    std::atomic_bool m_b_ShowMrXInDebug{true};

    enum class ClickableType : uint8_t {
        None = 0,
        Detective = 1,
        MisterX = 2,
        Arrow = 3
    };

    struct ClickableID {
        ClickableType e_Type;
        int i_Index;
        int i_Data;
    };

    struct DirectionArrow {
        glm::vec2 vec2_Position;
        float f_Rotation;
        int i_DestinationNode;
        int i_TransportType;
    };

    GLuint m_FBO_Picking;
    GLuint m_TextureID_Picking;
    GLuint m_RBO_PickingDepth;
    GLuint m_FBO_PickingDilated;
    GLuint m_TextureID_PickingDilated;
    GLuint m_VAO_Arrow;
    GLuint m_VBO_Arrow;
    int m_i_ArrowVertexCount;
    GLuint m_ShaderProgram_Picking;
    GLuint m_ShaderProgram_Dilation;
    GLuint m_VAO_FullscreenQuad;
    GLuint m_VBO_FullscreenQuad;

    int m_i_SelectedPlayerIndex;
    std::vector<DirectionArrow> m_vec_CurrentArrows;
    uint32_t m_ui_NextPickingID;
    std::map<uint32_t, ClickableID> m_map_PickingIDToClickable;

    glm::vec3 IDToColor(uint32_t ui_ID) const;
    uint32_t ColorToID(unsigned char r, unsigned char g, unsigned char b) const;
    uint32_t RegisterClickable(ClickableType e_Type, int i_Index, int i_Data);
    std::vector<float> generateArrowVertices();
    void UpdateArrowsForSelectedPlayer();
    void RenderPickingPass(const glm::mat4& mat4_Projection, const glm::mat4& mat4_View);
    void ApplyDilationPass();
    void HandleColorPicking(int i_MouseX, int i_MouseY);
    void HandlePlayerClick(int i_PlayerIndex);
    void HandleArrowClick(int i_PlayerIndex, int i_DestinationNode);

    // Helper method to render Mr X token at a specific position
    void RenderMrXToken(const glm::vec2& vec2_Position, const glm::mat4& mat4_Projection, const glm::mat4& mat4_View, GLint i_MvpLoc, GLint i_ColorLoc);

    void CheckEndOfGame(Winner winner = Winner::None);

    bool CheckCapture() const;
    void ResetToInitial();
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_GAMESTATE_H