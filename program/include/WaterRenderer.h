#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

namespace ScotlandYard {
namespace Rendering {

class WaterRenderer {
public:
    WaterRenderer();
    ~WaterRenderer();

    void Initialize();
    void Cleanup();

    void Render(const glm::mat4& mat4_ViewProjection, float f_Time, const glm::mat4& mat4_GlobalScale);
    void RenderPolygon(const std::vector<glm::vec2>& vec_BoundaryPoints,
                       const glm::mat4& mat4_ViewProjection,
                       float f_Time,
                       const glm::mat4& mat4_GlobalScale);

    void SetWaterHeight(float f_Height);

private:
    void CreateShaders();
    void CreateQuadMesh();
    void UpdatePolygonMesh(const std::vector<glm::vec2>& vec_BoundaryPoints);

    GLuint m_ShaderProgram = 0;
    GLuint m_VAO_Quad = 0;
    GLuint m_VBO_Quad = 0;
    GLuint m_VAO_Polygon = 0;
    GLuint m_VBO_Polygon = 0;

    float m_f_WaterHeight = 0.1f;
    bool m_b_Initialized = false;
};

} // namespace Rendering
} // namespace ScotlandYard
