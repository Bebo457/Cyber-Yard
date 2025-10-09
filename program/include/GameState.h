#ifndef SCOTLANDYARD_STATES_GAMESTATE_H
#define SCOTLANDYARD_STATES_GAMESTATE_H

#include "IGameState.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <SDL.h>

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

    // VAO i VBO dla planszy
    GLuint VAO_plane = 0;
    GLuint VBO_plane = 0;
    GLuint shaderProgram_plane = 0;

    // VAO i VBO dla kółek
    std::vector<glm::vec2> circlePositions; // pozycje kółek (x, z)
    GLuint circleShaderProgram = 0;
    GLuint circleVAO = 0;
    GLuint circleVBO = 0;
    int circleVertexCount = 0;

    SDL_Window* m_p_Window = nullptr;

    float rotation = 0.0f;

    int m_i_Width = 800;   // szerokość okna
    int m_i_Height = 600;  // wysokość okna

    GLuint textureID = 0;

    std::vector<float> generateCircleVertices(float radius, int segments);
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_GAMESTATE_H