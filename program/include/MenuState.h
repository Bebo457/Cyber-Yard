#ifndef SCOTLANDYARD_STATES_MENUSTATE_H
#define SCOTLANDYARD_STATES_MENUSTATE_H

#include "IGameState.h"
#include <GL/glew.h>
#include <string>

namespace ScotlandYard {
namespace Core {
    class Application;
}

namespace States {

class MenuState : public Core::IGameState {
public:
    MenuState();
    ~MenuState() override;

    void OnEnter() override;
    void OnExit() override;
    void OnPause() override;
    void OnResume() override;
    void Update(float f_DeltaTime) override;
    void Render(Core::Application* p_App) override;
    void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;

private:
    int m_i_SelectedOption;
    int m_i_HoverOption;
    static constexpr int BUTTON_COUNT = 4;
    float m_f_FrameAlpha[BUTTON_COUNT];

    struct Button {
        float f_X, f_Y, f_Width, f_Height;
        std::string s_Text;
    };

    Button m_Buttons[BUTTON_COUNT];

    void RenderText(const std::string& s_Text, float f_X, float f_Y, float f_Scale, float f_R, float f_G, float f_B, Core::Application* p_App);
    void RenderTextBold(const std::string& s_Text, float f_X, float f_Y, float f_Scale, float f_R, float f_G, float f_B, Core::Application* p_App);
    void RenderButton(const Button& button, int i_Index, bool b_Selected, int i_WindowWidth, int i_WindowHeight, Core::Application* p_App);
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_MENUSTATE_H
