#include "MenuState.h"
#include "Application.h"
#include "StateManager.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace ScotlandYard {
namespace States {

MenuState::MenuState()
    : m_i_SelectedOption(0)
{
    m_Buttons[0] = {0.0f, 0.0f, 200.0f, 60.0f, "Rozpocznij"};
    m_Buttons[1] = {0.0f, 0.0f, 200.0f, 60.0f, "Ustawienia"};
    m_Buttons[2] = {0.0f, 0.0f, 200.0f, 60.0f, "Wyjdz"};
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

void MenuState::Update(float f_DeltaTime) {
}

void MenuState::RenderText(const std::string& s_Text, float f_X, float f_Y, float f_Scale, float f_R, float f_G, float f_B, Core::Application* p_App) {
    const auto& characters = p_App->GetCharacterMap();
    GLuint shaderProgram = p_App->GetTextShaderProgram();
    GLuint vao = p_App->GetTextVAO();
    GLuint vbo = p_App->GetTextVBO();

    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), f_R, f_G, f_B);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);

    for (auto c : s_Text) {
        auto it = characters.find(c);
        if (it == characters.end()) continue;

        const Core::Character& ch = it->second;

        float f_Xpos = f_X + ch.m_i_BearingX * f_Scale;
        float f_Ypos = f_Y + (ch.m_i_Height - ch.m_i_BearingY) * f_Scale; 

        float f_W = ch.m_i_Width * f_Scale;
        float f_H = ch.m_i_Height * f_Scale;

        float vertices[6][4] = {
            { f_Xpos,     f_Ypos - f_H,   0.0f, 1.0f },
            { f_Xpos,     f_Ypos,         0.0f, 0.0f },
            { f_Xpos + f_W, f_Ypos,         1.0f, 0.0f },

            { f_Xpos,     f_Ypos - f_H,   0.0f, 1.0f },
            { f_Xpos + f_W, f_Ypos,         1.0f, 0.0f },
            { f_Xpos + f_W, f_Ypos - f_H,   1.0f, 1.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.m_TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        f_X += (ch.m_i_Advance >> 6) * f_Scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MenuState::RenderButton(const Button& button, bool b_Selected, int i_WindowWidth, int i_WindowHeight, Core::Application* p_App) {
    GLuint shaderProgram = p_App->GetTextShaderProgram();
    GLuint vao = p_App->GetTextVAO();
    GLuint vbo = p_App->GetTextVBO();
    const auto& characters = p_App->GetCharacterMap();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(glm::ortho(0.0f, (float)i_WindowWidth, 0.0f, (float)i_WindowHeight)));

    // draw filled rectangle
    glm::vec3 fillColor = b_Selected ? glm::vec3(0.78f, 0.78f, 0.78f) : glm::vec3(0.94f, 0.94f, 0.94f);
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), fillColor.r, fillColor.g, fillColor.b);

    float x = button.f_X;
    float y = button.f_Y;
    float w = button.f_Width;
    float h = button.f_Height;

    float vertices[6][4] = {
        { x,     y,     0.0f, 0.0f },
        { x,     y + h, 0.0f, 0.0f },
        { x + w, y + h, 0.0f, 0.0f },
        { x,     y,     0.0f, 0.0f },
        { x + w, y + h, 0.0f, 0.0f },
        { x + w, y,     0.0f, 0.0f }
    };

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // draw border
    float border = 3.0f;
    glm::vec3 borderColor(0.2f, 0.2f, 0.2f);
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), borderColor.r, borderColor.g, borderColor.b);

    float bx = x - border / 2.0f;
    float by = y - border / 2.0f;
    float bw = w + border;
    float bh = h + border;

    float borderVerts[24][4] = {
        // top
        { bx,      by + h, 0.0f, 0.0f },
        { bx,      by + h + border, 0.0f, 0.0f },
        { bx + bw, by + h + border, 0.0f, 0.0f },
        { bx,      by + h, 0.0f, 0.0f },
        { bx + bw, by + h + border, 0.0f, 0.0f },
        { bx + bw, by + h, 0.0f, 0.0f },

        // bottom
        { bx,      by - border, 0.0f, 0.0f },
        { bx,      by,          0.0f, 0.0f },
        { bx + bw, by,          0.0f, 0.0f },
        { bx,      by - border, 0.0f, 0.0f },
        { bx + bw, by,          0.0f, 0.0f },
        { bx + bw, by - border, 0.0f, 0.0f },

        // left
        { bx - border, by,          0.0f, 0.0f },
        { bx,          by,          0.0f, 0.0f },
        { bx,          by + h,      0.0f, 0.0f },
        { bx - border, by,          0.0f, 0.0f },
        { bx,          by + h,      0.0f, 0.0f },
        { bx - border, by + h,      0.0f, 0.0f },

        // right
        { bx + w,      by,          0.0f, 0.0f },
        { bx + w + border, by,      0.0f, 0.0f },
        { bx + w + border, by + h,  0.0f, 0.0f },
        { bx + w,      by,          0.0f, 0.0f },
        { bx + w + border, by + h,  0.0f, 0.0f },
        { bx + w,      by + h,      0.0f, 0.0f },
    };

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(borderVerts), borderVerts);
    glDrawArrays(GL_TRIANGLES, 0, 24);

    // draw text
    float f_TextScale = 1.0f;
    float f_TextWidth = 0.0f;
    for (auto c : button.s_Text) {
        auto it = characters.find(c);
        if (it != characters.end()) {
            f_TextWidth += (it->second.m_i_Advance >> 6) * f_TextScale;
        }
    }

    float f_TextX = x + (w - f_TextWidth) / 2.0f;
    float f_TextY = y + h / 2.0f - 8.0f;
    RenderText(button.s_Text, f_TextX, f_TextY, f_TextScale, 0.0f, 0.0f, 0.0f, p_App);
}

void MenuState::Render(Core::Application* p_App) {
    int i_WindowWidth = p_App->GetWidth();
    int i_WindowHeight = p_App->GetHeight();
    const auto& characters = p_App->GetCharacterMap();

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(i_WindowWidth), 0.0f, static_cast<float>(i_WindowHeight));

    GLuint shaderProgram = p_App->GetTextShaderProgram();
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    float f_TitleScale = 1.0f;
    std::string s_Title = "MENU";
    float f_TitleWidth = 0.0f;
    for (auto c : s_Title) {
        auto it = characters.find(c);
        if (it != characters.end()) {
            f_TitleWidth += (it->second.m_i_Advance >> 6) * f_TitleScale;
        }
    }
    float f_TitleX = (i_WindowWidth - f_TitleWidth) / 2.0f;
    float f_TitleY = i_WindowHeight - 50.0f - 24.0f;
    RenderText(s_Title, f_TitleX, f_TitleY, f_TitleScale, 0.0f, 0.0f, 0.0f, p_App);

    float f_ButtonSpacing = 80.0f;
    float f_TotalHeight = 3 * m_Buttons[0].f_Height + 2 * f_ButtonSpacing;
    float f_StartY = (i_WindowHeight - f_TotalHeight) / 2.0f;

    for (int i = 0; i < 3; i++) {
    m_Buttons[i].f_X = (i_WindowWidth - m_Buttons[i].f_Width) / 2.0f;
    m_Buttons[i].f_Y = f_StartY + (2 - i) * (m_Buttons[i].f_Height + f_ButtonSpacing);
    RenderButton(m_Buttons[i], i == m_i_SelectedOption, i_WindowWidth, i_WindowHeight, p_App);
    }

    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

void MenuState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
                m_i_SelectedOption = (m_i_SelectedOption - 1 + 3) % 3;
                break;
            case SDLK_DOWN:
                m_i_SelectedOption = (m_i_SelectedOption + 1) % 3;
                break;
            case SDLK_RETURN:
                switch (m_i_SelectedOption) {
                    case 0:
                        p_App->GetStateManager()->ChangeState("game");
                        break;
                    case 1:
                        break;
                    case 2:
                        p_App->RequestExit();
                        break;
                }
                break;
            case SDLK_ESCAPE:
                p_App->RequestExit();
                break;
        }
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN) {
    if (event.button.button == SDL_BUTTON_LEFT) {
        float f_MouseX = static_cast<float>(event.button.x);
        float f_MouseY = static_cast<float>(event.button.y);

        // convert SDL (origin top-left, Y down) to our UI coords (origin bottom-left, Y up)
        float f_WindowHeight = static_cast<float>(p_App->GetHeight());
        f_MouseY = f_WindowHeight - f_MouseY;

        for (int i = 0; i < 3; i++) {
            if (f_MouseX >= m_Buttons[i].f_X && f_MouseX <= m_Buttons[i].f_X + m_Buttons[i].f_Width &&
                f_MouseY >= m_Buttons[i].f_Y && f_MouseY <= m_Buttons[i].f_Y + m_Buttons[i].f_Height) {
                m_i_SelectedOption = i;
                switch (i) {
                    case 0:
                        p_App->GetStateManager()->ChangeState("game");
                        break;
                    case 1:
                        break;
                    case 2:
                        p_App->RequestExit();
                        break;
                }
                break;
            }
        }
    }
    }
    else if (event.type == SDL_MOUSEMOTION) {
        float f_MouseX = static_cast<float>(event.motion.x);
        float f_MouseY = static_cast<float>(event.motion.y);

        for (int i = 0; i < 3; i++) {
            if (f_MouseX >= m_Buttons[i].f_X && f_MouseX <= m_Buttons[i].f_X + m_Buttons[i].f_Width &&
                f_MouseY >= m_Buttons[i].f_Y && f_MouseY <= m_Buttons[i].f_Y + m_Buttons[i].f_Height) {
                m_i_SelectedOption = i;
                break;
            }
        }
    }
}

} // namespace States
} // namespace ScotlandYard
