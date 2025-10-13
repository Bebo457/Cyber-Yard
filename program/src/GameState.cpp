#include "GameState.h"
#include "Application.h"
#include <GL/glew.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#include "HUDOverlay.h"

namespace ScotlandYard {
namespace States {

GameState::GameState()
    : m_b_GameActive(false)
    , m_b_Camera3D(true)
    , m_b_TexturesLoaded(false)
    , m_VAO_Plane(0)
    , m_VBO_Plane(0)
    , m_ShaderProgram_Plane(0)
    , m_ShaderProgram_Circle(0)
    , m_VAO_Circle(0)
    , m_VBO_Circle(0)
    , m_i_CircleVertexCount(0)
    , m_p_Window(nullptr)
    , m_f_Rotation(0.0f)
    , m_i_Width(800)
    , m_i_Height(600)
    , m_TextureID(0)
    , m_vec3_CameraPosition(0.0f, 1.5f, 4.0f)
    , m_vec3_CameraVelocity(0.0f, 0.0f, 0.0f)
    , m_vec3_CameraFront(0.0f, -0.3f, -1.0f)
    , m_vec3_CameraUp(0.0f, 1.0f, 0.0f)
{
}

GameState::~GameState() {}

void GameState::OnEnter() {
    m_b_GameActive = true;

    // ==============================
    // Dane wierzchołków (pozycja, kolor, UV)
    // ==============================
    float size = 1.0f;
    float planeVertices[] = {
        -size, 0.0f, -size,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         size, 0.0f, -size,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
         size, 0.0f,  size,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,

        -size, 0.0f, -size,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         size, 0.0f,  size,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        -size, 0.0f,  size,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f
    };

    // ==============================
    // Pozycje kółek
    // ==============================
    m_vec_CirclePositions = {
        {-0.9f, -0.9f},
        {-0.5f, -0.9f},
        { 0.0f, -0.9f},
        { 0.5f, -0.9f},
        { 0.9f, -0.9f}
    };

    // ==============================
    // VAO/VBO planszy
    // ==============================
    glGenVertexArrays(1, &m_VAO_Plane);
    glGenBuffers(1, &m_VBO_Plane);

    glBindVertexArray(m_VAO_Plane);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Plane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // ==============================
    // VAO/VBO kółek
    // ==============================
    float f_Radius = 0.05f;
    int i_Segments = 30;
    std::vector<float> vec_CircleVertices = generateCircleVertices(f_Radius, i_Segments);
    m_i_CircleVertexCount = static_cast<int>(vec_CircleVertices.size() / 3);

    glGenVertexArrays(1, &m_VAO_Circle);
    glGenBuffers(1, &m_VBO_Circle);

    glBindVertexArray(m_VAO_Circle);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Circle);
    glBufferData(GL_ARRAY_BUFFER, vec_CircleVertices.size() * sizeof(float), vec_CircleVertices.data(), GL_STATIC_DRAW);

    // tylko pozycja
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // ==============================
    // Shadery planszy
    // ==============================
    const char* vertexShaderSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aColor;
        layout(location = 2) in vec2 aTexCoord;
        uniform mat4 MVP;
        out vec3 vertexColor;
        out vec2 TexCoord;
        void main() {
            vertexColor = aColor;
            TexCoord = aTexCoord;
            gl_Position = MVP * vec4(aPos, 1.0);
        }
    )";

    const char* fragmentShaderSrc = R"(
        #version 330 core
        in vec3 vertexColor;
        in vec2 TexCoord;
        uniform sampler2D ourTexture;
        out vec4 FragColor;
        void main() {
            FragColor = texture(ourTexture, TexCoord);
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSrc, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);

    m_ShaderProgram_Plane = glCreateProgram();
    glAttachShader(m_ShaderProgram_Plane, vertexShader);
    glAttachShader(m_ShaderProgram_Plane, fragmentShader);
    glLinkProgram(m_ShaderProgram_Plane);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // ==============================
    // Shadery kółek
    // ==============================
    const char* circleVertexShaderSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 MVP;
        void main() {
            gl_Position = MVP * vec4(aPos, 1.0);
        }
    )";

    const char* circleFragmentShaderSrc = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )";

    GLuint cVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(cVertexShader, 1, &circleVertexShaderSrc, nullptr);
    glCompileShader(cVertexShader);

    GLuint cFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(cFragmentShader, 1, &circleFragmentShaderSrc, nullptr);
    glCompileShader(cFragmentShader);

    m_ShaderProgram_Circle = glCreateProgram();
    glAttachShader(m_ShaderProgram_Circle, cVertexShader);
    glAttachShader(m_ShaderProgram_Circle, cFragmentShader);
    glLinkProgram(m_ShaderProgram_Circle);

    glDeleteShader(cVertexShader);
    glDeleteShader(cFragmentShader);

    UI::SetCameraToggleCallback([this]() {
        m_b_Camera3D = !m_b_Camera3D;
        });

    glEnable(GL_DEPTH_TEST);
}

void GameState::LoadTextures(Core::Application* p_App) {
    if (m_b_TexturesLoaded) return;

    std::string s_TexturePath = p_App->GetAssetPath("textures/Scotland_Yard_schematic.png");
    m_TextureID = p_App->LoadTexture(s_TexturePath);
    if (m_TextureID == 0) {
        printf("Failed to load board texture\n");
    }

    std::string s_IconPath = p_App->GetAssetPath("icons/camera_icon.png");
    UI::LoadCameraIconPNG(s_IconPath.c_str(), p_App);

    m_b_TexturesLoaded = true;
}

void GameState::OnExit() {
    if (m_VAO_Plane) {
        glDeleteVertexArrays(1, &m_VAO_Plane);
        m_VAO_Plane = 0;
    }
    if (m_VBO_Plane) {
        glDeleteBuffers(1, &m_VBO_Plane);
        m_VBO_Plane = 0;
    }
    if (m_VAO_Circle) {
        glDeleteVertexArrays(1, &m_VAO_Circle);
        m_VAO_Circle = 0;
    }
    if (m_VBO_Circle) {
        glDeleteBuffers(1, &m_VBO_Circle);
        m_VBO_Circle = 0;
    }
    if (m_ShaderProgram_Plane) {
        glDeleteProgram(m_ShaderProgram_Plane);
        m_ShaderProgram_Plane = 0;
    }
    if (m_ShaderProgram_Circle) {
        glDeleteProgram(m_ShaderProgram_Circle);
        m_ShaderProgram_Circle = 0;
    }
    if (m_TextureID) {
        glDeleteTextures(1, &m_TextureID);
        m_TextureID = 0;
    }
    m_b_GameActive = false;
}

void GameState::OnPause() {
    m_b_GameActive = false;
}

void GameState::OnResume() {
    m_b_GameActive = true;
}

void GameState::Update(float f_DeltaTime) {
    if (!m_b_GameActive) return;
    UpdateCameraPhysics(f_DeltaTime);
}

void GameState::Render(Core::Application* p_App) {
    // Load textures on first render
    LoadTextures(p_App);

    m_i_Width = p_App->GetWidth();
    m_i_Height = p_App->GetHeight();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_ShaderProgram_Plane);

    // Model matrix (rotation)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(m_f_Rotation), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 view, projection;

    if (m_b_Camera3D) {
        glm::vec3 vec3_CameraTarget = m_vec3_CameraPosition + m_vec3_CameraFront;
        view = glm::lookAt(m_vec3_CameraPosition, vec3_CameraTarget, m_vec3_CameraUp);
        projection = glm::perspective(glm::radians(45.0f),
            (float)m_i_Width / (float)m_i_Height,
            0.1f, 100.0f);
    }
    else {
        // 2D orthographic (top-down)
        m_vec3_CameraPosition = glm::vec3(0.0f, 2.0f, 5.0f);
        m_vec3_CameraVelocity = glm::vec3(0.0f, 0.0f, 0.0f);

        view = glm::lookAt(
            glm::vec3(0.0f, 10.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f)
        );

        float f_Aspect = (float)m_i_Width / (float)m_i_Height;
        float f_HalfHeight = 1.1f;
        float f_HalfWidth = f_HalfHeight * f_Aspect;

        projection = glm::ortho(
            -f_HalfWidth, f_HalfWidth,
            -f_HalfHeight, f_HalfHeight,
            0.1f, 20.0f
        );
    }

    glm::mat4 MVP = projection * view * model;

    GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Plane, "MVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));

    // Ustawienie tekstury
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    GLuint texLoc = glGetUniformLocation(m_ShaderProgram_Plane, "ourTexture");
    glUniform1i(texLoc, 0);

    // Rysowanie planszy
    glBindVertexArray(m_VAO_Plane);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Rysowanie kółek
    glUseProgram(m_ShaderProgram_Circle);

    for (auto& pos : m_vec_CirclePositions) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(pos.x, 0.01f, pos.y));
        glm::mat4 MVP = projection * view * model;

        GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Circle, "MVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));

        glBindVertexArray(m_VAO_Circle);
        glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);
        glBindVertexArray(0);
    }

    ScotlandYard::UI::SetRound(1);
    ScotlandYard::UI::RenderHUD(p_App);


    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

void GameState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
    if (event.type == SDL_KEYDOWN) {
        float f_DeltaTime = p_App->GetDeltaTime();

        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                // Can set exit flag or can not I dont know do what you want, but like can we do what we want?
                break;

            case SDLK_w:
                AccelerateCameraForward(f_DeltaTime);
                break;

            case SDLK_s:
                AccelerateCameraBackward(f_DeltaTime);
                break;

            case SDLK_a:
                AccelerateCameraLeft(f_DeltaTime);
                break;

            case SDLK_d:
                AccelerateCameraRight(f_DeltaTime);
                break;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int i_X = event.button.x;
        int i_Y = event.button.y;

        UI::HandleMouseClick(i_X, i_Y);
    }
}

std::vector<float> GameState::generateCircleVertices(float f_Radius, int i_Segments) {
    std::vector<float> vec_Vertices;

    vec_Vertices.push_back(0.0f);
    vec_Vertices.push_back(0.01f);
    vec_Vertices.push_back(0.0f);

    constexpr float k_Pi = 3.14159265358979323846f;
    for (int i = 0; i <= i_Segments; i++) {
        float f_Theta = 2.0f * k_Pi * i / i_Segments;
        float f_X = f_Radius * cos(f_Theta);
        float f_Z = f_Radius * sin(f_Theta);
        vec_Vertices.push_back(f_X);
        vec_Vertices.push_back(0.01f);
        vec_Vertices.push_back(f_Z);
    }

    return vec_Vertices;
}

void GameState::AccelerateCameraForward(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(m_vec3_CameraFront);
    float f_CurrentSpeed = glm::dot(m_vec3_CameraVelocity, vec3_Forward);

    if (f_CurrentSpeed < k_MaxCameraSpeed) {
        m_vec3_CameraVelocity += vec3_Forward * k_CameraAcceleration * f_DeltaTime;

        float f_NewSpeed = glm::length(m_vec3_CameraVelocity);
        if (f_NewSpeed > k_MaxCameraSpeed) {
            m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
        }
    }
}

void GameState::AccelerateCameraBackward(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(m_vec3_CameraFront);
    float f_CurrentSpeed = glm::dot(m_vec3_CameraVelocity, -vec3_Forward);

    if (f_CurrentSpeed < k_MaxCameraSpeed) {
        m_vec3_CameraVelocity += -vec3_Forward * k_CameraAcceleration * f_DeltaTime;

        float f_NewSpeed = glm::length(m_vec3_CameraVelocity);
        if (f_NewSpeed > k_MaxCameraSpeed) {
            m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
        }
    }
}

void GameState::AccelerateCameraLeft(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Left = glm::normalize(glm::cross(m_vec3_CameraUp, m_vec3_CameraFront));
    float f_CurrentSpeed = glm::dot(m_vec3_CameraVelocity, vec3_Left);

    if (f_CurrentSpeed < k_MaxCameraSpeed) {
        m_vec3_CameraVelocity += vec3_Left * k_CameraAcceleration * f_DeltaTime;

        float f_NewSpeed = glm::length(m_vec3_CameraVelocity);
        if (f_NewSpeed > k_MaxCameraSpeed) {
            m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
        }
    }
}

void GameState::AccelerateCameraRight(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Right = glm::normalize(glm::cross(m_vec3_CameraFront, m_vec3_CameraUp));
    float f_CurrentSpeed = glm::dot(m_vec3_CameraVelocity, vec3_Right);

    if (f_CurrentSpeed < k_MaxCameraSpeed) {
        m_vec3_CameraVelocity += vec3_Right * k_CameraAcceleration * f_DeltaTime;

        float f_NewSpeed = glm::length(m_vec3_CameraVelocity);
        if (f_NewSpeed > k_MaxCameraSpeed) {
            m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
        }
    }
}

void GameState::UpdateCameraPhysics(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    // Apply friction
    m_vec3_CameraVelocity *= k_CameraFriction;

    // Update position
    m_vec3_CameraPosition += m_vec3_CameraVelocity * f_DeltaTime;
}

} // namespace States
} // namespace ScotlandYard