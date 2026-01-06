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
    if (m_EBO_RiverStrip) {
        glDeleteBuffers(1, &m_EBO_RiverStrip);
        m_EBO_RiverStrip = 0;
    }
    if (m_VBO_RiverStrip) {
        glDeleteBuffers(1, &m_VBO_RiverStrip);
        m_VBO_RiverStrip = 0;
    }
    if (m_VAO_RiverStrip) {
        glDeleteVertexArrays(1, &m_VAO_RiverStrip);
        m_VAO_RiverStrip = 0;
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
        uniform float uVoronoiScale1;
        uniform float uVoronoiScale2;
        uniform float uRippleDensity;
        uniform int uIsCausticsLayer;
        uniform vec3 uWaterColor;
        uniform vec3 uFoamColor;
        uniform float uVoronoiPowerExponent;
        uniform float uVoronoiEdgeSmooth;
        uniform float uFoamThresholdMin;
        uniform float uFoamThresholdMax;
        out vec4 FragColor;

        // Hash functions , voronoi noise generator
        vec2 hash2(vec2 p) {
            p = vec2(dot(p, vec2(127.1, 311.7)),
                     dot(p, vec2(269.5, 183.3)));
            return fract(sin(p) * 43758.5453);
        }

        float hash(vec2 p) {
            return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
        }

        // musgrave displacement noise
        float valueNoise(vec2 p) {
            vec2 i = floor(p);
            vec2 f = fract(p);
            f = f * f * (3.0 - 2.0 * f); // Smoothstep

            float a = hash(i);
            float b = hash(i + vec2(1.0, 0.0));
            float c = hash(i + vec2(0.0, 1.0));
            float d = hash(i + vec2(1.0, 1.0));

            return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
        }

        // Musgrave (fBm)
        float musgrave(vec2 p, int octaves) {
            float value = 0.0;
            float amplitude = 0.5;
            float frequency = 1.0;

            for (int i = 0; i < octaves; i++) {
                value += amplitude * valueNoise(p * frequency);
                frequency *= 2.0;
                amplitude *= 0.5;
            }

            return value;
        }

        vec2 voronoi(vec2 uv, float powerExponent) {
            vec2 i = floor(uv);
            vec2 f = fract(uv);

            float f_MinDist1 = 8.0;  // F1 - closest point
            float f_MinDist2 = 8.0;  // F2 - second closest point

            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    vec2 neighbor = vec2(float(x), float(y));
                    vec2 point = hash2(i + neighbor);

                    //animate points
                    point = 0.5 + 0.3 * sin(uTime * 0.3 + 6.2831 * point);

                    vec2 diff = neighbor + point - f;
                    float f_Dist = length(diff);

                    f_Dist = pow(f_Dist, powerExponent);

                    // F1 and F2 edge extraction
                    if (f_Dist < f_MinDist1) {
                        f_MinDist2 = f_MinDist1;
                        f_MinDist1 = f_Dist;
                    } else if (f_Dist < f_MinDist2) {
                        f_MinDist2 = f_Dist;
                    }
                }
            }

            return vec2(f_MinDist1, f_MinDist2);
        }

        void main() {
            // Animation parameters
            float f_FlowSpeed = 0.15 * uRippleDensity;
            float f_WobbleSpeed1 = 0.4 * uRippleDensity;
            float f_WobbleSpeed2 = 0.6 * uRippleDensity;
            float f_WobbleAmount = 0.3;

            // Linear movement
            float f_FlowOffset = uTime * f_FlowSpeed;

            // wobble
            float f_MusgraveScale = 0.5;
            float f_MusgraveSpeed = 0.1;
            vec2 uv_MusgraveInput = vUV * f_MusgraveScale + uTime * f_MusgraveSpeed;
            float f_WobbleX = musgrave(uv_MusgraveInput, 3) * 2.0 - 1.0;
            float f_WobbleY = musgrave(uv_MusgraveInput + vec2(100.0, 50.0), 3) * 2.0 - 1.0;

            vec2 uv_Animated = vUV;
            uv_Animated.x += f_FlowOffset + f_WobbleX * f_WobbleAmount * 1.0;
            uv_Animated.y += f_WobbleY * f_WobbleAmount * 1.0;

            // Voronoi F1 and F2
            vec2 vec2_Voronoi1 = voronoi(uv_Animated * uVoronoiScale1, uVoronoiPowerExponent);
            vec2 vec2_Voronoi2 = voronoi(uv_Animated * uVoronoiScale2, uVoronoiPowerExponent);

            float f_Edges1 = vec2_Voronoi1.y - vec2_Voronoi1.x;
            float f_Edges2 = vec2_Voronoi2.y - vec2_Voronoi2.x;
            f_Edges1 = smoothstep(0.0, uVoronoiEdgeSmooth, f_Edges1);
            f_Edges2 = smoothstep(0.0, uVoronoiEdgeSmooth, f_Edges2);

            float f_Foam = f_Edges1 * 0.6 + f_Edges2 * 0.4;

            float f_FoamEdge = smoothstep(uFoamThresholdMin, uFoamThresholdMax, f_Foam);

            if (uIsCausticsLayer == 1) {
                //bottom caustics layer
                float f_ShadowIntensity = 1.0 - f_FoamEdge;
                vec3 vec3_ShadowColor = vec3(0.0, 0.0, 0.0);
                float f_ShadowAlpha = f_ShadowIntensity * 0.6;
                FragColor = vec4(vec3_ShadowColor, f_ShadowAlpha);
            } else {
                vec3 vec3_TopColor = mix(uFoamColor, uWaterColor, f_FoamEdge);
                float f_TopAlpha = 0.6;

                FragColor = vec4(vec3_TopColor, f_TopAlpha);
            }
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

void WaterRenderer::SetRiverStrip(const std::vector<glm::vec2>& vec_CenterlinePath, float f_Width) {
    if (vec_CenterlinePath.size() < 2) {
        m_i_RiverStripIndexCount = 0;
        return;
    }

    std::vector<float> vec_Vertices;
    std::vector<unsigned int> vec_Indices;

    float halfWidth = f_Width * 0.5f;
    float totalDistance = 0.0f;
    std::vector<float> distances;
    distances.push_back(0.0f);

    for (size_t i = 1; i < vec_CenterlinePath.size(); ++i) {
        float segmentLength = glm::length(vec_CenterlinePath[i] - vec_CenterlinePath[i - 1]);
        totalDistance += segmentLength;
        distances.push_back(totalDistance);
    }

    if (totalDistance < 0.0001f) {
        m_i_RiverStripIndexCount = 0;
        return;
    }

    for (size_t i = 0; i < vec_CenterlinePath.size(); ++i) {
        glm::vec2 tangent;
        if (i == 0) {
            glm::vec2 diff = vec_CenterlinePath[1] - vec_CenterlinePath[0];
            float length = glm::length(diff);
            if (length < 0.0001f) {
                m_i_RiverStripIndexCount = 0;
                return;
            }
            tangent = diff / length;
        } else if (i == vec_CenterlinePath.size() - 1) {
            glm::vec2 diff = vec_CenterlinePath[i] - vec_CenterlinePath[i - 1];
            float length = glm::length(diff);
            if (length < 0.0001f) {
                m_i_RiverStripIndexCount = 0;
                return;
            }
            tangent = diff / length;
        } else {
            glm::vec2 diff = vec_CenterlinePath[i + 1] - vec_CenterlinePath[i - 1];
            float length = glm::length(diff);
            if (length < 0.0001f) {
                m_i_RiverStripIndexCount = 0;
                return;
            }
            tangent = diff / length;
        }

        glm::vec2 normal(-tangent.y, tangent.x);
        glm::vec2 rightPos = vec_CenterlinePath[i] + normal * halfWidth;
        glm::vec2 leftPos = vec_CenterlinePath[i] - normal * halfWidth;

        vec_Vertices.push_back(rightPos.x);
        vec_Vertices.push_back(m_f_WaterHeight);
        vec_Vertices.push_back(rightPos.y);
        vec_Vertices.push_back(rightPos.x);
        vec_Vertices.push_back(rightPos.y);

        vec_Vertices.push_back(leftPos.x);
        vec_Vertices.push_back(m_f_WaterHeight);
        vec_Vertices.push_back(leftPos.y);
        vec_Vertices.push_back(leftPos.x);
        vec_Vertices.push_back(leftPos.y);
    }

    for (size_t i = 0; i < vec_CenterlinePath.size() - 1; ++i) {
        unsigned int rightCurrent = static_cast<unsigned int>(i * 2);
        unsigned int leftCurrent = rightCurrent + 1;
        unsigned int rightNext = static_cast<unsigned int>((i + 1) * 2);
        unsigned int leftNext = rightNext + 1;

        vec_Indices.push_back(rightCurrent);
        vec_Indices.push_back(leftCurrent);
        vec_Indices.push_back(rightNext);

        vec_Indices.push_back(rightNext);
        vec_Indices.push_back(leftCurrent);
        vec_Indices.push_back(leftNext);
    }

    if (!m_VAO_RiverStrip) {
        glGenVertexArrays(1, &m_VAO_RiverStrip);
        glGenBuffers(1, &m_VBO_RiverStrip);
        glGenBuffers(1, &m_EBO_RiverStrip);
    }

    glBindVertexArray(m_VAO_RiverStrip);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_RiverStrip);
    glBufferData(GL_ARRAY_BUFFER, vec_Vertices.size() * sizeof(float), vec_Vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_RiverStrip);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, vec_Indices.size() * sizeof(unsigned int), vec_Indices.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    m_i_RiverStripIndexCount = static_cast<int>(vec_Indices.size());
}

void WaterRenderer::Render(const glm::mat4& mat4_ViewProjection, float f_Time, const glm::mat4& mat4_GlobalScale) {
    if (!m_b_Initialized) return;
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "WaterRenderer: GL error before render: " << err << std::endl;
    }
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

    GLint mvpLoc = glGetUniformLocation(m_ShaderProgram, "uMVP");
    GLint timeLoc = glGetUniformLocation(m_ShaderProgram, "uTime");
    GLint heightLoc = glGetUniformLocation(m_ShaderProgram, "uWaterHeight");
    GLint voronoi1Loc = glGetUniformLocation(m_ShaderProgram, "uVoronoiScale1");
    GLint voronoi2Loc = glGetUniformLocation(m_ShaderProgram, "uVoronoiScale2");
    GLint rippleLoc = glGetUniformLocation(m_ShaderProgram, "uRippleDensity");
    GLint causticsLayerLoc = glGetUniformLocation(m_ShaderProgram, "uIsCausticsLayer");
    GLint waterColorLoc = glGetUniformLocation(m_ShaderProgram, "uWaterColor");
    GLint foamColorLoc = glGetUniformLocation(m_ShaderProgram, "uFoamColor");
    GLint powerExpLoc = glGetUniformLocation(m_ShaderProgram, "uVoronoiPowerExponent");
    GLint edgeSmoothLoc = glGetUniformLocation(m_ShaderProgram, "uVoronoiEdgeSmooth");
    GLint foamMinLoc = glGetUniformLocation(m_ShaderProgram, "uFoamThresholdMin");
    GLint foamMaxLoc = glGetUniformLocation(m_ShaderProgram, "uFoamThresholdMax");

    glUniform1f(timeLoc, f_Time);
    glUniform1f(heightLoc, m_f_WaterHeight);
    glUniform1f(voronoi1Loc, m_f_VoronoiScale1);
    glUniform1f(voronoi2Loc, m_f_VoronoiScale2);
    glUniform1f(rippleLoc, m_f_RippleDensity);
    glUniform3fv(waterColorLoc, 1, glm::value_ptr(m_vec3_WaterColor));
    glUniform3fv(foamColorLoc, 1, glm::value_ptr(m_vec3_FoamColor));
    glUniform1f(powerExpLoc, m_f_VoronoiPowerExponent);
    glUniform1f(edgeSmoothLoc, m_f_VoronoiEdgeSmooth);
    glUniform1f(foamMinLoc, m_f_FoamThresholdMin);
    glUniform1f(foamMaxLoc, m_f_FoamThresholdMax);

    if (m_VAO_Quad == 0) {
        std::cerr << "WaterRenderer: Invalid VAO" << std::endl;
        return;
    }

    // test water quad
    float f_QuadSize = 8.0f;
    float f_X = -10.0f;
    float f_Z = 8.0f;

    // Render caustics layer
    glm::mat4 mat4_CausticsModel = glm::translate(glm::mat4(1.0f), glm::vec3(f_X, m_f_WaterHeight - m_f_CausticsDepth, f_Z));
    mat4_CausticsModel = glm::scale(mat4_CausticsModel, glm::vec3(f_QuadSize, 1.0f, f_QuadSize));
    mat4_CausticsModel = mat4_GlobalScale * mat4_CausticsModel;
    glm::mat4 mat4_CausticsMVP = mat4_ViewProjection * mat4_CausticsModel;

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_CausticsMVP));
    glUniform1i(causticsLayerLoc, 1);

    glBindVertexArray(m_VAO_Quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Render top surface
    glm::mat4 mat4_Model = glm::translate(glm::mat4(1.0f), glm::vec3(f_X, m_f_WaterHeight, f_Z));
    mat4_Model = glm::scale(mat4_Model, glm::vec3(f_QuadSize, 1.0f, f_QuadSize));
    mat4_Model = mat4_GlobalScale * mat4_Model;
    glm::mat4 mat4_MVP = mat4_ViewProjection * mat4_Model;

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
    glUniform1i(causticsLayerLoc, 0);

    glBindVertexArray(m_VAO_Quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

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

    GLint mvpLoc = glGetUniformLocation(m_ShaderProgram, "uMVP");
    GLint timeLoc = glGetUniformLocation(m_ShaderProgram, "uTime");
    GLint heightLoc = glGetUniformLocation(m_ShaderProgram, "uWaterHeight");
    GLint voronoi1Loc = glGetUniformLocation(m_ShaderProgram, "uVoronoiScale1");
    GLint voronoi2Loc = glGetUniformLocation(m_ShaderProgram, "uVoronoiScale2");
    GLint rippleLoc = glGetUniformLocation(m_ShaderProgram, "uRippleDensity");
    GLint causticsLayerLoc = glGetUniformLocation(m_ShaderProgram, "uIsCausticsLayer");
    GLint waterColorLoc = glGetUniformLocation(m_ShaderProgram, "uWaterColor");
    GLint foamColorLoc = glGetUniformLocation(m_ShaderProgram, "uFoamColor");
    GLint powerExpLoc = glGetUniformLocation(m_ShaderProgram, "uVoronoiPowerExponent");
    GLint edgeSmoothLoc = glGetUniformLocation(m_ShaderProgram, "uVoronoiEdgeSmooth");
    GLint foamMinLoc = glGetUniformLocation(m_ShaderProgram, "uFoamThresholdMin");
    GLint foamMaxLoc = glGetUniformLocation(m_ShaderProgram, "uFoamThresholdMax");

    glUniform1f(timeLoc, f_Time);
    glUniform1f(heightLoc, m_f_WaterHeight);
    glUniform1f(voronoi1Loc, m_f_VoronoiScale1);
    glUniform1f(voronoi2Loc, m_f_VoronoiScale2);
    glUniform1f(rippleLoc, m_f_RippleDensity);
    glUniform3fv(waterColorLoc, 1, glm::value_ptr(m_vec3_WaterColor));
    glUniform3fv(foamColorLoc, 1, glm::value_ptr(m_vec3_FoamColor));
    glUniform1f(powerExpLoc, m_f_VoronoiPowerExponent);
    glUniform1f(edgeSmoothLoc, m_f_VoronoiEdgeSmooth);
    glUniform1f(foamMinLoc, m_f_FoamThresholdMin);
    glUniform1f(foamMaxLoc, m_f_FoamThresholdMax);

    // Render polygon (top layer only for now)
    glm::mat4 mat4_Model = mat4_GlobalScale;
    glm::mat4 mat4_MVP = mat4_ViewProjection * mat4_Model;

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
    glUniform1i(causticsLayerLoc, 0);

    glBindVertexArray(m_VAO_Polygon);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vec_BoundaryPoints.size() * 3));
    glBindVertexArray(0);

    glDepthMask(b_DepthMaskWas);
    if (b_CullWas) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (b_BlendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFunc(i_BlendSrc, i_BlendDst);
}

void WaterRenderer::RenderRiverStrip(const glm::mat4& mat4_ViewProjection,
                                      float f_Time,
                                      const glm::mat4& mat4_GlobalScale) {
    if (!m_b_Initialized || m_i_RiverStripIndexCount == 0) return;

    GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
    GLboolean b_CullWas = glIsEnabled(GL_CULL_FACE);
    GLboolean b_DepthMaskWas;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &b_DepthMaskWas);
    GLint i_BlendSrc, i_BlendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &i_BlendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &i_BlendDst);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glUseProgram(m_ShaderProgram);

    GLint mvpLoc = glGetUniformLocation(m_ShaderProgram, "uMVP");
    GLint timeLoc = glGetUniformLocation(m_ShaderProgram, "uTime");
    GLint heightLoc = glGetUniformLocation(m_ShaderProgram, "uWaterHeight");
    GLint voronoi1Loc = glGetUniformLocation(m_ShaderProgram, "uVoronoiScale1");
    GLint voronoi2Loc = glGetUniformLocation(m_ShaderProgram, "uVoronoiScale2");
    GLint rippleLoc = glGetUniformLocation(m_ShaderProgram, "uRippleDensity");
    GLint causticsLayerLoc = glGetUniformLocation(m_ShaderProgram, "uIsCausticsLayer");
    GLint waterColorLoc = glGetUniformLocation(m_ShaderProgram, "uWaterColor");
    GLint foamColorLoc = glGetUniformLocation(m_ShaderProgram, "uFoamColor");
    GLint powerExpLoc = glGetUniformLocation(m_ShaderProgram, "uVoronoiPowerExponent");
    GLint edgeSmoothLoc = glGetUniformLocation(m_ShaderProgram, "uVoronoiEdgeSmooth");
    GLint foamMinLoc = glGetUniformLocation(m_ShaderProgram, "uFoamThresholdMin");
    GLint foamMaxLoc = glGetUniformLocation(m_ShaderProgram, "uFoamThresholdMax");

    glUniform1f(timeLoc, f_Time);
    glUniform1f(heightLoc, m_f_WaterHeight);
    glUniform1f(voronoi1Loc, m_f_VoronoiScale1);
    glUniform1f(voronoi2Loc, m_f_VoronoiScale2);
    glUniform1f(rippleLoc, m_f_RippleDensity);
    glUniform3fv(waterColorLoc, 1, glm::value_ptr(m_vec3_WaterColor));
    glUniform3fv(foamColorLoc, 1, glm::value_ptr(m_vec3_FoamColor));
    glUniform1f(powerExpLoc, m_f_VoronoiPowerExponent);
    glUniform1f(edgeSmoothLoc, m_f_VoronoiEdgeSmooth);
    glUniform1f(foamMinLoc, m_f_FoamThresholdMin);
    glUniform1f(foamMaxLoc, m_f_FoamThresholdMax);

    glUniform1i(causticsLayerLoc, 1);
    glm::mat4 mat4_CausticsModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -m_f_CausticsDepth, 0.0f));
    mat4_CausticsModel = mat4_GlobalScale * mat4_CausticsModel;
    glm::mat4 mat4_CausticsMVP = mat4_ViewProjection * mat4_CausticsModel;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_CausticsMVP));

    glBindVertexArray(m_VAO_RiverStrip);
    glDrawElements(GL_TRIANGLES, m_i_RiverStripIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glUniform1i(causticsLayerLoc, 0);
    glm::mat4 mat4_MVP = mat4_ViewProjection * mat4_GlobalScale;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    glBindVertexArray(m_VAO_RiverStrip);
    glDrawElements(GL_TRIANGLES, m_i_RiverStripIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDepthMask(b_DepthMaskWas);
    if (b_CullWas) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (b_BlendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFunc(i_BlendSrc, i_BlendDst);
}

void WaterRenderer::SetWaterHeight(float f_Height) {
    m_f_WaterHeight = f_Height;
}

void WaterRenderer::SetVoronoiScale(float f_Scale1, float f_Scale2) {
    m_f_VoronoiScale1 = f_Scale1;
    m_f_VoronoiScale2 = f_Scale2;
}

void WaterRenderer::SetRippleDensity(float f_Density) {
    m_f_RippleDensity = f_Density;
}

void WaterRenderer::SetCausticsDepth(float f_Depth) {
    m_f_CausticsDepth = f_Depth;
}

void WaterRenderer::SetWaterColor(const glm::vec3& vec3_Color) {
    m_vec3_WaterColor = vec3_Color;
}

void WaterRenderer::SetFoamColor(const glm::vec3& vec3_Color) {
    m_vec3_FoamColor = vec3_Color;
}

void WaterRenderer::SetVoronoiSmoothness(float f_PowerExponent, float f_EdgeSmooth, float f_FoamThresholdMin, float f_FoamThresholdMax) {
    m_f_VoronoiPowerExponent = f_PowerExponent;
    m_f_VoronoiEdgeSmooth = f_EdgeSmooth;
    m_f_FoamThresholdMin = f_FoamThresholdMin;
    m_f_FoamThresholdMax = f_FoamThresholdMax;
}

} // namespace Rendering
} // namespace ScotlandYard
