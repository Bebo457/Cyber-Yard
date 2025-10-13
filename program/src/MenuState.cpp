#include "MenuState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

namespace ScotlandYard {
namespace States {

MenuState::MenuState()
    : m_i_SelectedOption(0)
    , m_i_HoverOption(-1)
    , m_WhiteTexture(0)
{
    for (int i = 0; i < BUTTON_COUNT; ++i) m_f_FrameAlpha[i] = 0.0f;
    m_Buttons[0] = {0.0f, 0.0f, 320.0f, 70.0f, "New Game"};
    m_Buttons[1] = {0.0f, 0.0f, 320.0f, 70.0f, "Load Game"};
    m_Buttons[2] = {0.0f, 0.0f, 320.0f, 70.0f, "Settings"};
    m_Buttons[3] = {0.0f, 0.0f, 320.0f, 70.0f, "Exit"};
}

MenuState::~MenuState() {
}

void MenuState::OnEnter() {
    m_i_SelectedOption = 0;

    // texture for rendering solid colors
    if (!m_WhiteTexture) {
        unsigned char white = 255;
        glGenTextures(1, &m_WhiteTexture);
        glBindTexture(GL_TEXTURE_2D, m_WhiteTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void MenuState::OnExit() {
    if (m_WhiteTexture) {
        glDeleteTextures(1, &m_WhiteTexture);
        m_WhiteTexture = 0;
    }
}

void MenuState::OnPause() {
}

void MenuState::OnResume() {
}

void MenuState::Update(float f_DeltaTime) {
    const float f_Speed = 8.0f; 
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        float f_Target = (i == m_i_SelectedOption) ? 0.18f : (i == m_i_HoverOption ? 0.09f : 0.0f);
        float f_Cur = m_f_FrameAlpha[i];
        float f_T = std::clamp(f_Speed * f_DeltaTime, 0.0f, 1.0f);
        m_f_FrameAlpha[i] = f_Cur + (f_Target - f_Cur) * f_T;
    }
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

void MenuState::RenderTextBold(const std::string& s_Text, float f_X, float f_Y, float f_Scale, float f_R, float f_G, float f_B, Core::Application* p_App) {
    float f_px = std::max(1.0f, f_Scale * 1.0f);
    const float f_offsets[6][2] = {
        {0.0f, 0.0f}, {f_px, 0.0f}, {0.0f, f_px}, {f_px, f_px}, {-f_px, 0.0f}, {0.0f, -f_px}
    };
    for (int i = 0; i < 6; ++i) {
        RenderText(s_Text, f_X + f_offsets[i][0], f_Y + f_offsets[i][1], f_Scale, f_R, f_G, f_B, p_App);
    }
}

void MenuState::RenderButton(const Button& button, int i_Index, bool b_Selected, int i_WindowWidth, int i_WindowHeight, Core::Application* p_App) {
    GLuint shaderProgram = p_App->GetTextShaderProgram();
    GLuint vao = p_App->GetTextVAO();
    GLuint vbo = p_App->GetTextVBO();
    const auto& characters = p_App->GetCharacterMap();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(glm::ortho(0.0f, (float)i_WindowWidth, 0.0f, (float)i_WindowHeight)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_WhiteTexture);
    glUniform1i(glGetUniformLocation(shaderProgram, "text"), 0);

    float x = button.m_f_X;
    float y = button.m_f_Y;
    float w = button.m_f_Width;
    float h = button.m_f_Height;
    float shadowOffset = 6.0f;
    glm::vec4 shadowColor(0.12f, 0.14f, 0.16f, 1.0f);
    float sx = x;
    float sy = y - shadowOffset;

    ScotlandYard::UI::DrawRoundedRectScreen(sx, sy, sx + w, sy + h, { shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a }, 8, p_App);

    ScotlandYard::UI::Color baseColor = { 1.0f, 0.84f, 0.0f, 1.0f };

    ScotlandYard::UI::Color buttonColor = baseColor;
    if (b_Selected) {
        buttonColor.r = std::min(1.0f, baseColor.r * 1.06f);
        buttonColor.g = std::min(1.0f, baseColor.g * 1.03f);
        buttonColor.b = std::min(1.0f, baseColor.b * 1.03f);
    } else {
        buttonColor.r = baseColor.r * 0.96f;
        buttonColor.g = baseColor.g * 0.96f;
        buttonColor.b = baseColor.b * 0.96f;
    }

    float bx0 = x;
    float by0 = y;
    float bx1 = x + w;
    float by1 = y + h;

    float f_FrameAlphaLocal = m_f_FrameAlpha[i_Index];
    if (f_FrameAlphaLocal > 1e-4f) {
        ScotlandYard::UI::Color frame = { 1.0f, 1.0f, 1.0f, f_FrameAlphaLocal };
        float f_Pad = 4.0f + f_FrameAlphaLocal * 6.0f;
        ScotlandYard::UI::DrawRoundedRectScreen(bx0 - f_Pad, by0 - f_Pad, bx1 + f_Pad, by1 + f_Pad, frame, 16, p_App);
    }

    ScotlandYard::UI::DrawRoundedRectScreen(bx0, by0, bx1, by1, buttonColor, 12, p_App);

    float borderPx = 3.0f;
    ScotlandYard::UI::Color borderColor = { 0.08f, 0.08f, 0.08f, 1.0f };
    ScotlandYard::UI::DrawRoundedRectScreen(x - borderPx/2.0f, y - borderPx/2.0f, x + w + borderPx/2.0f, y + h + borderPx/2.0f, borderColor, 12, p_App);

    float f_TextScale = 1.0f;
    float f_TextWidth = 0.0f;
    float maxTopOffset = -1e6f;  
    float minBottomOffset = 1e6f;
    for (auto c : button.m_s_Text) {
        auto it = characters.find(c);
        if (it != characters.end()) {
            const auto& ch = it->second;
            f_TextWidth += (ch.m_i_Advance >> 6) * f_TextScale;
            float topOff = (ch.m_i_Height - ch.m_i_BearingY) * f_TextScale;
            float botOff = -ch.m_i_BearingY * f_TextScale;
            if (topOff > maxTopOffset) maxTopOffset = topOff;
            if (botOff < minBottomOffset) minBottomOffset = botOff;
        }
    }

    float f_TextX = bx0 + ( (bx1 - bx0) - f_TextWidth ) / 2.0f;
    float f_TextY = by0 + ( (by1 - by0) ) / 2.0f - 8.0f; 
    if (maxTopOffset > -1e5f && minBottomOffset < 1e5f) {
        float textCenterOffset = (maxTopOffset + minBottomOffset) * 0.5f;
        f_TextY = by0 + (by1 - by0) * 0.5f - textCenterOffset;
    }
    RenderText(button.m_s_Text, f_TextX, f_TextY, f_TextScale, 1.0f, 1.0f, 1.0f, p_App);
}

void MenuState::Render(Core::Application* p_App) {
    int i_WindowWidth = p_App->GetWidth();
    int i_WindowHeight = p_App->GetHeight();
    const auto& characters = p_App->GetCharacterMap();

    glClearColor(0.16f, 0.18f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(i_WindowWidth), 0.0f, static_cast<float>(i_WindowHeight));

    GLuint shaderProgram = p_App->GetTextShaderProgram();
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    float f_TitleScale = 2.0f; 
    const float f_SpecialScaleMul = 1.35f; // multiplier for the larger letters
    std::string s_Title = "CYBER YARD";
    float f_TitleWidth = 0.0f;
    std::vector<float> charScales;
    charScales.reserve(s_Title.size());

    int i_LastIndex = -1;
    for (int i = (int)s_Title.size() - 1; i >= 0; --i) {
        if (s_Title[i] != ' ') { i_LastIndex = i; break; }
    }
    for (size_t idx = 0; idx < s_Title.size(); ++idx) {
        char c = s_Title[idx];
        bool b_IsEdge = (idx == 0) || (static_cast<int>(idx) == i_LastIndex);
        float f_ThisScale = b_IsEdge ? (f_TitleScale * f_SpecialScaleMul) : f_TitleScale;
        charScales.push_back(f_ThisScale);
        auto it = characters.find(c);
        if (it != characters.end()) {
            f_TitleWidth += (it->second.m_i_Advance >> 6) * f_ThisScale;
        }
    }
    float f_TitleX = (i_WindowWidth - f_TitleWidth) / 2.0f;
    float f_TitleY = i_WindowHeight - 140.0f; 
    float f_CursorX = f_TitleX;
    for (size_t i = 0; i < s_Title.size(); ++i) {
        char c = s_Title[i];
        float f_ThisScale = charScales[i];
        std::string s(1, c);
        RenderTextBold(s, f_CursorX, f_TitleY, f_ThisScale, 1.0f, 0.84f, 0.0f, p_App);
        auto it = characters.find(c);
        if (it != characters.end()) {
            f_CursorX += (it->second.m_i_Advance >> 6) * f_ThisScale;
        } else {
            f_CursorX += 8.0f * f_ThisScale;
        }
    }

    float f_ButtonSpacing = 28.0f;
    float f_TotalHeight = MenuState::BUTTON_COUNT * m_Buttons[0].m_f_Height + (MenuState::BUTTON_COUNT - 1) * f_ButtonSpacing;
    float f_StartY = (i_WindowHeight - f_TotalHeight) / 2.0f - 30.0f;

    for (int i = 0; i < MenuState::BUTTON_COUNT; i++) {
        m_Buttons[i].m_f_X = (i_WindowWidth - m_Buttons[i].m_f_Width) / 2.0f;
        m_Buttons[i].m_f_Y = f_StartY + (MenuState::BUTTON_COUNT - 1 - i) * (m_Buttons[i].m_f_Height + f_ButtonSpacing);
    RenderButton(m_Buttons[i], i, i == m_i_SelectedOption, i_WindowWidth, i_WindowHeight, p_App);
    }

    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

void MenuState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
                m_i_SelectedOption = (m_i_SelectedOption - 1 + MenuState::BUTTON_COUNT) % MenuState::BUTTON_COUNT;
                break;
            case SDLK_DOWN:
                m_i_SelectedOption = (m_i_SelectedOption + 1) % MenuState::BUTTON_COUNT;
                break;
            case SDLK_RETURN:
                switch (m_i_SelectedOption) {
                    case 0: // New Game
                        p_App->GetStateManager()->ChangeState("game");
                        break;
                    case 1: // Load Game 
                        break;
                    case 2: // Settings
                        break;
                    case 3: // Exit
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

        float f_WindowHeight = static_cast<float>(p_App->GetHeight());
        f_MouseY = f_WindowHeight - f_MouseY;

        for (int i = 0; i < MenuState::BUTTON_COUNT; i++) {
            if (f_MouseX >= m_Buttons[i].m_f_X && f_MouseX <= m_Buttons[i].m_f_X + m_Buttons[i].m_f_Width &&
                f_MouseY >= m_Buttons[i].m_f_Y && f_MouseY <= m_Buttons[i].m_f_Y + m_Buttons[i].m_f_Height) {
                m_i_HoverOption = i;
                m_i_SelectedOption = i;
                switch (i) {
                    case 0: // New Game
                        p_App->GetStateManager()->ChangeState("game");
                        break;
                    case 1: // Load Game 
                        break;
                    case 2: // Settings
                        break;
                    case 3: // Exit
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

        float f_WindowHeight = static_cast<float>(p_App->GetHeight());
        f_MouseY = f_WindowHeight - f_MouseY;

        int i_HoverIndex = -1;
        for (int i = 0; i < MenuState::BUTTON_COUNT; i++) {
            if (f_MouseX >= m_Buttons[i].m_f_X && f_MouseX <= m_Buttons[i].m_f_X + m_Buttons[i].m_f_Width &&
                f_MouseY >= m_Buttons[i].m_f_Y && f_MouseY <= m_Buttons[i].m_f_Y + m_Buttons[i].m_f_Height) {
                i_HoverIndex = i;
                break;
            }
        }
        m_i_HoverOption = i_HoverIndex;
    }
}

} // namespace States
} // namespace ScotlandYard
