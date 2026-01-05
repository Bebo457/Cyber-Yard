#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

namespace ScotlandYard {
namespace Rendering {

class PolygonRenderer {
public:
    PolygonRenderer();
    ~PolygonRenderer();
    bool Initialize();

    void SetPolygon(const std::vector<glm::vec2>& vec_Vertices, float f_YHeight = 0.0f);
    void SetRiverStrip(const std::vector<glm::vec2>& vec_CenterlinePath, float f_Width, float f_YHeight = 0.0f);
    void Render(const glm::mat4& mat4_MVP, GLuint textureID, const glm::vec2& vec2_TileScale = glm::vec2(1.0f));

    void Clear();

private:
    void CreateShaders();
    void TriangulatePolygon(const std::vector<glm::vec2>& vec_Vertices);

    GLuint m_ShaderProgram = 0;
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
    GLuint m_EBO = 0;

    std::vector<float> m_vec_VertexData;  // Interleaved: pos(3) + normal(3) + uv(2)
    std::vector<unsigned int> m_vec_Indices;

    float m_f_YHeight = 0.0f;
    int m_i_VertexCount = 0;
};

} // namespace Rendering
} // namespace ScotlandYard