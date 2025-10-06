#include "GameState.h"
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

void GameState::Update(float deltaTime) {
    if (!m_b_GameActive) return;
}

void GameState::Render() {
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GameState::HandleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                break;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int x = event.button.x;
        int y = event.button.y;
    }
}

} // namespace States
} // namespace ScotlandYard
