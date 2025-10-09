#ifndef SCOTLANDYARD_CORE_STATEMANAGER_H
#define SCOTLANDYARD_CORE_STATEMANAGER_H

#include "IGameState.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <stack>

namespace ScotlandYard {
namespace Core {

class Application;

class StateManager {
public:
    StateManager();
    ~StateManager();

    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    void RegisterState(const std::string& s_Name, std::unique_ptr<IGameState> u_State);
    void PushState(const std::string& s_Name);
    void PopState();
    void ChangeState(const std::string& s_Name);
    void Update(float f_DeltaTime);
    void Render(Application* p_App);
    void HandleEvent(const SDL_Event& event, Application* p_App);

    bool IsEmpty() const { return m_StateStack.empty(); }

private:
    std::unordered_map<std::string, std::unique_ptr<IGameState>> m_map_States;
    std::stack<IGameState*> m_StateStack;
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_STATEMANAGER_H
