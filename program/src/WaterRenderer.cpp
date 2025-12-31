#include "WaterRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>

namespace ScotlandYard {
namespace Rendering {

WaterRenderer::WaterRenderer() {}

WaterRenderer::~WaterRenderer() {
    Cleanup();
}

void WaterRenderer::Initialize() {
    if (m_b_Initialized) return;

    CreateShaders();
    if (m_ShaderProgram == 0) {
        std::cerr << "WaterRenderer: Shader creation failed" << std::endl;
        return;
    }

    CreateQuadMesh();

    if (m_VAO_Quad == 0 || m_VBO_Quad == 0) {
        std::cerr << "WaterRenderer: Mesh creation failed" << std::endl;
        return;
    }

    m_b_Initialized = true;
}

void WaterRenderer::Cleanup() {
    if (m_VBO_Quad) {
        glDeleteBuffers(1, &m_VBO_Quad);
        m_VBO_Quad = 0;
    }
    if (m_VAO_Quad) {
        glDeleteVertexArrays(1, &m_VAO_Quad);
        m_VAO_Quad = 0;
    }
    if (m_VBO_Polygon) {
        glDeleteBuffers(1, &m_VBO_Polygon);
        m_VBO_Polygon = 0;
    }
    if (m_VAO_Polygon) {
        glDeleteVertexArrays(1, &m_VAO_Polygon);
        m_VAO_Polygon = 0;
    }
    if (m_ShaderProgram) {
        glDeleteProgram(m_ShaderProgram);
        m_ShaderProgram = 0;
    }

    m_b_Initialized = false;
}

void WaterRenderer::CreateShaders() {
    const char* vs_Src = R"(#version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec2 aUV;

        uniform mat4 uMVP;
        out vec2 vUV;
        out vec3 vWorldPos;

        void main() {
            vUV = aUV;
            vWorldPos = aPos;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";

    const char* fs_Src = R"(#version 330 core
        in vec2 vUV;
        in vec3 vWorldPos;

        uniform float uTime;
        uniform float uWaterHeight;
        out vec4 FragColor;

        // Voronoi noise helper
        vec2 hash2(vec2 p) {
            p = vec2(dot(p, vec2(127.1, 311.7)),
                     dot(p, vec2(269.5, 183.3)));
            return fract(sin(p) * 43758.5453);
        }

        float voronoi(vec2 uv) {
            vec2 i = floor(uv);
            vec2 f = fract(uv);

            float f_MinDist = 1.0;

            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    vec2 neighbor = vec2(float(x), float(y));
                    vec2 point = hash2(i + neighbor);

                    // Animate the points slightly
                    point = 0.5 + 0.5 * sin(uTime * 0.5 + 6.2831 * point);

                    vec2 diff = neighbor + point - f;
                    float f_Dist = length(diff);
                    f_MinDist = min(f_MinDist, f_Dist);
                }
            }

            return f_MinDist;
        }

        void main() {
            // Animation parameters
            float f_FlowSpeed = 0.15;
            float f_WobbleSpeed1 = 0.4;
            float f_WobbleSpeed2 = 0.6;
            float f_WobbleAmount = 0.3;

            // Linear movement along X axis
            float f_FlowOffset = uTime * f_FlowSpeed;

            // Wobble perpendicular to flow (along Y axis) using two cyclic functions
            float f_WobbleOffset = sin(uTime * f_WobbleSpeed1) * f_WobbleAmount +
                                   cos(uTime * f_WobbleSpeed2) * f_WobbleAmount * 0.5;

            // Animated UV coordinates
            vec2 uv_Animated = vUV;
            uv_Animated.x += f_FlowOffset;
            uv_Animated.y += f_WobbleOffset;

            // Generate Voronoi pattern at two scales for detail
            float f_VoronoiScale1 = 8.0;
            float f_VoronoiScale2 = 16.0;

            float f_Voronoi1 = voronoi(uv_Animated * f_VoronoiScale1);
            float f_Voronoi2 = voronoi(uv_Animated * f_VoronoiScale2);

            // Combine voronoi layers for foam pattern
            float f_Foam = f_Voronoi1 * 0.7 + f_Voronoi2 * 0.3;

            // Create foam edges (white where voronoi is low)
            float f_FoamEdge = smoothstep(0.1, 0.3, f_Foam);

            // Top water layer - transparent blue-green with white foam
            vec3 vec3_WaterColor = vec3(0.1, 0.4, 0.5);
            vec3 vec3_FoamColor = vec3(1.0, 1.0, 1.0);

            vec3 vec3_TopColor = mix(vec3_FoamColor, vec3_WaterColor, f_FoamEdge);
            float f_TopAlpha = 0.6;

            // Calculate Y distance from water surface for caustics
            float f_DistFromSurface = abs(vWorldPos.y - uWaterHeight);
            float f_CausticsStrength = 1.0 - clamp(f_DistFromSurface / 0.5, 0.0, 1.0);

            // Bottom caustics layer - darker version with higher contrast
            float f_CausticPattern = pow(1.0 - f_Foam, 2.0) * f_CausticsStrength;
            vec3 vec3_CausticColor = vec3(0.05, 0.15, 0.2) + vec3_TopColor * f_CausticPattern * 0.3;

            // Blend top and caustics based on view angle
            vec3 vec3_FinalColor = vec3_TopColor * 0.7 + vec3_CausticColor * 0.3;

            FragColor = vec4(vec3_FinalColor, f_TopAlpha);
        }
    )";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_Src, nullptr);
    glCompileShader(vs);

    GLint i_Success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char c_InfoLog[512];
        glGetShaderInfoLog(vs, 512, nullptr, c_InfoLog);
        std::cerr << "WaterRenderer: Vertex shader compilation failed: " << c_InfoLog << std::endl;
        glDeleteShader(vs);
        m_ShaderProgram = 0;
        return;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_Src, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char c_InfoLog[512];
        glGetShaderInfoLog(fs, 512, nullptr, c_InfoLog);
        std::cerr << "WaterRenderer: Fragment shader compilation failed: " << c_InfoLog << std::endl;
        glDeleteShader(vs);
        glDeleteShader(fs);
        m_ShaderProgram = 0;
        return;
    }

    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vs);
    glAttachShader(m_ShaderProgram, fs);
    glLinkProgram(m_ShaderProgram);

    glGetProgramiv(m_ShaderProgram, GL_LINK_STATUS, &i_Success);
    if (!i_Success) {
        char c_InfoLog[512];
        glGetProgramInfoLog(m_ShaderProgram, 512, nullptr, c_InfoLog);
        std::cerr << "WaterRenderer: Shader program linking failed: " << c_InfoLog << std::endl;
        glDeleteProgram(m_ShaderProgram);
        m_ShaderProgram = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

void WaterRenderer::CreateQuadMesh() {
    float f_QuadVertices[] = {
        // pos (x, y, z)     // uv
        -0.5f, 0.0f, -0.5f,   0.0f, 0.0f,
         0.5f, 0.0f, -0.5f,   1.0f, 0.0f,
         0.5f, 0.0f,  0.5f,   1.0f, 1.0f,
        -0.5f, 0.0f, -0.5f,   0.0f, 0.0f,
         0.5f, 0.0f,  0.5f,   1.0f, 1.0f,
        -0.5f, 0.0f,  0.5f,   0.0f, 1.0f
    };

    glGenVertexArrays(1, &m_VAO_Quad);
    glGenBuffers(1, &m_VBO_Quad);

    glBindVertexArray(m_VAO_Quad);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(f_QuadVertices), f_QuadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void WaterRenderer::UpdatePolygonMesh(const std::vector<glm::vec2>& vec_BoundaryPoints) {
    if (vec_BoundaryPoints.size() < 3) return;

    std::vector<float> vec_Vertices;

    glm::vec2 vec2_Center(0.0f, 0.0f);
    for (const auto& pt : vec_BoundaryPoints) {
        vec2_Center += pt;
    }
    vec2_Center /= static_cast<float>(vec_BoundaryPoints.size());

    for (size_t i = 0; i < vec_BoundaryPoints.size(); ++i) {
        const glm::vec2& pt1 = vec_BoundaryPoints[i];
        const glm::vec2& pt2 = vec_BoundaryPoints[(i + 1) % vec_BoundaryPoints.size()];

        // Triangle: center, pt1, pt2
        vec_Vertices.push_back(vec2_Center.x);
        vec_Vertices.push_back(m_f_WaterHeight);
        vec_Vertices.push_back(vec2_Center.y);
        vec_Vertices.push_back(0.5f);
        vec_Vertices.push_back(0.5f);

        vec_Vertices.push_back(pt1.x);
        vec_Vertices.push_back(m_f_WaterHeight);
        vec_Vertices.push_back(pt1.y);
        vec_Vertices.push_back(0.0f);
        vec_Vertices.push_back(0.0f);

        vec_Vertices.push_back(pt2.x);
        vec_Vertices.push_back(m_f_WaterHeight);
        vec_Vertices.push_back(pt2.y);
        vec_Vertices.push_back(1.0f);
        vec_Vertices.push_back(1.0f);
    }

    if (!m_VAO_Polygon) {
        glGenVertexArrays(1, &m_VAO_Polygon);
        glGenBuffers(1, &m_VBO_Polygon);
    }

    glBindVertexArray(m_VAO_Polygon);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Polygon);
    glBufferData(GL_ARRAY_BUFFER, vec_Vertices.size() * sizeof(float), vec_Vertices.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void WaterRenderer::Render(const glm::mat4& mat4_ViewProjection, float f_Time, const glm::mat4& mat4_GlobalScale) {
    if (!m_b_Initialized) return;
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "WaterRenderer: GL error before render: " << err << std::endl;
    }

    // perserve OpenGL state
    GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
    GLboolean b_CullWas = glIsEnabled(GL_CULL_FACE);
    GLboolean b_DepthMaskWas;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &b_DepthMaskWas);
    GLint i_BlendSrc, i_BlendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &i_BlendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &i_BlendDst);

    // water rendering state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glUseProgram(m_ShaderProgram);

    glm::mat4 mat4_Model = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, m_f_WaterHeight, 8.0f));
    mat4_Model = glm::scale(mat4_Model, glm::vec3(8.0f, 1.0f, 8.0f));
    mat4_Model = mat4_GlobalScale * mat4_Model;
    glm::mat4 mat4_MVP = mat4_ViewProjection * mat4_Model;

    GLint mvpLoc = glGetUniformLocation(m_ShaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    GLint timeLoc = glGetUniformLocation(m_ShaderProgram, "uTime");
    glUniform1f(timeLoc, f_Time);

    GLint heightLoc = glGetUniformLocation(m_ShaderProgram, "uWaterHeight");
    glUniform1f(heightLoc, m_f_WaterHeight);

    if (m_VAO_Quad == 0) {
        std::cerr << "WaterRenderer: Invalid VAO" << std::endl;
        return;
    }

    glBindVertexArray(m_VAO_Quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    //restore OpenGL state
    glDepthMask(b_DepthMaskWas);
    if (b_CullWas) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (b_BlendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFunc(i_BlendSrc, i_BlendDst);
}

//in function
void WaterRenderer::RenderPolygon(const std::vector<glm::vec2>& vec_BoundaryPoints, const glm::mat4& mat4_ViewProjection, float f_Time, const glm::mat4& mat4_GlobalScale) {
    if (!m_b_Initialized || vec_BoundaryPoints.size() < 3) return;

    UpdatePolygonMesh(vec_BoundaryPoints);

    GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
    GLboolean b_CullWas = glIsEnabled(GL_CULL_FACE);
    GLboolean b_DepthMaskWas;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &b_DepthMaskWas);
    GLint i_BlendSrc, i_BlendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &i_BlendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &i_BlendDst);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glUseProgram(m_ShaderProgram);

    glm::mat4 mat4_Model = mat4_GlobalScale;
    glm::mat4 mat4_MVP = mat4_ViewProjection * mat4_Model;

    GLint mvpLoc = glGetUniformLocation(m_ShaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    GLint timeLoc = glGetUniformLocation(m_ShaderProgram, "uTime");
    glUniform1f(timeLoc, f_Time);

    GLint heightLoc = glGetUniformLocation(m_ShaderProgram, "uWaterHeight");
    glUniform1f(heightLoc, m_f_WaterHeight);

    glBindVertexArray(m_VAO_Polygon);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vec_BoundaryPoints.size() * 3));
    glBindVertexArray(0);

    //restore OpenGL state
    glDepthMask(b_DepthMaskWas);
    if (b_CullWas) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (b_BlendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFunc(i_BlendSrc, i_BlendDst);
}

void WaterRenderer::SetWaterHeight(float f_Height) {
    m_f_WaterHeight = f_Height;
}

} // namespace Rendering
} // namespace ScotlandYard
