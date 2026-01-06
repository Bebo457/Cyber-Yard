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
    void SetRiverStrip(const std::vector<glm::vec2>& vec_CenterlinePath, float f_Width);
    void RenderRiverStrip(const glm::mat4& mat4_ViewProjection, float f_Time, const glm::mat4& mat4_GlobalScale);

    void SetWaterHeight(float f_Height);
    void SetVoronoiScale(float f_Scale1, float f_Scale2);
    void SetRippleDensity(float f_Density);
    void SetCausticsDepth(float f_Depth);
    void SetWaterColor(const glm::vec3& vec3_Color);
    void SetFoamColor(const glm::vec3& vec3_Color);
    void SetVoronoiSmoothness(float f_PowerExponent, float f_EdgeSmooth, float f_FoamThresholdMin, float f_FoamThresholdMax);

private:
    void CreateShaders();
    void CreateQuadMesh();
    void UpdatePolygonMesh(const std::vector<glm::vec2>& vec_BoundaryPoints);

    GLuint m_ShaderProgram = 0;
    GLuint m_VAO_Quad = 0;
    GLuint m_VBO_Quad = 0;
    GLuint m_VAO_Polygon = 0;
    GLuint m_VBO_Polygon = 0;
    GLuint m_VAO_RiverStrip = 0;
    GLuint m_VBO_RiverStrip = 0;
    GLuint m_EBO_RiverStrip = 0;
    int m_i_RiverStripIndexCount = 0;

    float m_f_WaterHeight = 0.1f;
    float m_f_VoronoiScale1 = 8.0f;
    float m_f_VoronoiScale2 = 12.0f;
    float m_f_RippleDensity = 1.9f;
    float m_f_CausticsDepth = 0.5f;
    glm::vec3 m_vec3_WaterColor = glm::vec3(0.1f, 0.3f, 0.5f);
    glm::vec3 m_vec3_FoamColor = glm::vec3(1.0f, 1.0f, 1.0f);

    float m_f_VoronoiPowerExponent = 0.5f;    // lower = rounder
    float m_f_VoronoiEdgeSmooth = 0.4f;       //edge smoothstep range
    float m_f_FoamThresholdMin = 0.1f;        // foam threshold min
    float m_f_FoamThresholdMax = 0.2f;        // foam threshold max

    bool m_b_Initialized = false;
};

} // namespace Rendering
} // namespace ScotlandYard
