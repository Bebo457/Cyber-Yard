#include "MenuState.h"
#include "Application.h"
#include "StateManager.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>

namespace ScotlandYard {
namespace States {

MenuState::MenuState()
    : m_i_SelectedOption(0)
    , m_VAO(0)
    , m_VBO(0)
    , m_ShaderProgram(0)
{
    m_Buttons[0] = {0.0f, 0.0f, 200.0f, 60.0f, "Rozpocznij"};
    m_Buttons[1] = {0.0f, 0.0f, 200.0f, 60.0f, "Ustawienia"};
    m_Buttons[2] = {0.0f, 0.0f, 200.0f, 60.0f, "Wyjdz"};
}

MenuState::~MenuState() {
}

void MenuState::OnEnter() {
    m_i_SelectedOption = 0;
    InitializeOpenGL();
    LoadFont();
}

void MenuState::OnExit() {
    for (auto& pair : m_map_Characters) {
        glDeleteTextures(1, &pair.second.m_TextureID);
    }
    m_map_Characters.clear();

    if (m_VAO) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    if (m_VBO) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_ShaderProgram) {
        glDeleteProgram(m_ShaderProgram);
        m_ShaderProgram = 0;
    }
}

void MenuState::OnPause() {
}

void MenuState::OnResume() {
}

void MenuState::Update(float f_DeltaTime) {
}

void MenuState::InitializeOpenGL() {
    const char* p_VertexShaderSrc = R"(
        #version 330 core
        layout (location = 0) in vec4 vertex;
        out vec2 TexCoords;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    const char* p_FragmentShaderSrc = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 color;
        uniform sampler2D text;
        uniform vec3 textColor;
        void main() {
            vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
            color = vec4(textColor, 1.0) * sampled;
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &p_VertexShaderSrc, nullptr);
    glCompileShader(vertexShader);

    GLint i_Success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &p_FragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vertexShader);
    glAttachShader(m_ShaderProgram, fragmentShader);
    glLinkProgram(m_ShaderProgram);

    glGetProgramiv(m_ShaderProgram, GL_LINK_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ShaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void MenuState::LoadFont() {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Could not init FreeType Library" << std::endl;
        return;
    }

    FT_Face face;
#ifdef _WIN32
    if (FT_New_Face(ft, "C:\\Windows\\Fonts\\arial.ttf", 0, &face)) {
        std::cerr << "Failed to load font" << std::endl;
        FT_Done_FreeType(ft);
        return;
    }
#elif __linux__
    if (FT_New_Face(ft, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &face)) {
        std::cerr << "Failed to load font" << std::endl;
        FT_Done_FreeType(ft);
        return;
    }
#elif __APPLE__
    if (FT_New_Face(ft, "/System/Library/Fonts/Arial.ttf", 0, &face)) {
        std::cerr << "Failed to load font" << std::endl;
        FT_Done_FreeType(ft);
        return;
    }
#endif

    FT_Set_Pixel_Sizes(face, 0, 24);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "Failed to load Glyph" << std::endl;
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            static_cast<int>(face->glyph->bitmap.width),
            static_cast<int>(face->glyph->bitmap.rows),
            face->glyph->bitmap_left,
            face->glyph->bitmap_top,
            static_cast<int>(face->glyph->advance.x)
        };
        m_map_Characters.insert(std::pair<char, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

void MenuState::RenderText(const std::string& s_Text, float f_X, float f_Y, float f_Scale, float f_R, float f_G, float f_B) {
    glUseProgram(m_ShaderProgram);
    glUniform3f(glGetUniformLocation(m_ShaderProgram, "textColor"), f_R, f_G, f_B);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_VAO);

    for (auto c : s_Text) {
        Character ch = m_map_Characters[c];

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
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        f_X += (ch.m_i_Advance >> 6) * f_Scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MenuState::RenderButton(const Button& button, bool b_Selected, int i_WindowWidth, int i_WindowHeight) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(m_ShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_ShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(glm::ortho(0.0f, (float)i_WindowWidth, 0.0f, (float)i_WindowHeight)));

    // draw filled rectangle
    glm::vec3 fillColor = b_Selected ? glm::vec3(0.78f, 0.78f, 0.78f) : glm::vec3(0.94f, 0.94f, 0.94f);
    glUniform3f(glGetUniformLocation(m_ShaderProgram, "textColor"), fillColor.r, fillColor.g, fillColor.b);

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

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // draw border
    float border = 3.0f;
    glm::vec3 borderColor(0.2f, 0.2f, 0.2f);
    glUniform3f(glGetUniformLocation(m_ShaderProgram, "textColor"), borderColor.r, borderColor.g, borderColor.b);

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
        f_TextWidth += (m_map_Characters[c].m_i_Advance >> 6) * f_TextScale;
    }

    float f_TextX = x + (w - f_TextWidth) / 2.0f;
    float f_TextY = y + h / 2.0f - 8.0f;
    RenderText(button.s_Text, f_TextX, f_TextY, f_TextScale, 0.0f, 0.0f, 0.0f);
}

void MenuState::Render(Core::Application* p_App) {
    int i_WindowWidth = p_App->GetWidth();
    int i_WindowHeight = p_App->GetHeight();

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(i_WindowWidth), 0.0f, static_cast<float>(i_WindowHeight));

    glUseProgram(m_ShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_ShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    float f_TitleScale = 1.0f;
    std::string s_Title = "MENU";
    float f_TitleWidth = 0.0f;
    for (auto c : s_Title) {
        f_TitleWidth += (m_map_Characters[c].m_i_Advance >> 6) * f_TitleScale;
    }
    float f_TitleX = (i_WindowWidth - f_TitleWidth) / 2.0f;
    float f_TitleY = i_WindowHeight - 50.0f - 24.0f;
    RenderText(s_Title, f_TitleX, f_TitleY, f_TitleScale, 0.0f, 0.0f, 0.0f);

    float f_ButtonSpacing = 80.0f;
    float f_TotalHeight = 3 * m_Buttons[0].f_Height + 2 * f_ButtonSpacing;
    float f_StartY = (i_WindowHeight - f_TotalHeight) / 2.0f;

    for (int i = 0; i < 3; i++) {
    m_Buttons[i].f_X = (i_WindowWidth - m_Buttons[i].f_Width) / 2.0f;
    m_Buttons[i].f_Y = f_StartY + (2 - i) * (m_Buttons[i].f_Height + f_ButtonSpacing);
    RenderButton(m_Buttons[i], i == m_i_SelectedOption, i_WindowWidth, i_WindowHeight);
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
