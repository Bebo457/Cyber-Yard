#ifndef SCOTLANDYARD_STATES_GAMESTATE_H
#define SCOTLANDYARD_STATES_GAMESTATE_H

#include "IGameState.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <SDL2/SDL.h>

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

    GLuint m_VAO_Plane;
    GLuint m_VBO_Plane;
    GLuint m_ShaderProgram_Plane;

    std::vector<glm::vec2> m_vec_CirclePositions;
    GLuint m_ShaderProgram_Circle;
    GLuint m_VAO_Circle;
    GLuint m_VBO_Circle;
    int m_i_CircleVertexCount;

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

    static constexpr float k_CameraAcceleration = 1000.0f;
    static constexpr float k_MaxCameraSpeed = 10000.0f;
    static constexpr float k_CameraFriction = 0.28f;

    std::vector<float> generateCircleVertices(float f_Radius, int i_Segments);
    void LoadTextures(Core::Application* p_App);

    void AccelerateCameraForward(float f_DeltaTime);
    void AccelerateCameraBackward(float f_DeltaTime);
    void AccelerateCameraLeft(float f_DeltaTime);
    void AccelerateCameraRight(float f_DeltaTime);
    void UpdateCameraPhysics(float f_DeltaTime);
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_GAMESTATE_H