#ifndef SCOTLANDYARD_STATES_GAMESTATE_H
#define SCOTLANDYARD_STATES_GAMESTATE_H

#include "IGameState.h"

namespace ScotlandYard {
namespace States {

class GameState : public Core::IGameState {
public:
    GameState();
    ~GameState() override;

    void OnEnter() override;
    void OnExit() override;
    void OnPause() override;
    void OnResume() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleEvent(const SDL_Event& event) override;

private:
    bool m_b_GameActive;
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_GAMESTATE_H
