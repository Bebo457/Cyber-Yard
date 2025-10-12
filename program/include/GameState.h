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
    bool m_bCamera3D = true; // start 3D

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

    std::vector<float> generateCircleVertices(float f_Radius, int i_Segments);
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_GAMESTATE_H