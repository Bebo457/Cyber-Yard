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

    // ========================================
    // Ładowanie tekstury z pliku
    // ========================================
    int texWidth, texHeight, texChannels;
    unsigned char* data = stbi_load("assets/textures/Scotland_Yard_schematic.png", &texWidth, &texHeight, &texChannels, 0);
    if (!data) {
        printf("Nie udalo sie zaladowac tekstury: \n");
    } else {
        glGenTextures(1, &m_TextureID);
        glBindTexture(GL_TEXTURE_2D, m_TextureID);

        // parametry tekstury
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLenum format = GL_RGB;
        if (texChannels == 4)
            format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_2D, 0, format, texWidth, texHeight, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);
        printf("Tekstura zaladowana pomyslnie: %dx%d\n", texWidth, texHeight);
        
    }

    glEnable(GL_DEPTH_TEST);
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
}

void GameState::Render(Core::Application* p_App) {
    m_i_Width = p_App->GetWidth();
    m_i_Height = p_App->GetHeight();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_ShaderProgram_Plane);

    // Ustawienie macierzy modelu (obrót)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(m_f_Rotation), glm::vec3(0.0f, 1.0f, 0.0f));

    // Widok (kamera)
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 2.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    // Projekcja (perspektywa)
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        (float)m_i_Width / (float)m_i_Height,
        0.1f, 100.0f
    );

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

    ScotlandYard::UI::RenderSimpleHUD();

    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

void GameState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                // Można np. ustawić flagę wyjścia
                break;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int i_X = event.button.x;
        int i_Y = event.button.y;
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

} // namespace States
} // namespace ScotlandYard