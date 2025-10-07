#ifndef SCOTLANDYARD_STATES_MENUSTATE_H
#define SCOTLANDYARD_STATES_MENUSTATE_H

#include "IGameState.h"

namespace ScotlandYard {
namespace States {

class MenuState : public Core::IGameState {
public:
    MenuState();
    ~MenuState() override;

    void OnEnter() override;
    void OnExit() override;
    void OnPause() override;
    void OnResume() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleEvent(const SDL_Event& event) override;

private:
    int m_i_SelectedOption;
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_MENUSTATE_H
