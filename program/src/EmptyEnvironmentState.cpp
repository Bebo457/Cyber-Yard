#include "EmptyEnvironmentState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"
#include "RoadGenerator.h"
#include "SampleMapDataGenerator.h"
#include "MapDataSerializer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <iostream>

namespace ScotlandYard {
namespace States {

EmptyEnvironmentState::EmptyEnvironmentState() {}
EmptyEnvironmentState::~EmptyEnvironmentState() = default;

void EmptyEnvironmentState::CreateShaders() {
    // --- OLD SHADER (keep as is) ---
    const char* vsSrc = R"(#version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNormal;
        layout(location=2) in vec2 aUV;

        uniform mat4 uMVP;
        out vec2 vUV;

        void main(){
            vUV = aUV;
            gl_Position = uMVP * vec4(aPos,1.0);
        }
    )";

    const char* fsSrc = R"(#version 330 core
        in vec2 vUV;

        uniform sampler2D uSidewalk;
        uniform sampler2D uGrass;
        uniform sampler2D uMask;      
        uniform int uUseMask;         
        uniform vec2 uTileUV;         

        out vec4 FragColor;

        void main(){
            vec2 tiledUV = vUV * uTileUV;

            vec4 sidewalk = texture(uSidewalk, tiledUV);
            vec4 grass    = texture(uGrass, tiledUV);

            if(uUseMask == 1){
                vec3 m = texture(uMask, vUV).rgb;

                // parks = green = grass
                float park = step(0.35, m.g);

                FragColor = mix(sidewalk, grass, park);
            } else {
                FragColor = sidewalk;
            }
        }
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

    // --- NEW HARD-MASK SHADER FOR ROAD ---
    const char* roadVsSrc = R"(#version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNormal;
        layout(location=2) in vec2 aUV;

        uniform mat4 uMVP;
        out vec2 vUV;

        void main() {
            vUV = aUV;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";

    const char* roadFsSrc = R"(#version 330 core
        in vec2 vUV;

        uniform sampler2D uRoad;      // only one texture
        uniform vec2 uTileUV;

        out vec4 FragColor;

        void main() {
            vec2 tiledUV = vUV * uTileUV;
            FragColor = texture(uRoad, tiledUV);
        }
    )";

    GLuint roadVs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(roadVs, 1, &roadVsSrc, nullptr);
    glCompileShader(roadVs);

    GLuint roadFs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(roadFs, 1, &roadFsSrc, nullptr);
    glCompileShader(roadFs);

    m_ShaderRoad = glCreateProgram();  // <-- new member variable GLuint m_ShaderRoad
    glAttachShader(m_ShaderRoad, roadVs);
    glAttachShader(m_ShaderRoad, roadFs);
    glLinkProgram(m_ShaderRoad);

    glDeleteShader(roadVs);
    glDeleteShader(roadFs);
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
    bool test=true;
    CreateTestRoad(p_App);

    if (p_App && !p_App->IsTrainingMode() and test==true) {
        const_cast<Core::Application*>(p_App)->UpdateUIScaling();

        glEnable(GL_DEPTH_TEST);
        CreateShaders();
        CreatePlane();
        m_TexSidewalk = p_App->LoadTexture(p_App->GetAssetPath("textures/sidewalk.png"));
        m_TexGrass = p_App->LoadTexture(p_App->GetAssetPath("textures/grass.png"));
        TryLoadGeneratedMap(p_App);

        // NEW: Load sample map data for testing
        LoadSampleMapData();

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

        // water renderer
        m_p_WaterRenderer = std::make_unique<Rendering::WaterRenderer>();
        m_p_WaterRenderer->Initialize();
        m_p_WaterRenderer->SetWaterHeight(0.1f);
        m_mat4_GlobalScaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f));
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
    if (m_TexSidewalk) { p_App->UnloadTexture(m_TexSidewalk); m_TexSidewalk = 0; }
    if (m_TexGrass) { p_App->UnloadTexture(m_TexGrass);    m_TexGrass = 0; }
    if (m_TexMask) { p_App->UnloadTexture(m_TexMask);     m_TexMask = 0; }
    m_b_UseMask = false;

    if (m_p_WaterRenderer) {
        m_p_WaterRenderer->Cleanup();
        m_p_WaterRenderer.reset();
    }

    if (m_VBO_Road) { glDeleteBuffers(1, &m_VBO_Road); m_VBO_Road = 0; }
    if (m_EBO_Road) { glDeleteBuffers(1, &m_EBO_Road); m_EBO_Road = 0; }
    if (m_VAO_Road) { glDeleteVertexArrays(1, &m_VAO_Road); m_VAO_Road = 0; }
    if (m_TexRoad) { p_App->UnloadTexture(m_TexRoad); m_TexRoad = 0; }

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
        m_TexMask = p_App->LoadTexture(s_Path);
        m_b_UseMask = (m_TexMask != 0);
    }
    else {
        m_b_UseMask = false;
        m_TexMask = 0;
    }
}

void EmptyEnvironmentState::Update(float f_DeltaTime) {
    m_f_Time += f_DeltaTime;

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

    int i_W = p_App->GetVirtualWidth();
    int i_H = p_App->GetVirtualHeight();

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

    GLint mvpLoc = glGetUniformLocation(m_ShaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    // Tiling
    GLint tileLoc = glGetUniformLocation(m_ShaderProgram, "uTileUV");
    glUniform2f(tileLoc, 12.0f, 9.0f);

    // Bind sidewalk
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TexSidewalk);
    glUniform1i(glGetUniformLocation(m_ShaderProgram, "uSidewalk"), 0);

    // Bind grass
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_TexGrass);
    glUniform1i(glGetUniformLocation(m_ShaderProgram, "uGrass"), 1);

    // Bind mask
    glUniform1i(glGetUniformLocation(m_ShaderProgram, "uUseMask"), m_b_UseMask ? 1 : 0);
    if (m_b_UseMask) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_TexMask);
        glUniform1i(glGetUniformLocation(m_ShaderProgram, "uMask"), 2);
    }

    glBindVertexArray(m_VAO_Plane);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // --- Render road ---
    if (m_VAO_Road && m_TexRoad) {
        glUseProgram(m_ShaderRoad);

        // MVP matrix
        GLint mvpLoc = glGetUniformLocation(m_ShaderRoad, "uMVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_Projection * mat4_View));

        // Tiling
        GLint tileLoc = glGetUniformLocation(m_ShaderRoad, "uTileUV");
        glUniform2f(tileLoc, 2.0f, 2.0f); // adjust repeats as needed

        // Bind road texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_TexRoad);
        glUniform1i(glGetUniformLocation(m_ShaderRoad, "uRoad"), 0);

        // Draw the road mesh
        glBindVertexArray(m_VAO_Road);
        glDrawElements(GL_TRIANGLES, m_RoadIndexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }


    // Render water
    if (m_p_WaterRenderer) {
        m_p_WaterRenderer->Render(mat4_Projection * mat4_View, m_f_Time, m_mat4_GlobalScaleMatrix);
    }

    // HUD
    UI::SetViewport(p_App->GetVirtualWidth(), p_App->GetVirtualHeight());
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
        glm::value_ptr(glm::ortho(0.0f, (float)p_App->GetVirtualWidth(), 0.0f, (float)p_App->GetVirtualHeight())));
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

void EmptyEnvironmentState::CreateTestRoad(Core::Application* p_App) {
    std::vector<glm::vec2> roadPoints;

    int segments = 20;
    float radius = 10.0f; // larger radius
    glm::vec2 center(0.0f, 0.0f);
    for (int i = 0; i <= segments; ++i) {
        float angle = glm::half_pi<float>() * i / segments; // 90 degree curve
        float x = center.x + radius * cos(angle);
        float y = center.y + radius * sin(angle);
        roadPoints.emplace_back(x, y);
    }

    std::vector<float> roadWidths;
    for (int i = 0; i <= segments; ++i) {
        // float w = 2.0f + 2.0f * sin((float)i / segments * glm::half_pi<float>()); // road widens along the curve
        // float w = 1.0f; // constant width
        float w = 1.0f - 0.05f * i;
        roadWidths.push_back(w);
    }

    ScotlandYard::Core::RoadMesh roadMesh = ScotlandYard::Core::RoadGenerator::GenerateRoad(
        roadPoints,
        roadWidths,
        2.0f // texture repeats every 2 meters
    );

    for (auto& v : roadMesh.vertices) {
        v.y = 0.5f;
    }

    struct Vertex { glm::vec3 pos; glm::vec3 normal; glm::vec2 uv; };
    std::vector<Vertex> vertices;
    for (size_t i = 0; i < roadMesh.vertices.size(); ++i) {
        vertices.push_back({roadMesh.vertices[i], roadMesh.normals[i], roadMesh.texCoords[i]});
    }

    glGenVertexArrays(1, &m_VAO_Road);
    glGenBuffers(1, &m_VBO_Road);
    glGenBuffers(1, &m_EBO_Road);

    glBindVertexArray(m_VAO_Road);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Road);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_Road);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, roadMesh.indices.size() * sizeof(unsigned int),
                roadMesh.indices.data(), GL_STATIC_DRAW);

    // Vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)); // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)); // uv
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    m_RoadIndexCount = static_cast<int>(roadMesh.indices.size());

    // Load road texture
    m_TexRoad = p_App->LoadTexture(p_App->GetAssetPath("textures/road.jpg"));

    if (m_TexRoad == 0) {
        std::cerr << "[Road] Failed to load road texture!" << std::endl;
    } else {
        std::cout << "[Road] Road texture loaded successfully." << std::endl;
    }
}

// Load sample map data
void EmptyEnvironmentState::LoadSampleMapData() {
    std::cout << "[EmptyEnvironmentState] Loading sample map data..." << std::endl;

    // Try to load from saved file first
    std::string mapPath = "maps/example_city.symap";

    if (std::filesystem::exists(mapPath)) {
        std::cout << "[EmptyEnvironmentState] Found saved map file, loading..." << std::endl;
        m_MapData = MapGen::MapDataSerializer::LoadFromFile(mapPath);
    }

    // If loading failed or file doesn't exist, generate new map
    if (m_MapData.IsEmpty()) {
        std::cout << "[EmptyEnvironmentState] No saved map found or loading failed, generating new map..." << std::endl;
        m_MapData = MapGen::SampleMapDataGenerator::GenerateRealisticCityMap(42);

        // Create maps directory if it doesn't exist
        std::filesystem::create_directories("maps");

        // Save the generated map for future use
        std::cout << "[EmptyEnvironmentState] Saving generated map to: " << mapPath << std::endl;
        MapGen::MapDataSerializer::SaveToFile(m_MapData, mapPath);
    } else {
        std::cout << "[EmptyEnvironmentState] Successfully loaded map from file!" << std::endl;
    }

    m_b_MapDataLoaded = true;

    // Print stats
    auto stats = m_MapData.GetStats();
    std::cout << "[EmptyEnvironmentState] Map loaded:" << std::endl;
    std::cout << "  - Graph nodes: " << stats.nodes << std::endl;
    std::cout << "  - Streets: " << stats.streets << std::endl;
    std::cout << "  - Buildings: " << stats.buildings << std::endl;
    std::cout << "  - Trees: " << stats.trees << std::endl;
    std::cout << "  - Parks: " << stats.parks << std::endl;
    std::cout << "  - Bridges: " << stats.bridges << std::endl;

    // DEBUG: Print some graph nodes
    // std::cout << "[EmptyEnvironmentState] First 3 graph nodes:" << std::endl;
    // for (int i = 0; i < std::min(3, static_cast<int>(m_MapData.vec_GraphNodes.size())); ++i) {
    //     const auto& node = m_MapData.vec_GraphNodes[i];
    //     std::cout << "  Node " << node.i_ID << ": pos=(" << node.position.x << ", " << node.position.y << ")"
    //               << ", taxi_connections=" << node.vec_TaxiConnections.size()
    //               << ", bus=" << node.vec_BusConnections.size()
    //               << ", metro=" << node.vec_MetroConnections.size()
    //               << std::endl;
    // }

    // DEBUG: Print some streets
    // std::cout << "[EmptyEnvironmentState] First 3 streets:" << std::endl;
    // for (int i = 0; i < std::min(3, static_cast<int>(m_MapData.vec_Streets.size())); ++i) {
    //     const auto& street = m_MapData.vec_Streets[i];
    //     std::cout << "  Street " << i << ": " << street.i_Node1 << " <-> " << street.i_Node2
    //               << ", tier=" << street.i_Tier
    //               << ", width=" << street.f_Width
    //               << ", inPark=" << (street.b_IsInPark ? "yes" : "no")
    //               << std::endl;
    // }
}

// render map data (placeholder - to be implemented by graphics team)
void EmptyEnvironmentState::RenderMapData(Core::Application* p_App) {
    if (!m_b_MapDataLoaded) return;

    // TODO: Implement rendering for:
    // 1. River - use WaterRenderer with m_MapData.vec_RiverPath
    // 2. Parks - render park polygons with grass texture
    // 3. Streets - use RoadGenerator for each street segment
    // 4. Buildings - use BuildingGenerator to create meshes
    // 5. Trees - use TreeGenerator for each tree instance
    // 6. Graph nodes - render as spheres/cubes for debugging

    // For now, just log that we would render
    // (Uncomment this when actually implementing rendering)
    /*
    std::cout << "[EmptyEnvironmentState] Rendering map data..." << std::endl;
    std::cout << "  - Would render " << m_MapData.vec_GraphNodes.size() << " graph nodes" << std::endl;
    std::cout << "  - Would render " << m_MapData.vec_Streets.size() << " streets" << std::endl;
    std::cout << "  - Would render " << m_MapData.vec_Buildings.size() << " buildings" << std::endl;
    std::cout << "  - Would render " << m_MapData.vec_Trees.size() << " trees" << std::endl;
    */
}


} // namespace States
} // namespace ScotlandYard
