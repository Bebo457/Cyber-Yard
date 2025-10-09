#include "GameState.h"
#include "Application.h"
#include <GL/glew.h>

namespace ScotlandYard {
namespace States {

GameState::GameState()
    : m_b_GameActive(false)
{
}

GameState::~GameState() {
}

void GameState::OnEnter() {
    m_b_GameActive = true;
}

void GameState::OnExit() {
    m_b_GameActive = false;
}

void GameState::OnPause() {
    m_b_GameActive = false;
}

void GameState::OnResume() {
    m_b_GameActive = true;
}

void GameState::Update(float f_DeltaTime) {
    if (!m_b_GameActive) return;
}

void GameState::Render(Core::Application* p_App) {
    // Use OpenGL for game rendering
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Game rendering code will go here
    
    // Swap OpenGL buffers for game
    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

void GameState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                break;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int i_X = event.button.x;
        int i_Y = event.button.y;
    }
}

} // namespace States
} // namespace ScotlandYard
