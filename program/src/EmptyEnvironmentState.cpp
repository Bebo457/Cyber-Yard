#include "EmptyEnvironmentState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

namespace ScotlandYard {
namespace States {

EmptyEnvironmentState::EmptyEnvironmentState() {}
EmptyEnvironmentState::~EmptyEnvironmentState() = default;

void EmptyEnvironmentState::CreateShaders() {
    const char* vsSrc = R"(#version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNormal;
        layout(location=2) in vec2 aUV;
        uniform mat4 uMVP;
        out vec2 vUV;
        void main(){ vUV=aUV; gl_Position = uMVP * vec4(aPos,1.0); }
    )";
    const char* fsSrc = R"(#version 330 core
        in vec2 vUV; uniform sampler2D uTex; uniform vec4 uColor;
        out vec4 FragColor;
        void main(){ vec4 t = texture(uTex, vUV); FragColor = mix(uColor, vec4(1.0,1.0,1.0,1.0), t.a) * t; }
    )";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);

    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vs);
    glAttachShader(m_ShaderProgram, fs);
    glLinkProgram(m_ShaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void EmptyEnvironmentState::CreatePlane() {
    float f_PlaneVertices[] = {
        // pos                 // normal       // uv
        -1.0f, 0.0f, 17.0f,     0.0f,1.0f,0.0f,  0.0f,1.0f,
         23.0f,0.0f, 17.0f,     0.0f,1.0f,0.0f,  1.0f,1.0f,
         23.0f,0.0f,-1.0f,      0.0f,1.0f,0.0f,  1.0f,0.0f,
        -1.0f, 0.0f, 17.0f,     0.0f,1.0f,0.0f,  0.0f,1.0f,
         23.0f,0.0f,-1.0f,      0.0f,1.0f,0.0f,  1.0f,0.0f,
        -1.0f, 0.0f,-1.0f,      0.0f,1.0f,0.0f,  0.0f,0.0f
    };

    glGenVertexArrays(1, &m_VAO_Plane);
    glGenBuffers(1, &m_VBO_Plane);
    glBindVertexArray(m_VAO_Plane);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Plane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(f_PlaneVertices), f_PlaneVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void EmptyEnvironmentState::OnEnter(Core::Application* p_App) {
    if (p_App && !p_App->IsTrainingMode()) {
        glEnable(GL_DEPTH_TEST);
        CreateShaders();
        CreatePlane();
        TryLoadGeneratedMap(p_App);

        // HUD setup: load camera icon and hook toggle
        std::string s_IconPath = p_App->GetAssetPath("icons/camera_icon.png");
        UI::LoadCameraIconPNG(s_IconPath.c_str(), p_App);
        UI::SetCameraToggleCallback([this]() { this->m_b_Camera3D = !this->m_b_Camera3D; });

        // Initialize top bar with labels and hide all counts (no players here)
        std::vector<std::string> vec_Labels = { "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus" };
        std::vector<UI::Color> vec_Colors = {
            {0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 1.0f},      // Black
            {0xE2 / 255.0f, 0x70 / 255.0f, 0x3F / 255.0f, 1.0f},      // Taxi (orange)
            {0xED / 255.0f, 0xD1 / 255.0f, 0x00 / 255.0f, 1.0f},      // Double (yellow)
            {0xF5 / 255.0f, 0x51 / 255.0f, 0xAE / 255.0f, 1.0f},      // Metro (pink/magenta)
            {0x41 / 255.0f, 0x84 / 255.0f, 0x3D / 255.0f, 1.0f},      // Bus (green)
        };
        std::vector<int> vec_Counts = { -1, -1, -1, -1, -1, -1 };
        UI::SetTopBar(vec_Labels, vec_Colors, vec_Counts);
        UI::SetMrXButtonsVisible(false);
        UI::SetMrXButtonsEnabled(false, false);

        // Initialize default ticket slots and round for full HUD visuals
        std::vector<UI::TicketSlot> vec_Slots(UI::k_TicketSlotCount);
        UI::SetTicketStates(vec_Slots);
        UI::SetRound(1);
    }
    
    UI::SetPauseCallback([this]() {
        UI::ShowPausedModal(true);
    });

    UI::SetPausedResumeCallback([this]() {
        UI::ShowPausedModal(false);
        this->m_b_GameActive = true;
    });

    UI::SetPausedDebugCallback([this]() {
    });

    UI::SetPausedMenuCallback([this, p_App]() {
        UI::ShowPausedModal(false);
        if (p_App && p_App->GetStateManager()) {
            p_App->GetStateManager()->ChangeState("menu", p_App);
        }
    });
}

void EmptyEnvironmentState::OnExit(Core::Application* p_App) {
    if (m_VBO_Plane) {
        glDeleteBuffers(1, &m_VBO_Plane);
        m_VBO_Plane = 0;
    }
    if (m_VAO_Plane) {
        glDeleteVertexArrays(1, &m_VAO_Plane);
        m_VAO_Plane = 0;
    }
    if (m_ShaderProgram) {
        glDeleteProgram(m_ShaderProgram);
        m_ShaderProgram = 0;
    }
    if (m_TextureID) {
        p_App->UnloadTexture(m_TextureID);
        m_TextureID = 0;
    }
    m_b_HasMapTexture = false;
}

void EmptyEnvironmentState::OnPause() {
    m_b_GameActive = false;
}

void EmptyEnvironmentState::OnResume() {
    m_b_GameActive = true;
}

void EmptyEnvironmentState::TryLoadGeneratedMap(Core::Application* p_App) {
    std::string s_Path = "generated_map.bmp";
    if (std::filesystem::exists(s_Path)) {
        m_TextureID = p_App->LoadTexture(s_Path);
        m_b_HasMapTexture = (m_TextureID != 0);
    } else {
        m_b_HasMapTexture = false;
    }
}

void EmptyEnvironmentState::Update(float f_DeltaTime) {
    if (!m_b_GameActive) return;  
    if (!m_b_Camera3D) return;

    // Angle friction and limits
    m_f_CameraAngleVelocity *= k_CameraScrollFriction;
    if (fabs(m_f_CameraAngleVelocity) < 0.0001f) {
        m_f_CameraAngleVelocity = 0.0f;
    }

    m_f_CameraAngle += m_f_CameraAngleVelocity;

    if (m_f_CameraAngle < k_MinCameraAngle) {
        m_f_CameraAngle = k_MinCameraAngle;
        m_f_CameraAngleVelocity = 0.0f;
    }
    if (m_f_CameraAngle > k_MaxCameraAngle) {
        m_f_CameraAngle = k_MaxCameraAngle;
        m_f_CameraAngleVelocity = 0.0f;
    }

    // Update front vector from angle
    m_vec3_CameraFront.x = 0.0f;
    m_vec3_CameraFront.y = sin(m_f_CameraAngle);
    m_vec3_CameraFront.z = -cos(m_f_CameraAngle);
    m_vec3_CameraFront = glm::normalize(m_vec3_CameraFront);

    // Scroll-to-forward acceleration
    if (fabs(m_f_CameraAngleVelocity) > 0.0001f) {
        glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
        float f_ForwardAccel = -m_f_CameraAngleVelocity * k_CameraScrollToForwardRatio;
        m_vec3_CameraVelocity += vec3_Forward * f_ForwardAccel;

        float f_Speed = glm::length(m_vec3_CameraVelocity);
        if (f_Speed > k_MaxCameraSpeed) {
            m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
        }
    }

    // Keyboard (WASD)
    const Uint8* p_KeyState = SDL_GetKeyboardState(nullptr);
    if (p_KeyState[SDL_SCANCODE_W]) AccelerateCameraForward(f_DeltaTime);
    if (p_KeyState[SDL_SCANCODE_S]) AccelerateCameraBackward(f_DeltaTime);
    if (p_KeyState[SDL_SCANCODE_A]) AccelerateCameraLeft(f_DeltaTime);
    if (p_KeyState[SDL_SCANCODE_D]) AccelerateCameraRight(f_DeltaTime);

    // Physics integration
    UpdateCameraPhysics(f_DeltaTime);
}

void EmptyEnvironmentState::Render(Core::Application* p_App) {
    if (p_App->IsTrainingMode()) return;

    glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int i_W = p_App->GetWidth();
    int i_H = p_App->GetHeight();

    glm::mat4 mat4_View, mat4_Projection;

    if (m_b_Camera3D) {
        m_vec3_CameraPosition = m_vec3_Saved3DCameraPosition;
        glm::vec3 vec3_Target = m_vec3_CameraPosition + m_vec3_CameraFront;
        mat4_View = glm::lookAt(m_vec3_CameraPosition, vec3_Target, m_vec3_CameraUp);
        mat4_Projection = glm::perspective(glm::radians(45.0f), (float)i_W / (float)i_H, 0.1f, 100.0f);
    } else {
        // Simple top-down ortho similar to GameState 2D mode
        glm::vec3 vec3_CamPos = glm::vec3(11.0f, 5.0f, 8.0f);
        glm::vec3 vec3_Target = glm::vec3(11.0f, 0.0f, 8.0f);
        glm::vec3 vec3_Up = glm::vec3(0.0f, 0.0f, -1.0f);
        mat4_View = glm::lookAt(vec3_CamPos, vec3_Target, vec3_Up);

        float f_Aspect = (float)i_W / (float)i_H;
        float f_HalfHeight = 10.0f;
        float f_HalfWidth = f_HalfHeight * f_Aspect;
        mat4_Projection = glm::ortho(-f_HalfWidth, f_HalfWidth, -f_HalfHeight, f_HalfHeight, 0.1f, 10.0f);
    }

    glm::mat4 mat4_Model(1.0f);
    glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;

    glUseProgram(m_ShaderProgram);
    GLuint i_LocMVP = glGetUniformLocation(m_ShaderProgram, "uMVP");
    glUniformMatrix4fv(i_LocMVP, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_b_HasMapTexture ? m_TextureID : 0);
    glUniform1i(glGetUniformLocation(m_ShaderProgram, "uTex"), 0);
    glUniform4f(glGetUniformLocation(m_ShaderProgram, "uColor"), 0.8f, 0.8f, 0.85f, 1.0f);

    glBindVertexArray(m_VAO_Plane);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // HUD
    UI::SetViewport(i_W, i_H);
    UI::RenderHUD(p_App);

    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

void EmptyEnvironmentState::RenderText(const std::string& s_Text, float f_X, float f_Y,
                                       float f_Scale, float f_R, float f_G, float f_B,
                                       Core::Application* p_App) {
    const auto& characters = p_App->GetCharacterMap();
    GLuint i_Program = p_App->GetTextShaderProgram();
    GLuint i_VAO = p_App->GetTextVAO();
    GLuint i_VBO = p_App->GetTextVBO();

    glUseProgram(i_Program);
    glUniform3f(glGetUniformLocation(i_Program, "textColor"), f_R, f_G, f_B);
    glUniformMatrix4fv(glGetUniformLocation(i_Program, "projection"), 1, GL_FALSE,
        glm::value_ptr(glm::ortho(0.0f, (float)p_App->GetWidth(), 0.0f, (float)p_App->GetHeight())));
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(i_VAO);

    for (auto c : s_Text) {
        auto it = characters.find(c);
        if (it == characters.end()) continue;

        const Core::Character& ch = it->second;
        float f_Xpos = f_X + ch.m_i_BearingX * f_Scale;
        float f_Ypos = f_Y + ch.m_i_BearingY * f_Scale;
        float f_W = ch.m_i_Width * f_Scale;
        float f_H = ch.m_i_Height * f_Scale;

        float f_Vertices[6][4] = {
            {f_Xpos,         f_Ypos - f_H, 0.0f, 1.0f},
            {f_Xpos,         f_Ypos,       0.0f, 0.0f},
            {f_Xpos + f_W,   f_Ypos,       1.0f, 0.0f},
            {f_Xpos,         f_Ypos - f_H, 0.0f, 1.0f},
            {f_Xpos + f_W,   f_Ypos,       1.0f, 0.0f},
            {f_Xpos + f_W,   f_Ypos - f_H, 1.0f, 1.0f}
        };

        glBindTexture(GL_TEXTURE_2D, ch.m_TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, i_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(f_Vertices), f_Vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        f_X += (ch.m_i_Advance >> 6) * f_Scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void EmptyEnvironmentState::AccelerateCameraForward(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
    m_vec3_CameraVelocity += vec3_Forward * k_CameraAcceleration * f_DeltaTime;

    float f_Speed = glm::length(m_vec3_CameraVelocity);
    if (f_Speed > k_MaxCameraSpeed) {
        m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
    }
}

void EmptyEnvironmentState::AccelerateCameraBackward(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
    m_vec3_CameraVelocity += -vec3_Forward * k_CameraAcceleration * f_DeltaTime;

    float f_Speed = glm::length(m_vec3_CameraVelocity);
    if (f_Speed > k_MaxCameraSpeed) {
        m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
    }
}

void EmptyEnvironmentState::AccelerateCameraLeft(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
    glm::vec3 vec3_Left = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), vec3_Forward));
    m_vec3_CameraVelocity += vec3_Left * k_CameraAcceleration * f_DeltaTime;

    float f_Speed = glm::length(m_vec3_CameraVelocity);
    if (f_Speed > k_MaxCameraSpeed) {
        m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
    }
}

void EmptyEnvironmentState::AccelerateCameraRight(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
    glm::vec3 vec3_Right = glm::normalize(glm::cross(vec3_Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_vec3_CameraVelocity += vec3_Right * k_CameraAcceleration * f_DeltaTime;

    float f_Speed = glm::length(m_vec3_CameraVelocity);
    if (f_Speed > k_MaxCameraSpeed) {
        m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
    }
}

void EmptyEnvironmentState::UpdateCameraPhysics(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    m_vec3_CameraVelocity *= k_CameraFriction;
    if (glm::length(m_vec3_CameraVelocity) < 0.05f) {
        m_vec3_CameraVelocity = glm::vec3(0.0f);
    }

    float f_OriginalY = m_vec3_Saved3DCameraPosition.y;
    m_vec3_Saved3DCameraPosition += m_vec3_CameraVelocity * f_DeltaTime;
    m_vec3_Saved3DCameraPosition.y = f_OriginalY;
}

void EmptyEnvironmentState::HandleEvent(const SDL_Event& ev, Core::Application* p_App) {
    if (ev.type == SDL_KEYDOWN) {
        switch (ev.key.keysym.sym) {
        case SDLK_g:
            p_App->GetStateManager()->PushState("mapgen", p_App);
            break;
        case SDLK_l:
            TryLoadGeneratedMap(p_App);
            break;
        case SDLK_ESCAPE:
            p_App->GetStateManager()->ChangeState("menu", p_App);
            break;
        }
    }

    if (ev.type == SDL_MOUSEWHEEL && m_b_Camera3D) {
        float f_ScrollInput = ev.wheel.y * k_CameraScrollAcceleration;
        m_f_CameraAngleVelocity -= f_ScrollInput;
    }

    if (ev.type == SDL_MOUSEBUTTONDOWN) {
        UI::HandleMouseClick(ev.button.x, ev.button.y);
    }

    if (ev.type == SDL_MOUSEMOTION) {
        UI::HandleMouseMotion(ev.motion.x, ev.motion.y);
    }
}

} // namespace States
} // namespace ScotlandYard
