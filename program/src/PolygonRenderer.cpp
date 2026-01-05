#include "PolygonRenderer.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace ScotlandYard {
namespace Rendering {

PolygonRenderer::PolygonRenderer() {}

PolygonRenderer::~PolygonRenderer() {
    Clear();
    if (m_ShaderProgram) glDeleteProgram(m_ShaderProgram);
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
}

bool PolygonRenderer::Initialize() {
    CreateShaders();

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    return m_ShaderProgram != 0 && m_VAO != 0;
}

void PolygonRenderer::CreateShaders() {
    const char* s_VertexShaderSource = R"(
        #version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNormal;
        layout(location=2) in vec2 aUV;

        uniform mat4 uMVP;

        out vec2 vUV;
        out vec3 vNormal;

        void main() {
            vUV = aUV;
            vNormal = aNormal;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";

    const char* s_FragmentShaderSource = R"(
        #version 330 core
        in vec2 vUV;
        in vec3 vNormal;

        uniform sampler2D uTexture;
        uniform vec2 uTileScale;

        out vec4 FragColor;

        void main() {
            vec2 tiledUV = vUV * uTileScale;
            FragColor = texture(uTexture, tiledUV);
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &s_VertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Polygon vertex shader compilation failed: " << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &s_FragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Polygon fragment shader compilation failed: " << infoLog << std::endl;
    }

    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vertexShader);
    glAttachShader(m_ShaderProgram, fragmentShader);
    glLinkProgram(m_ShaderProgram);

    glGetProgramiv(m_ShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ShaderProgram, 512, nullptr, infoLog);
        std::cerr << "Polygon shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void PolygonRenderer::SetPolygon(const std::vector<glm::vec2>& vec_Vertices, float f_YHeight) {
    if (vec_Vertices.size() < 3) return;

    m_f_YHeight = f_YHeight;
    m_vec_VertexData.clear();
    m_vec_Indices.clear();

    glm::vec2 minBounds(vec_Vertices[0]);
    glm::vec2 maxBounds(vec_Vertices[0]);

    for (const auto& v : vec_Vertices) {
        minBounds.x = std::min(minBounds.x, v.x);
        minBounds.y = std::min(minBounds.y, v.y);
        maxBounds.x = std::max(maxBounds.x, v.x);
        maxBounds.y = std::max(maxBounds.y, v.y);
    }

    glm::vec2 size = maxBounds - minBounds;

    for (const auto& v : vec_Vertices) {
        // Position (X, Y=height, Z)
        m_vec_VertexData.push_back(v.x);
        m_vec_VertexData.push_back(f_YHeight);
        m_vec_VertexData.push_back(v.y);

        // Normal (pointing up)
        m_vec_VertexData.push_back(0.0f);
        m_vec_VertexData.push_back(1.0f);
        m_vec_VertexData.push_back(0.0f);

        // UV coordinates
        float u = (v.x - minBounds.x) / size.x;
        float v_coord = (v.y - minBounds.y) / size.y;
        m_vec_VertexData.push_back(u);
        m_vec_VertexData.push_back(v_coord);
    }

    TriangulatePolygon(vec_Vertices);
    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vec_VertexData.size() * sizeof(float), m_vec_VertexData.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_vec_Indices.size() * sizeof(unsigned int), m_vec_Indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    m_i_VertexCount = static_cast<int>(m_vec_Indices.size());
}

void PolygonRenderer::TriangulatePolygon(const std::vector<glm::vec2>& vec_Vertices) {
    int n = static_cast<int>(vec_Vertices.size());

    for (int i = 1; i < n - 1; ++i) {
        m_vec_Indices.push_back(0);
        m_vec_Indices.push_back(i);
        m_vec_Indices.push_back(i + 1);
    }
}

void PolygonRenderer::SetRiverStrip(const std::vector<glm::vec2>& vec_CenterlinePath, float f_Width, float f_YHeight) {
    if (vec_CenterlinePath.size() < 2) return;

    m_f_YHeight = f_YHeight;
    m_vec_VertexData.clear();
    m_vec_Indices.clear();

    float halfWidth = f_Width * 0.5f;
    float totalDistance = 0.0f;
    std::vector<float> distances;
    distances.push_back(0.0f);

    for (size_t i = 1; i < vec_CenterlinePath.size(); ++i) {
        float segmentLength = glm::length(vec_CenterlinePath[i] - vec_CenterlinePath[i - 1]);
        totalDistance += segmentLength;
        distances.push_back(totalDistance);
    }

    for (size_t i = 0; i < vec_CenterlinePath.size(); ++i) {
        //calculate tangent
        glm::vec2 tangent;
        if (i == 0) {
            // First point: use direction to next point
            tangent = glm::normalize(vec_CenterlinePath[1] - vec_CenterlinePath[0]);
        } else if (i == vec_CenterlinePath.size() - 1) {
            // Last point: use direction from previous point
            tangent = glm::normalize(vec_CenterlinePath[i] - vec_CenterlinePath[i - 1]);
        } else {
            // Middle points: average of incoming and outgoing directions
            tangent = glm::normalize(vec_CenterlinePath[i + 1] - vec_CenterlinePath[i - 1]);
        }

        glm::vec2 normal(-tangent.y, tangent.x);
        glm::vec2 rightPos = vec_CenterlinePath[i] + normal * halfWidth;
        glm::vec2 leftPos = vec_CenterlinePath[i] - normal * halfWidth;
        float u = distances[i] / totalDistance;

        // Right vertex (v = 0)
        m_vec_VertexData.push_back(rightPos.x);
        m_vec_VertexData.push_back(f_YHeight);
        m_vec_VertexData.push_back(rightPos.y);
        m_vec_VertexData.push_back(0.0f);  // Normal up
        m_vec_VertexData.push_back(1.0f);
        m_vec_VertexData.push_back(0.0f);
        m_vec_VertexData.push_back(u);
        m_vec_VertexData.push_back(0.0f);  // v = 0 for right side

        // Left vertex (v = 1)
        m_vec_VertexData.push_back(leftPos.x);
        m_vec_VertexData.push_back(f_YHeight);
        m_vec_VertexData.push_back(leftPos.y);
        m_vec_VertexData.push_back(0.0f);  // Normal up
        m_vec_VertexData.push_back(1.0f);
        m_vec_VertexData.push_back(0.0f);
        m_vec_VertexData.push_back(u);
        m_vec_VertexData.push_back(1.0f);  // v = 1 for left side
    }

    for (size_t i = 0; i < vec_CenterlinePath.size() - 1; ++i) {
        unsigned int rightCurrent = static_cast<unsigned int>(i * 2);
        unsigned int leftCurrent = rightCurrent + 1;
        unsigned int rightNext = static_cast<unsigned int>((i + 1) * 2);
        unsigned int leftNext = rightNext + 1;

        // First triangle: rightCurrent, leftCurrent, rightNext
        m_vec_Indices.push_back(rightCurrent);
        m_vec_Indices.push_back(leftCurrent);
        m_vec_Indices.push_back(rightNext);

        // Second triangle: rightNext, leftCurrent, leftNext
        m_vec_Indices.push_back(rightNext);
        m_vec_Indices.push_back(leftCurrent);
        m_vec_Indices.push_back(leftNext);
    }

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vec_VertexData.size() * sizeof(float), m_vec_VertexData.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_vec_Indices.size() * sizeof(unsigned int), m_vec_Indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // UV attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    m_i_VertexCount = static_cast<int>(m_vec_Indices.size());
}

void PolygonRenderer::Render(const glm::mat4& mat4_MVP, GLuint textureID, const glm::vec2& vec2_TileScale) {
    if (m_i_VertexCount == 0) return;

    glUseProgram(m_ShaderProgram);

    // Set uniforms
    GLint mvpLoc = glGetUniformLocation(m_ShaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    GLint tileLoc = glGetUniformLocation(m_ShaderProgram, "uTileScale");
    glUniform2fv(tileLoc, 1, glm::value_ptr(vec2_TileScale));

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    GLint texLoc = glGetUniformLocation(m_ShaderProgram, "uTexture");
    glUniform1i(texLoc, 0);

    // Render
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_i_VertexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void PolygonRenderer::Clear() {
    m_vec_VertexData.clear();
    m_vec_Indices.clear();
    m_i_VertexCount = 0;
}

} // namespace Rendering
} // namespace ScotlandYard