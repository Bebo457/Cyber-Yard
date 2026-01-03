#pragma once

#include "IGameState.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>

namespace ScotlandYard { namespace Core { class Application; } }

namespace ScotlandYard {
namespace States {

class EmptyEnvironmentState : public Core::IGameState {
public:
    EmptyEnvironmentState();
    ~EmptyEnvironmentState() override;

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

    GLuint m_TexSidewalk = 0;
    GLuint m_TexGrass = 0;
    GLuint m_TexMask = 0;
    bool   m_b_UseMask = false;

    // Game state
    bool m_b_GameActive = true;
    
    // Camera system (mirrors GameState behavior)
    bool m_b_Camera3D = true;
    glm::vec3 m_vec3_CameraPosition{0.0f, 2.2f, 6.0f};
    glm::vec3 m_vec3_CameraVelocity{0.0f, 0.0f, 0.0f};
    glm::vec3 m_vec3_CameraFront{0.0f, -0.3f, -1.0f};
    glm::vec3 m_vec3_CameraUp{0.0f, 1.0f, 0.0f};
    float m_f_CameraAngle{-0.2915f};
    float m_f_CameraAngleVelocity{0.0f};
    glm::vec3 m_vec3_Saved3DCameraPosition{11.0f, 2.2f, 12.0f};

    // Camera constants (copied from GameState)
    static constexpr float k_CameraScrollAcceleration = 0.003f;
    static constexpr float k_CameraScrollFriction = 0.90f;
    static constexpr float k_CameraScrollToForwardRatio = 8.0f;
    static constexpr float k_CameraAcceleration = 12.0f;
    static constexpr float k_MaxCameraSpeed = 80.0f;
    static constexpr float k_CameraFriction = 0.90f;
    static constexpr float k_MinCameraAngle = -1.55f;
    static constexpr float k_MaxCameraAngle = -0.2915f;

    // Private methods
    void CreatePlane();
    void CreateShaders();
    void TryLoadGeneratedMap(Core::Application* p_App);
    void RenderText(const std::string& s_Text, float f_X, float f_Y, float f_Scale,
                    float f_R, float f_G, float f_B, Core::Application* p_App);
    
    void AccelerateCameraForward(float f_DeltaTime);
    void AccelerateCameraBackward(float f_DeltaTime);
    void AccelerateCameraLeft(float f_DeltaTime);
    void AccelerateCameraRight(float f_DeltaTime);
    void UpdateCameraPhysics(float f_DeltaTime);
};

} // namespace States
} // namespace ScotlandYard
