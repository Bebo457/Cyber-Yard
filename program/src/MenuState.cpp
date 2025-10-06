#include "MenuState.h"
#include <GL/glew.h>

namespace ScotlandYard {
namespace States {

MenuState::MenuState()
    : m_i_SelectedOption(0)
{
}

MenuState::~MenuState() {
}

void MenuState::OnEnter() {
    m_i_SelectedOption = 0;
}

void MenuState::OnExit() {
}

void MenuState::OnPause() {
}

void MenuState::OnResume() {
}

void MenuState::Update(float deltaTime) {
}

void MenuState::Render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void MenuState::HandleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
                m_i_SelectedOption = (m_i_SelectedOption - 1 + 4) % 4;
                break;
            case SDLK_DOWN:
                m_i_SelectedOption = (m_i_SelectedOption + 1) % 4;
                break;
            case SDLK_RETURN:
                break;
        }
    }
}

} // namespace States
} // namespace ScotlandYard
