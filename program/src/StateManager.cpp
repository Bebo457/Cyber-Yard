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

void StateManager::RegisterState(const std::string& name, std::unique_ptr<IGameState> pState) {
    m_map_States[name] = std::move(pState);
}

void StateManager::PushState(const std::string& name) {
    auto it = m_map_States.find(name);
    if (it == m_map_States.end()) {
        throw std::runtime_error("State not found: " + name);
    }

    if (!m_StateStack.empty()) {
        m_StateStack.top()->OnPause();
    }

    IGameState* pState = it->second.get();
    m_StateStack.push(pState);
    pState->OnEnter();
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

void StateManager::ChangeState(const std::string& name) {
    auto it = m_map_States.find(name);
    if (it == m_map_States.end()) {
        throw std::runtime_error("State not found: " + name);
    }

    while (!m_StateStack.empty()) {
        m_StateStack.top()->OnExit();
        m_StateStack.pop();
    }

    IGameState* pState = it->second.get();
    m_StateStack.push(pState);
    pState->OnEnter();
}

void StateManager::Update(float deltaTime) {
    if (!m_StateStack.empty()) {
        m_StateStack.top()->Update(deltaTime);
    }
}

void StateManager::Render() {
    if (!m_StateStack.empty()) {
        m_StateStack.top()->Render();
    }
}

void StateManager::HandleEvent(const SDL_Event& event) {
    if (!m_StateStack.empty()) {
        m_StateStack.top()->HandleEvent(event);
    }
}

} // namespace Core
} // namespace ScotlandYard
