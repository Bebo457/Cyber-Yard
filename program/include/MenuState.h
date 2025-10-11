#ifndef SCOTLANDYARD_STATES_MENUSTATE_H
#define SCOTLANDYARD_STATES_MENUSTATE_H

#include "IGameState.h"
#include <GL/glew.h>
#include <map>
#include <string>

namespace ScotlandYard {
namespace Core {
    class Application;
}

namespace States {

struct Character {
    GLuint m_TextureID;
    int m_i_Width;
    int m_i_Height;
    int m_i_BearingX;
    int m_i_BearingY;
    int m_i_Advance;
};

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
        float f_X, f_Y, f_Width, f_Height;
        std::string s_Text;
    };

    Button m_Buttons[3];

    GLuint m_VAO;
    GLuint m_VBO;
    GLuint m_ShaderProgram;

    std::map<char, Character> m_map_Characters;

    void InitializeOpenGL();
    void LoadFont();
    void RenderText(const std::string& s_Text, float f_X, float f_Y, float f_Scale, float f_R, float f_G, float f_B);
    void RenderButton(const Button& button, bool b_Selected, int i_WindowWidth, int i_WindowHeight);
};

} // namespace States
} // namespace ScotlandYard

#endif // SCOTLANDYARD_STATES_MENUSTATE_H
