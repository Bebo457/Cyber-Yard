#include "GameState.h"
#include "Application.h"
#include <GL/glew.h>

//Loader obrazów
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace ScotlandYard {
namespace States {

GameState::GameState()
    : m_b_GameActive(false)
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
    circlePositions = {
        {-0.9f, -0.9f},
        {-0.5f, -0.9f},
        { 0.0f, -0.9f},
        { 0.5f, -0.9f},
        { 0.9f, -0.9f}
        // dodaj kolejne według planszy
    };

    // ==============================
    // VAO/VBO planszy
    // ==============================
    glGenVertexArrays(1, &VAO_plane);
    glGenBuffers(1, &VBO_plane);

    glBindVertexArray(VAO_plane);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_plane);
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
    float radius = 0.05f;
    int segments = 30;
    std::vector<float> circleVertices = generateCircleVertices(radius, segments);
    circleVertexCount = static_cast<int>(circleVertices.size() / 3);

    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);

    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float), circleVertices.data(), GL_STATIC_DRAW);

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

    shaderProgram_plane = glCreateProgram();
    glAttachShader(shaderProgram_plane, vertexShader);
    glAttachShader(shaderProgram_plane, fragmentShader);
    glLinkProgram(shaderProgram_plane);

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

    circleShaderProgram = glCreateProgram();
    glAttachShader(circleShaderProgram, cVertexShader);
    glAttachShader(circleShaderProgram, cFragmentShader);
    glLinkProgram(circleShaderProgram);

    glDeleteShader(cVertexShader);
    glDeleteShader(cFragmentShader);

    // ========================================
    // Ładowanie tekstury z pliku
    // ========================================
    int texWidth, texHeight, texChannels;
    unsigned char* data = stbi_load("C:/Users/milos/Desktop/Testy/Cyber-Yard/program/assets/Scotland_Yard_schematic.png", &texWidth, &texHeight, &texChannels, 0);
    if (!data) {
        printf("Nie udalo sie zaladowac tekstury: \n");
    } else {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

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
    glDeleteVertexArrays(1, &VAO_plane);
    glDeleteBuffers(1, &VBO_plane);
    glDeleteProgram(shaderProgram_plane);
    glDeleteProgram(circleShaderProgram);
    if (textureID)
        glDeleteTextures(1, &textureID);
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
    // rotation += deltaTime * 50.0f; // obrót w stopniach na sekundę
}

void GameState::Render(Core::Application* p_App) {
    // Use OpenGL for game rendering
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram_plane);

    // Ustawienie macierzy modelu (obrót)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 1.0f, 0.0f));

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

    GLuint mvpLoc = glGetUniformLocation(shaderProgram_plane, "MVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));

    // Ustawienie tekstury
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    GLuint texLoc = glGetUniformLocation(shaderProgram_plane, "ourTexture");
    glUniform1i(texLoc, 0);

    // Rysowanie planszy
    glBindVertexArray(VAO_plane);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Rysowanie kółek
    glUseProgram(circleShaderProgram); // prosty shader kolorowy czerwony

    for (auto& pos : circlePositions) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(pos.x, 0.01f, pos.y)); // Y lekko nad planszą
        glm::mat4 MVP = projection * view * model;

        GLuint mvpLoc = glGetUniformLocation(circleShaderProgram, "MVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));

        glBindVertexArray(circleVAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, circleVertexCount);
        glBindVertexArray(0);
    }

    // Zmiana buforów (SDL)
    SDL_GL_SwapWindow(m_p_Window);
    
    // Game rendering code will go here
    
    // Swap OpenGL buffers for game
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

// Generowanie wierzchołków koła
std::vector<float> GameState::generateCircleVertices(float radius, int segments) {
    std::vector<float> vertices;

    // Środek koła
    vertices.push_back(0.0f); // X
    vertices.push_back(0.01f); // Y lekko nad płaszczyzną
    vertices.push_back(0.0f); // Z

    for (int i = 0; i <= segments; i++) {
        float theta = 2.0f * 3.1415926f * i / segments;
        float x = radius * cos(theta);
        float z = radius * sin(theta);
        vertices.push_back(x);
        vertices.push_back(0.01f);
        vertices.push_back(z);
    }

    return vertices;
}

} // namespace States
} // namespace ScotlandYard
