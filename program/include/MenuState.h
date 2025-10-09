#ifndef SCOTLANDYARD_STATES_MENUSTATE_H
#define SCOTLANDYARD_STATES_MENUSTATE_H

#include "IGameState.h"
#include <SDL2/SDL.h>

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
    
    struct Button {
        int i_X, i_Y, i_Width, i_Height;
        const char* p_Text;
        SDL_Texture* p_Texture;
        int i_TextWidth, i_TextHeight;
    };
    
    Button m_Buttons[3];
    SDL_Texture* m_p_TitleTexture;
    int m_i_TitleWidth, m_i_TitleHeight;
    bool m_b_TexturesCreated;
    
    void InitializeButtons(Core::Application* p_App);
    void CreateTextures(Core::Application* p_App);
    void DestroyTextures();
    void RenderButton(Core::Application* p_App, const Button& button, bool b_Selected);
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_MENUSTATE_H
