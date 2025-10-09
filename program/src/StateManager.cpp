#include "StateManager.h"
#include <stdexcept>

namespace ScotlandYard {
namespace Core {

StateManager::StateManager() {
}

StateManager::~StateManager() {
    while (!m_StateStack.empty()) {
        m_StateStack.top()->OnExit();
        m_StateStack.pop();
    }
}

void StateManager::RegisterState(const std::string& s_Name, std::unique_ptr<IGameState> u_State) {
    m_map_States[s_Name] = std::move(u_State);
}

void StateManager::PushState(const std::string& s_Name) {
    auto it = m_map_States.find(s_Name);
    if (it == m_map_States.end()) {
        throw std::runtime_error("State not found: " + s_Name);
    }

    if (!m_StateStack.empty()) {
        m_StateStack.top()->OnPause();
    }

    IGameState* p_State = it->second.get();
    m_StateStack.push(p_State);
    p_State->OnEnter();
}

void StateManager::PopState() {
    if (m_StateStack.empty()) {
        return;
    }

    m_StateStack.top()->OnExit();
    m_StateStack.pop();

    if (!m_StateStack.empty()) {
        m_StateStack.top()->OnResume();
    }
}

void StateManager::ChangeState(const std::string& s_Name) {
    auto it = m_map_States.find(s_Name);
    if (it == m_map_States.end()) {
        throw std::runtime_error("State not found: " + s_Name);
    }

    while (!m_StateStack.empty()) {
        m_StateStack.top()->OnExit();
        m_StateStack.pop();
    }

    IGameState* p_State = it->second.get();
    m_StateStack.push(p_State);
    p_State->OnEnter();
}

void StateManager::Update(float f_DeltaTime) {
    if (!m_StateStack.empty()) {
        m_StateStack.top()->Update(f_DeltaTime);
    }
}

void StateManager::Render(Application* p_App) {
    if (!m_StateStack.empty()) {
        m_StateStack.top()->Render(p_App);
    }
}

void StateManager::HandleEvent(const SDL_Event& event, Application* p_App) {
    if (!m_StateStack.empty()) {
        m_StateStack.top()->HandleEvent(event, p_App);
    }
}

} // namespace Core
} // namespace ScotlandYard
