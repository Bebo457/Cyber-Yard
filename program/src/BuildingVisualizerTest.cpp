#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <array>
#include <limits>

#include "BuildingGenerator.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

using namespace ScotlandYard::Core;

// ===== SHADER SOURCE CODE =====
const char* k_VertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalize(mat3(transpose(inverse(model))) * aNormal);
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* k_FragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform vec3 objColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform bool useTexture;
uniform bool isRoof;
uniform float roofUVScale;
uniform float textureExposure;

vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

vec3 linearToSrgb(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
}

void main()
{
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
  
    // Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;  
    
    vec2 sampleUV = TexCoord;
    if (isRoof) {
        sampleUV *= roofUVScale;
    }
    vec4 texSample = texture(texture1, sampleUV);
    vec3 texLinear = srgbToLinear(texSample.rgb) * textureExposure;
    vec3 baseColor = useTexture ? texLinear : objColor;
    // Optionally tint roofs slightly warmer if needed
    float alpha = useTexture ? texSample.a : 1.0;
    // Drop nearly transparent fragments to avoid darkening from premultiplied backgrounds
    if (alpha < 0.05)
        discard;
    vec3 resultLinear = (ambient + diffuse + specular) * baseColor;
    vec3 result = useTexture ? linearToSrgb(resultLinear) : resultLinear;
    FragColor = vec4(result, alpha);
}
)";

// Simple UI shaders (orthographic quads)
const char* k_UIVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 uScreenSize;
void main() {
    vec2 ndc = vec2((aPos.x / uScreenSize.x) * 2.0 - 1.0, 1.0 - (aPos.y / uScreenSize.y) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

const char* k_UIFragmentShader = R"(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)";

// ===== GLOBALS =====
SDL_Window* g_p_Window = nullptr;
SDL_GLContext g_GLContext = nullptr;
bool g_b_ShouldClose = false;

// Window size (used by simple UI overlay)
int g_i_WindowWidth = 1280;
int g_i_WindowHeight = 720;

GLuint g_ShaderProgram = 0;
GLuint g_VAO = 0;
GLuint g_VBO = 0;
GLuint g_VBO_Normal = 0;
GLuint g_VBO_TexCoord = 0;
GLuint g_EBO = 0;
GLuint g_TextureID = 0;
GLuint g_TextureID_Roof = 0;

struct FacadeTextureOption {
    std::string name;
    std::string path;
    GLuint textureId = 0;
};

std::vector<FacadeTextureOption> g_FacadeTextures;
int g_i_FacadeIndex = 0;

std::vector<FacadeTextureOption> g_WindowTextures;
int g_i_WindowIndex = 0;

std::vector<FacadeTextureOption> g_DoorTextures;
int g_i_DoorIndex = 0;

// Ivy overlay
GLuint g_VAO_Ivy = 0;
GLuint g_VBO_Ivy = 0;
GLuint g_VBO_Ivy_Normal = 0;
GLuint g_VBO_Ivy_TexCoord = 0;
GLuint g_EBO_Ivy = 0;
GLuint g_TextureID_Ivy = 0;
glm::vec3 g_IvyWallCenter(0.0f);
glm::vec3 g_IvyWallNormal(0.0f, 1.0f, 0.0f);
bool g_b_HasIvy = false;
bool g_b_ShowIvy = true;

// Windows
std::vector<GLuint> g_VAO_Windows;
std::vector<GLuint> g_VBO_Windows;
std::vector<GLuint> g_VBO_Windows_Normal;
std::vector<GLuint> g_VBO_Windows_TexCoord;
std::vector<GLuint> g_EBO_Windows;
GLuint g_TextureID_Windows = 0;
GLuint g_TextureID_WindowSide = 0;
bool g_b_UseSideWindowTexture = false;

// Doors
std::vector<GLuint> g_VAO_Doors;
std::vector<GLuint> g_VBO_Doors;
std::vector<GLuint> g_VBO_Doors_Normal;
std::vector<GLuint> g_VBO_Doors_TexCoord;
std::vector<GLuint> g_EBO_Doors;
GLuint g_TextureID_Doors = 0;

BuildingMesh g_BuildingMesh;

float g_f_RotationX = -30.0f;
float g_f_RotationY = 45.0f;
float g_f_ZoomDistance = 30.0f;

bool g_b_UseFlatRoof = false;
int g_i_BuildingExample = 0; 

// Building parameters (editable)
float g_f_BuildingHeight = 10.0f;
float g_f_RoofHeight = 3.0f;
float g_f_BaseWidth = 10.0f;
float g_f_BaseDepth = 5.0f;

// Forward declarations for regeneration utilities
void GenerateBuilding();
void UploadMeshToGPU();
void UploadWindowsToGPU();
void UploadDoorsToGPU();
void UploadIvyToGPU();
void RegenerateAndUpload();
void LoadFacadeTextures();
void CycleFacadeTexture();
std::string GetCurrentFacadeLabel();
void LoadWindowTextures();
void LoadDoorTextures();
void CycleWindowTexture();
void CycleDoorTexture();
std::string GetCurrentWindowLabel();
std::string GetCurrentDoorLabel();

// ===== SIMPLE UI STATE =====
GLuint g_UIShaderProgram = 0;
GLuint g_UIVAO = 0;
GLuint g_UIVBO = 0;

struct UIButton {
    SDL_Rect rectPx{};
    std::string label;
    std::function<void()> onClick;
};

std::vector<UIButton> g_Buttons;

struct Glyph { unsigned char rows[7]; };
std::unordered_map<char, Glyph> g_Glyphs;

bool g_b_MousePressed = false;
int g_i_LastMouseX = 0;
int g_i_LastMouseY = 0;

// ===== SHADER COMPILATION =====
GLuint CompileShader(const char* p_Source, GLenum shaderType) {
    GLuint ui_Shader = glCreateShader(shaderType);
    glShaderSource(ui_Shader, 1, &p_Source, nullptr);
    glCompileShader(ui_Shader);

    int i_Success;
    char infoLog[512];
    glGetShaderiv(ui_Shader, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        glGetShaderInfoLog(ui_Shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }
    return ui_Shader;
}

GLuint CreateUIShader() {
    GLuint vs = CompileShader(k_UIVertexShader, GL_VERTEX_SHADER);
    GLuint fs = CompileShader(k_UIFragmentShader, GL_FRAGMENT_SHADER);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "UI shader link failed: " << infoLog << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

void InitGlyphs() {
    auto set = [](char c, std::initializer_list<unsigned char> rows) {
        Glyph g{}; size_t i = 0; for (auto r : rows) { g.rows[i++] = r; } g_Glyphs[c] = g; };
    // Digits 0-9
    set('0',{0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110});
    set('1',{0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110});
    set('2',{0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111});
    set('3',{0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110});
    set('4',{0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010});
    set('5',{0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110});
    set('6',{0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110});
    set('7',{0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000});
    set('8',{0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110});
    set('9',{0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100});
    // Letters we display
    set('A',{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001});
    set('B',{0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110});
    set('C',{0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110});
    set('D',{0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110});
    set('E',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111});
    set('F',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000});
    set('G',{0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110});
    set('H',{0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001});
    set('K',{0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001});
    set('I',{0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110});
    set('J',{0b00011,0b00001,0b00001,0b00001,0b10001,0b10001,0b01110});
    set('L',{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111});
    set('O',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110});
    set('P',{0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000});
    set('R',{0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001});
    set('S',{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110});
    set('T',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100});
    set('U',{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110});
    set('V',{0b10001,0b10001,0b10001,0b10001,0b01010,0b01010,0b00100});
    set('W',{0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010});
    set('Z',{0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111});
    set('Y',{0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100});
    set('D',{0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110});
    set('M',{0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001});
    set('N',{0b10001,0b10001,0b11001,0b10101,0b10011,0b10001,0b10001});
    // Symbols
    set('+',{0b00000,0b00100,0b00100,0b11111,0b00100,0b00100,0b00000});
    set('-',{0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000});
    set('.',{0b00000,0b00000,0b00000,0b00000,0b00000,0b01100,0b01100});
    set(' ',{0,0,0,0,0,0,0});
    set(':',{0b00000,0b01100,0b01100,0b00000,0b01100,0b01100,0b00000});
}

void EnsureUIBuffers() {
    if (!g_UIVAO) {
        glGenVertexArrays(1, &g_UIVAO);
        glGenBuffers(1, &g_UIVBO);
        glBindVertexArray(g_UIVAO);
        glBindBuffer(GL_ARRAY_BUFFER, g_UIVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
}

void DrawQuadPx(float x0, float y0, float x1, float y1, const glm::vec4& color) {
    EnsureUIBuffers();
    float verts[12] = {
        x0, y0,
        x1, y0,
        x0, y1,
        x1, y0,
        x1, y1,
        x0, y1
    };
    glUseProgram(g_UIShaderProgram);
    glUniform4f(glGetUniformLocation(g_UIShaderProgram, "uColor"), color.r, color.g, color.b, color.a);
    glUniform2f(glGetUniformLocation(g_UIShaderProgram, "uScreenSize"), (float)g_i_WindowWidth, (float)g_i_WindowHeight);
    glBindVertexArray(g_UIVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_UIVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
}

void DrawTextPx(const std::string& text, float x, float y, float scale, const glm::vec4& color) {
    EnsureUIBuffers();
    float cursor = x;
    glUseProgram(g_UIShaderProgram);
    glUniform4f(glGetUniformLocation(g_UIShaderProgram, "uColor"), color.r, color.g, color.b, color.a);
    glUniform2f(glGetUniformLocation(g_UIShaderProgram, "uScreenSize"), (float)g_i_WindowWidth, (float)g_i_WindowHeight);
    glBindVertexArray(g_UIVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_UIVBO);
    for (char c : text) {
        auto it = g_Glyphs.find((char)std::toupper(static_cast<unsigned char>(c)));
        if (it == g_Glyphs.end()) {
            cursor += 4.0f * scale;
            continue;
        }
        const Glyph& g = it->second;
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (g.rows[row] & (1 << (4 - col))) {
                    float px = cursor + col * scale;
                    float py = y + row * scale;
                    float verts[12] = {
                        px, py,
                        px + scale, py,
                        px, py + scale,
                        px + scale, py,
                        px + scale, py + scale,
                        px, py + scale
                    };
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
            }
        }
        cursor += 6.0f * scale; 
    }
    glBindVertexArray(0);
    glUseProgram(0);
}

float MeasureTextPx(const std::string& text, float scale) {
    float w = 0.0f;
    for (char c : text) {
        auto it = g_Glyphs.find((char)std::toupper(static_cast<unsigned char>(c)));
        w += (it != g_Glyphs.end() ? 6.0f * scale : 4.0f * scale);
    }
    return w;
}

void BuildButtons() {
    g_Buttons.clear();
    const int panelRightMargin = 18;
    const int btnW = 56;
    const int btnH = 26;
    const int gapY = 14;
    const int gapLabel = 8;
    const int buttonStartY = 18 + 60; // Start from top of right panel
    int y = buttonStartY;

    auto addRow = [&](const std::string& centerLabel,
                      std::function<void()> onMinus, std::function<void()> onPlus) {
        int centerW = 110;
        int rowX = g_i_WindowWidth - panelRightMargin - (btnW * 2 + centerW - btnW) - 16;
        SDL_Rect rMinus{ rowX, y, btnW, btnH };
        SDL_Rect rPlus{ rowX + btnW + centerW - btnW, y, btnW, btnH }; // place plus on right
        g_Buttons.push_back({ rMinus, "-", onMinus });
        g_Buttons.push_back({ rPlus, "+", onPlus });
        y += btnH + gapY;
    };

    addRow("WYS",
        [](){ g_f_BuildingHeight = std::max(1.0f, g_f_BuildingHeight - 1.0f); g_i_BuildingExample = 3; RegenerateAndUpload(); },
        [](){ g_f_BuildingHeight += 1.0f; g_i_BuildingExample = 3; RegenerateAndUpload(); });

    addRow("DACH",
        [](){ g_f_RoofHeight = std::max(0.5f, g_f_RoofHeight - 0.5f); g_i_BuildingExample = 3; RegenerateAndUpload(); },
        [](){ g_f_RoofHeight += 0.5f; g_i_BuildingExample = 3; RegenerateAndUpload(); });

    addRow("SZER",
        [](){ g_f_BaseWidth = std::max(2.0f, g_f_BaseWidth - 1.0f); g_i_BuildingExample = 3; RegenerateAndUpload(); },
        [](){ g_f_BaseWidth += 1.0f; g_i_BuildingExample = 3; RegenerateAndUpload(); });

    addRow("GLEB",
        [](){ g_f_BaseDepth = std::max(2.0f, g_f_BaseDepth - 1.0f); g_i_BuildingExample = 3; RegenerateAndUpload(); },
        [](){ g_f_BaseDepth += 1.0f; g_i_BuildingExample = 3; RegenerateAndUpload(); });

    int centerW = 110;
    int rowX = g_i_WindowWidth - panelRightMargin - (btnW * 2 + 40) - 16;
    
    SDL_Rect rRoofToggle{ rowX, y, btnW * 2 + 40, btnH };
    g_Buttons.push_back({ rRoofToggle, "DACH PL/SPA", [](){ g_b_UseFlatRoof = !g_b_UseFlatRoof; g_i_BuildingExample = 3; RegenerateAndUpload(); } });

    y += btnH + gapY;
    SDL_Rect rFacade{ rowX, y, btnW * 2 + 40, btnH };
    g_Buttons.push_back({ rFacade, "ELEWACJA", [](){ CycleFacadeTexture(); } });

    y += btnH + gapY;
    SDL_Rect rDoors{ rowX, y, btnW * 2 + 40, btnH };
    g_Buttons.push_back({ rDoors, "DRZWI", [](){ CycleDoorTexture(); } });

    y += btnH + gapY;
    SDL_Rect rWindows{ rowX, y, btnW * 2 + 40, btnH };
    g_Buttons.push_back({ rWindows, "OKNA", [](){ CycleWindowTexture(); } });

    y += btnH + gapY;
    SDL_Rect rSideWindows{ rowX, y, btnW * 2 + 40, btnH };
    g_Buttons.push_back({ rSideWindows, "OKNA BOK", [](){ g_b_UseSideWindowTexture = !g_b_UseSideWindowTexture; } });

    y += btnH + gapY;
    SDL_Rect rIvy{ rowX, y, btnW * 2 + 40, btnH };
    g_Buttons.push_back({ rIvy, "BLUSZCZ", [](){ g_b_ShowIvy = !g_b_ShowIvy; } });
}

bool HandleUIClick(int x, int y) {
    for (const auto& b : g_Buttons) {
        if (x >= b.rectPx.x && x <= b.rectPx.x + b.rectPx.w &&
            y >= b.rectPx.y && y <= b.rectPx.y + b.rectPx.h) {
            if (b.onClick) b.onClick();
            return true;
        }
    }
    return false;
}

void RenderUI() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawQuadPx(10, 10, 280, g_i_WindowHeight - 10, glm::vec4(0.05f, 0.07f, 0.12f, 0.88f));

    float titleScale = 4.0f;
    DrawTextPx("PANEL", 22, 28, titleScale, glm::vec4(1,1,1,1));

    float infoY = 60.0f;
    float infoScale = 3.0f;
    auto writeLine = [&](const std::string& label, float value) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %.1f", label.c_str(), value);
        DrawTextPx(buf, 22, infoY, infoScale, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
        infoY += 26.0f;
    };
    writeLine("WYS", g_f_BuildingHeight);
    writeLine("DACH", g_f_RoofHeight);
    writeLine("SZER", g_f_BaseWidth);
    writeLine("GLEB", g_f_BaseDepth);
    DrawTextPx(std::string("DACH: ") + (g_b_UseFlatRoof ? "PLASKI" : "SPADOWY"), 22, infoY, infoScale, glm::vec4(0.9f,0.9f,0.6f,1));
    infoY += 26.0f;
    DrawTextPx("TRYB: CUSTOM", 22, infoY, infoScale, glm::vec4(0.9f,0.9f,0.6f,1));
    infoY += 26.0f;
    DrawTextPx(std::string("ELEW: ") + GetCurrentFacadeLabel(), 22, infoY, infoScale, glm::vec4(0.8f,0.95f,0.8f,1));
    infoY += 26.0f;
    DrawTextPx(std::string("DRZW: ") + GetCurrentDoorLabel(), 22, infoY, infoScale, glm::vec4(0.8f,0.95f,0.8f,1));
    infoY += 26.0f;
    DrawTextPx(std::string("OKNA: ") + GetCurrentWindowLabel(), 22, infoY, infoScale, glm::vec4(0.8f,0.95f,0.8f,1));
    infoY += 26.0f;
    std::string s_SideWinState;
    if (g_TextureID_WindowSide == 0) {
        s_SideWinState = "BRAK";
    } else {
        s_SideWinState = g_b_UseSideWindowTexture ? "WL" : "WY";
    }
    DrawTextPx(std::string("OKNA BOK: ") + s_SideWinState, 22, infoY, infoScale, glm::vec4(0.8f,0.95f,0.8f,1));
    infoY += 26.0f;
    DrawTextPx(std::string("BLUSZCZ: ") + (g_b_ShowIvy ? "WL" : "WY"), 22, infoY, infoScale, glm::vec4(0.8f,0.95f,0.8f,1));

    const int panelRightMargin = 18;
    const int btnW = 56;
    const int btnH = 26;
    const int gapY = 14;
    const int gapLabel = 8;
    const int panelWidth = 200;
    const int buttonStartY = 18 + 60;
    
    DrawQuadPx(g_i_WindowWidth - panelWidth - 10, 10, g_i_WindowWidth - 10, g_i_WindowHeight - 10, glm::vec4(0.05f, 0.07f, 0.12f, 0.88f));
    
    int yRows = buttonStartY;
    int centerW = 110;
    int rowX = g_i_WindowWidth - panelRightMargin - (btnW * 2 + centerW - btnW) - 16;
    float labelScale = 2.6f;
    auto drawLabel = [&](const std::string& txt) {
        float labelX = rowX + btnW + 8.0f;
        float labelY = (float)yRows - gapLabel + 8.0f;
        DrawTextPx(txt, labelX, labelY, labelScale, glm::vec4(1,1,1,1));
        yRows += btnH + gapY;
    };
    drawLabel("WYS");
    drawLabel("DACH");
    drawLabel("SZER");
    drawLabel("GLEB");

    for (const auto& b : g_Buttons) {
        DrawQuadPx((float)b.rectPx.x, (float)b.rectPx.y, (float)(b.rectPx.x + b.rectPx.w), (float)(b.rectPx.y + b.rectPx.h), glm::vec4(0.2f, 0.26f, 0.34f, 0.9f));
        float tw = MeasureTextPx(b.label, 2.4f);
        float tx = b.rectPx.x + (b.rectPx.w - tw) * 0.5f;
        float ty = b.rectPx.y + b.rectPx.h * 0.2f;
        DrawTextPx(b.label, tx, ty, 2.4f, glm::vec4(1,1,1,1));
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

GLuint CreateShaderProgram() {
    GLuint ui_VertexShader = CompileShader(k_VertexShaderSource, GL_VERTEX_SHADER);
    GLuint ui_FragmentShader = CompileShader(k_FragmentShaderSource, GL_FRAGMENT_SHADER);

    GLuint ui_Program = glCreateProgram();
    glAttachShader(ui_Program, ui_VertexShader);
    glAttachShader(ui_Program, ui_FragmentShader);
    glLinkProgram(ui_Program);

    int i_Success;
    char infoLog[512];
    glGetProgramiv(ui_Program, GL_LINK_STATUS, &i_Success);
    if (!i_Success) {
        glGetProgramInfoLog(ui_Program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(ui_VertexShader);
    glDeleteShader(ui_FragmentShader);

    return ui_Program;
}

GLuint LoadTexture(const char* path) {
    GLuint ui_TextureID;
    glGenTextures(1, &ui_TextureID);
    glBindTexture(GL_TEXTURE_2D, ui_TextureID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int i_Width, i_Height, i_Channels;
    unsigned char* p_Data = stbi_load(path, &i_Width, &i_Height, &i_Channels, 0);
    if (p_Data) {
        GLenum e_Format = (i_Channels == 3) ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, e_Format, i_Width, i_Height, 0, e_Format, GL_UNSIGNED_BYTE, p_Data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "Texture loaded successfully: " << path << " (" << i_Width << "x" << i_Height << ")" << std::endl;
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(p_Data);
    
    return ui_TextureID;
}

std::string GetCurrentFacadeLabel() {
    if (g_FacadeTextures.empty()) return "BRAK";
    return g_FacadeTextures[g_i_FacadeIndex].name;
}

std::string GetCurrentWindowLabel() {
    if (g_WindowTextures.empty()) return "BRAK";
    return g_WindowTextures[g_i_WindowIndex].name;
}

std::string GetCurrentDoorLabel() {
    if (g_DoorTextures.empty()) return "BRAK";
    return g_DoorTextures[g_i_DoorIndex].name;
}

void LoadFacadeTextures() {
    g_FacadeTextures.clear();

    struct Option { const char* name; const char* path; } options[] = {
        {"BIALA", "assets/textures/facade/white.jpg"},
        {"ZOLTA", "assets/textures/facade/yellow.jpg"},
        {"NIEBIESKA", "assets/textures/facade/blue.jpg"}
    };

    for (const auto& opt : options) {
        FacadeTextureOption entry;
        entry.name = opt.name;
        entry.path = opt.path;
        entry.textureId = LoadTexture(opt.path);
        g_FacadeTextures.push_back(entry);
    }

    if (!g_FacadeTextures.empty()) {
        g_i_FacadeIndex = std::min(g_i_FacadeIndex, (int)g_FacadeTextures.size() - 1);
        g_TextureID = g_FacadeTextures[g_i_FacadeIndex].textureId;
    } else {
        g_TextureID = 0;
    }
}

void CycleFacadeTexture() {
    if (g_FacadeTextures.empty()) return;
    g_i_FacadeIndex = (g_i_FacadeIndex + 1) % static_cast<int>(g_FacadeTextures.size());
    g_TextureID = g_FacadeTextures[g_i_FacadeIndex].textureId;
}

void LoadWindowTextures() {
    for (auto& opt : g_WindowTextures) {
        if (opt.textureId) glDeleteTextures(1, &opt.textureId);
    }
    g_WindowTextures.clear();
    if (g_TextureID_WindowSide) {
        glDeleteTextures(1, &g_TextureID_WindowSide);
        g_TextureID_WindowSide = 0;
    }

    struct Option { const char* name; const char* path; } options[] = {
        {"W1", "assets/textures/windows/window1.png"},
        {"W2", "assets/textures/windows/window2.png"}
    };

    for (const auto& opt : options) {
        FacadeTextureOption entry;
        entry.name = opt.name;
        entry.path = opt.path;
        entry.textureId = LoadTexture(opt.path);
        g_WindowTextures.push_back(entry);
    }

    g_TextureID_WindowSide = LoadTexture("assets/textures/windows/window3.png");

    if (!g_WindowTextures.empty()) {
        g_i_WindowIndex = std::min(g_i_WindowIndex, static_cast<int>(g_WindowTextures.size()) - 1);
        g_TextureID_Windows = g_WindowTextures[g_i_WindowIndex].textureId;
    } else {
        g_TextureID_Windows = 0;
    }
}

void LoadDoorTextures() {
    for (auto& opt : g_DoorTextures) {
        if (opt.textureId) glDeleteTextures(1, &opt.textureId);
    }
    g_DoorTextures.clear();

    struct Option { const char* name; const char* path; } options[] = {
        {"D1", "assets/textures/door/door1.jpg"},
        {"D2", "assets/textures/door/door2.png"},
        {"D3", "assets/textures/door/door3.jpg"}
    };

    for (const auto& opt : options) {
        FacadeTextureOption entry;
        entry.name = opt.name;
        entry.path = opt.path;
        entry.textureId = LoadTexture(opt.path);
        g_DoorTextures.push_back(entry);
    }

    if (!g_DoorTextures.empty()) {
        g_i_DoorIndex = std::min(g_i_DoorIndex, static_cast<int>(g_DoorTextures.size()) - 1);
        g_TextureID_Doors = g_DoorTextures[g_i_DoorIndex].textureId;
    } else {
        g_TextureID_Doors = 0;
    }
}

void CycleWindowTexture() {
    if (g_WindowTextures.empty()) return;
    g_i_WindowIndex = (g_i_WindowIndex + 1) % static_cast<int>(g_WindowTextures.size());
    g_TextureID_Windows = g_WindowTextures[g_i_WindowIndex].textureId;
}

void CycleDoorTexture() {
    if (g_DoorTextures.empty()) return;
    g_i_DoorIndex = (g_i_DoorIndex + 1) % static_cast<int>(g_DoorTextures.size());
    g_TextureID_Doors = g_DoorTextures[g_i_DoorIndex].textureId;
}

// ===== BUILDING GENERATION =====
void GenerateBuilding() {
    std::vector<glm::vec2> vec_Points;
    
    switch (g_i_BuildingExample) {
        case 0:
            vec_Points = {
                glm::vec2(0.0f, 0.0f),
                glm::vec2(10.0f, 0.0f),
                glm::vec2(10.0f, 5.0f),
                glm::vec2(0.0f, 5.0f)
            };
            break;
        case 1:
            vec_Points = {
                glm::vec2(0.0f, 0.0f),
                glm::vec2(8.0f, 0.0f),
                glm::vec2(8.0f, 8.0f),
                glm::vec2(0.0f, 8.0f)
            };
            break;
        case 2:
            vec_Points = {
                glm::vec2(0.0f, 0.0f),
                glm::vec2(6.0f, 0.0f),
                glm::vec2(6.0f, 15.0f),
                glm::vec2(0.0f, 15.0f)
            };
            break;
        case 3:
            vec_Points = {
                glm::vec2(0.0f, 0.0f),
                glm::vec2(g_f_BaseWidth, 0.0f),
                glm::vec2(g_f_BaseWidth, g_f_BaseDepth),
                glm::vec2(0.0f, g_f_BaseDepth)
            };
            break;
    }

    g_BuildingMesh = BuildingGenerator::GenerateBuilding(
        vec_Points, 
        g_f_BuildingHeight,
        !g_b_UseFlatRoof,
        g_f_RoofHeight
    );
    
    std::cout << "Generated building with " << g_BuildingMesh.vertices.size() 
              << " vertices and " << g_BuildingMesh.indices.size() / 3 
              << " triangles" << std::endl;
    std::cout << "Roof type: " << (g_b_UseFlatRoof ? "FLAT" : "GABLED") << std::endl;
    std::cout << "Building height: " << g_f_BuildingHeight << ", Roof height: " << g_f_RoofHeight << std::endl;
    if (g_i_BuildingExample == 3) {
        std::cout << "Custom dimensions: " << g_f_BaseWidth << " x " << g_f_BaseDepth << std::endl;
    }
}

void UploadMeshToGPU() {
    if (g_VBO) glDeleteBuffers(1, &g_VBO);
    if (g_VBO_Normal) glDeleteBuffers(1, &g_VBO_Normal);
    if (g_VBO_TexCoord) glDeleteBuffers(1, &g_VBO_TexCoord);
    if (g_EBO) glDeleteBuffers(1, &g_EBO);
    
    glGenVertexArrays(1, &g_VAO);
    glBindVertexArray(g_VAO);
    
    glGenBuffers(1, &g_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 g_BuildingMesh.vertices.size() * sizeof(glm::vec3),
                 g_BuildingMesh.vertices.data(), 
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    
    glGenBuffers(1, &g_VBO_Normal);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO_Normal);
    glBufferData(GL_ARRAY_BUFFER,
                 g_BuildingMesh.normals.size() * sizeof(glm::vec3),
                 g_BuildingMesh.normals.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);
    
    glGenBuffers(1, &g_VBO_TexCoord);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO_TexCoord);
    glBufferData(GL_ARRAY_BUFFER,
                 g_BuildingMesh.texCoords.size() * sizeof(glm::vec2),
                 g_BuildingMesh.texCoords.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(2);
    
    glGenBuffers(1, &g_EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 g_BuildingMesh.indices.size() * sizeof(unsigned int),
                 g_BuildingMesh.indices.data(),
                 GL_STATIC_DRAW);
    
    glBindVertexArray(0);
}

void UploadWindowsToGPU() {
    for (auto vao : g_VAO_Windows) glDeleteVertexArrays(1, &vao);
    for (auto vbo : g_VBO_Windows) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Windows_Normal) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Windows_TexCoord) glDeleteBuffers(1, &vbo);
    for (auto ebo : g_EBO_Windows) glDeleteBuffers(1, &ebo);
    
    g_VAO_Windows.clear();
    g_VBO_Windows.clear();
    g_VBO_Windows_Normal.clear();
    g_VBO_Windows_TexCoord.clear();
    g_EBO_Windows.clear();
    
    for (const auto& windowWall : g_BuildingMesh.windowWalls) {
        if (windowWall.vertices.empty()) continue;
        
        GLuint vao, vbo, vbo_normal, vbo_texcoord, ebo;
        
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 
                     windowWall.vertices.size() * sizeof(glm::vec3),
                     windowWall.vertices.data(), 
                     GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);
        
        glGenBuffers(1, &vbo_normal);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_normal);
        glBufferData(GL_ARRAY_BUFFER,
                     windowWall.normals.size() * sizeof(glm::vec3),
                     windowWall.normals.data(),
                     GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(1);
        
        glGenBuffers(1, &vbo_texcoord);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_texcoord);
        glBufferData(GL_ARRAY_BUFFER,
                     windowWall.texCoords.size() * sizeof(glm::vec2),
                     windowWall.texCoords.data(),
                     GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
        glEnableVertexAttribArray(2);
        
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     windowWall.indices.size() * sizeof(unsigned int),
                     windowWall.indices.data(),
                     GL_STATIC_DRAW);
        
        glBindVertexArray(0);
        
        g_VAO_Windows.push_back(vao);
        g_VBO_Windows.push_back(vbo);
        g_VBO_Windows_Normal.push_back(vbo_normal);
        g_VBO_Windows_TexCoord.push_back(vbo_texcoord);
        g_EBO_Windows.push_back(ebo);
    }
}

void UploadDoorsToGPU() {
    for (auto vao : g_VAO_Doors) glDeleteVertexArrays(1, &vao);
    for (auto vbo : g_VBO_Doors) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Doors_Normal) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Doors_TexCoord) glDeleteBuffers(1, &vbo);
    for (auto ebo : g_EBO_Doors) glDeleteBuffers(1, &ebo);

    g_VAO_Doors.clear();
    g_VBO_Doors.clear();
    g_VBO_Doors_Normal.clear();
    g_VBO_Doors_TexCoord.clear();
    g_EBO_Doors.clear();

    for (const auto& door : g_BuildingMesh.doors) {
        if (door.vertices.empty()) continue;

        GLuint vao = 0, vbo = 0, vboNormal = 0, vboTex = 0, ebo = 0;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, door.vertices.size() * sizeof(glm::vec3), door.vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        glGenBuffers(1, &vboNormal);
        glBindBuffer(GL_ARRAY_BUFFER, vboNormal);
        glBufferData(GL_ARRAY_BUFFER, door.normals.size() * sizeof(glm::vec3), door.normals.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(1);

        glGenBuffers(1, &vboTex);
        glBindBuffer(GL_ARRAY_BUFFER, vboTex);
        glBufferData(GL_ARRAY_BUFFER, door.texCoords.size() * sizeof(glm::vec2), door.texCoords.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
        glEnableVertexAttribArray(2);

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, door.indices.size() * sizeof(unsigned int), door.indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);

        g_VAO_Doors.push_back(vao);
        g_VBO_Doors.push_back(vbo);
        g_VBO_Doors_Normal.push_back(vboNormal);
        g_VBO_Doors_TexCoord.push_back(vboTex);
        g_EBO_Doors.push_back(ebo);
    }
}

void UploadIvyToGPU() {
    if (g_VAO_Ivy) {
        glDeleteVertexArrays(1, &g_VAO_Ivy);
        g_VAO_Ivy = 0;
    }
    if (g_VBO_Ivy) {
        glDeleteBuffers(1, &g_VBO_Ivy);
        g_VBO_Ivy = 0;
    }
    if (g_VBO_Ivy_Normal) {
        glDeleteBuffers(1, &g_VBO_Ivy_Normal);
        g_VBO_Ivy_Normal = 0;
    }
    if (g_VBO_Ivy_TexCoord) {
        glDeleteBuffers(1, &g_VBO_Ivy_TexCoord);
        g_VBO_Ivy_TexCoord = 0;
    }
    if (g_EBO_Ivy) {
        glDeleteBuffers(1, &g_EBO_Ivy);
        g_EBO_Ivy = 0;
    }

    g_b_HasIvy = false;
    g_IvyWallCenter = glm::vec3(0.0f);
    g_IvyWallNormal = glm::vec3(0.0f, 1.0f, 0.0f);

    if (g_f_BaseWidth < 6.0f || g_f_BaseWidth > 11.0f || g_f_BuildingHeight < 8.0f || g_f_BuildingHeight > 13.0f) {
        return;
    }

    if (g_BuildingMesh.vertices.empty()) {
        return;
    }

    float backY = -std::numeric_limits<float>::max();
    for (const auto& v : g_BuildingMesh.vertices) {
        backY = std::max(backY, v.y);
    }

    if (!std::isfinite(backY)) {
        return;
    }

    const float tolerance = 0.05f;
    float minX = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();
    int wallVertCount = 0;

    for (const auto& v : g_BuildingMesh.vertices) {
        if (std::abs(v.y - backY) <= tolerance) {
            minX = std::min(minX, v.x);
            maxX = std::max(maxX, v.x);
            minZ = std::min(minZ, v.z);
            maxZ = std::max(maxZ, v.z);
            ++wallVertCount;
        }
    }

    if (wallVertCount < 4 || !std::isfinite(minX) || !std::isfinite(maxX)) {
        return;
    }

    if ((maxX - minX) < 0.01f || (maxZ - minZ) < 0.01f) {
        return;
    }

    float ivyTop = maxZ;
    float wallHeight = maxZ - minZ;
    const float targetSpan = 2.2f;
    float minSpan = std::max(0.8f, wallHeight * 0.6f);
    float maxSpan = std::max(minSpan + 0.1f, wallHeight - 0.4f);
    float ivyHeight = std::clamp(targetSpan, minSpan, maxSpan);
    float ivyBottom = ivyTop - ivyHeight;
    float minClear = minZ + wallHeight * 0.15f;
    if (ivyBottom < minClear) {
        ivyBottom = minClear;
        ivyHeight = ivyTop - ivyBottom;
    }

    const float offset = 0.025f;
    std::array<glm::vec3, 4> positions = {
        glm::vec3(minX, backY + offset, ivyTop),
        glm::vec3(maxX, backY + offset, ivyTop),
        glm::vec3(maxX, backY + offset, ivyBottom),
        glm::vec3(minX, backY + offset, ivyBottom)
    };

    std::array<glm::vec2, 4> texCoords = {
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(0.0f, 1.0f)
    };

    std::array<unsigned int, 6> indices = {0, 1, 2, 0, 2, 3};

    glm::vec3 normal = glm::cross(positions[1] - positions[0], positions[3] - positions[0]);
    float normalLen = glm::length(normal);
    if (normalLen > 1e-5f) {
        normal /= normalLen;
    } else {
        normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    if (normal.y < 0.0f) {
        normal = -normal;
    }

    std::array<glm::vec3, 4> normals = {
        normal,
        normal,
        normal,
        normal
    };

    glGenVertexArrays(1, &g_VAO_Ivy);
    glBindVertexArray(g_VAO_Ivy);

    glGenBuffers(1, &g_VBO_Ivy);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO_Ivy);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &g_VBO_Ivy_Normal);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO_Ivy_Normal);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &g_VBO_Ivy_TexCoord);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO_Ivy_TexCoord);
    glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(glm::vec2), texCoords.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(2);

    glGenBuffers(1, &g_EBO_Ivy);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_EBO_Ivy);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    g_IvyWallCenter = (positions[0] + positions[2]) * 0.5f;
    g_IvyWallNormal = normal;
    g_b_HasIvy = true;
}

void RegenerateAndUpload() {
    GenerateBuilding();
    UploadMeshToGPU();
    UploadWindowsToGPU();
    UploadDoorsToGPU();
    UploadIvyToGPU();
}

bool InitSDLAndOpenGL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    g_p_Window = SDL_CreateWindow(
        "Building Generator - Drag mouse to rotate, scroll to zoom",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!g_p_Window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        return false;
    }

    g_GLContext = SDL_GL_CreateContext(g_p_Window);
    if (!g_GLContext) {
        std::cerr << "OpenGL context creation failed: " << SDL_GetError() << std::endl;
        return false;
    }

    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "GLEW initialization failed!" << std::endl;
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);

    return true;
}

// ===== EVENT HANDLING =====
void HandleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_b_ShouldClose = true;
        }
        else if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    g_b_ShouldClose = true;
                    break;
                case SDLK_SPACE:
                    g_b_UseFlatRoof = !g_b_UseFlatRoof;
                    RegenerateAndUpload();
                    break;
                case SDLK_1:
                    g_i_BuildingExample = 0;
                    RegenerateAndUpload();
                    break;
                case SDLK_2:
                    g_i_BuildingExample = 1;
                    RegenerateAndUpload();
                    break;
                case SDLK_3:
                    g_i_BuildingExample = 2;
                    RegenerateAndUpload();
                    break;
                case SDLK_4:
                    g_i_BuildingExample = 3;
                    RegenerateAndUpload();
                    break;
                case SDLK_h:
                    g_f_BuildingHeight += 1.0f;
                    std::cout << "Building height: " << g_f_BuildingHeight << std::endl;
                    RegenerateAndUpload();
                    break;
                case SDLK_g:
                    g_f_BuildingHeight -= 1.0f;
                    if (g_f_BuildingHeight < 1.0f) g_f_BuildingHeight = 1.0f;
                    std::cout << "Building height: " << g_f_BuildingHeight << std::endl;
                    RegenerateAndUpload();
                    break;
                case SDLK_r:
                    g_f_RoofHeight += 0.5f;
                    std::cout << "Roof height: " << g_f_RoofHeight << std::endl;
                    RegenerateAndUpload();
                    break;
                case SDLK_f:
                    g_f_RoofHeight -= 0.5f;
                    if (g_f_RoofHeight < 0.5f) g_f_RoofHeight = 0.5f;
                    std::cout << "Roof height: " << g_f_RoofHeight << std::endl;
                    RegenerateAndUpload();
                    break;
                case SDLK_w:
                    g_f_BaseWidth += 1.0f;
                    std::cout << "Base width: " << g_f_BaseWidth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        RegenerateAndUpload();
                    }
                    break;
                case SDLK_q:
                    g_f_BaseWidth -= 1.0f;
                    if (g_f_BaseWidth < 2.0f) g_f_BaseWidth = 2.0f;
                    std::cout << "Base width: " << g_f_BaseWidth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        RegenerateAndUpload();
                    }
                    break;
                case SDLK_e:
                    g_f_BaseDepth += 1.0f;
                    std::cout << "Base depth: " << g_f_BaseDepth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        RegenerateAndUpload();
                    }
                    break;
                case SDLK_a:
                    g_f_BaseDepth -= 1.0f;
                    if (g_f_BaseDepth < 2.0f) g_f_BaseDepth = 2.0f;
                    std::cout << "Base depth: " << g_f_BaseDepth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        RegenerateAndUpload();
                    }
                    break;
                case SDLK_UP:
                    g_f_RotationX += 5.0f;
                    break;
                case SDLK_DOWN:
                    g_f_RotationX -= 5.0f;
                    break;
                case SDLK_LEFT:
                    g_f_RotationY -= 5.0f;
                    break;
                case SDLK_RIGHT:
                    g_f_RotationY += 5.0f;
                    break;
                case SDLK_EQUALS:
                    g_f_ZoomDistance -= 2.0f;
                    if (g_f_ZoomDistance < 5.0f) g_f_ZoomDistance = 5.0f;
                    break;
                case SDLK_MINUS:
                    g_f_ZoomDistance += 2.0f;
                    if (g_f_ZoomDistance > 100.0f) g_f_ZoomDistance = 100.0f;
                    break;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (!HandleUIClick(event.button.x, event.button.y)) {
                    g_b_MousePressed = true;
                    g_i_LastMouseX = event.button.x;
                    g_i_LastMouseY = event.button.y;
                }
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                g_b_MousePressed = false;
            }
        }
        else if (event.type == SDL_MOUSEMOTION) {
            if (g_b_MousePressed) {
                int i_DeltaX = event.motion.x - g_i_LastMouseX;
                int i_DeltaY = event.motion.y - g_i_LastMouseY;
                
                g_f_RotationY += i_DeltaX * 0.5f;
                g_f_RotationX += i_DeltaY * 0.5f;
                
                g_i_LastMouseX = event.motion.x;
                g_i_LastMouseY = event.motion.y;
            }
        }
        else if (event.type == SDL_MOUSEWHEEL) {
            g_f_ZoomDistance -= event.wheel.y * 2.0f;
            if (g_f_ZoomDistance < 5.0f) g_f_ZoomDistance = 5.0f;
            if (g_f_ZoomDistance > 100.0f) g_f_ZoomDistance = 100.0f;
        }
    }
}

// ===== RENDERING =====
void Render() {
    SDL_GetWindowSize(g_p_Window, &g_i_WindowWidth, &g_i_WindowHeight);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_ShaderProgram);

    // Setup matrices
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(g_f_RotationX), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(g_f_RotationY), glm::vec3(0.0f, 0.0f, 1.0f));
    
    // Center the building
    float f_CenterX = 0.0f;
    float f_CenterY = 0.0f;
    float f_CenterZ = g_f_BuildingHeight / 2.0f;
    
    if (g_i_BuildingExample == 0) { 
        f_CenterX = 5.0f; 
        f_CenterY = 2.5f; 
    }
    else if (g_i_BuildingExample == 1) { 
        f_CenterX = 4.0f; 
        f_CenterY = 4.0f; 
    }
    else if (g_i_BuildingExample == 2) { 
        f_CenterX = 3.0f; 
        f_CenterY = 7.5f; 
    }
    else if (g_i_BuildingExample == 3) { 
        f_CenterX = g_f_BaseWidth / 2.0f; 
        f_CenterY = g_f_BaseDepth / 2.0f; 
    }
    
    model = glm::translate(model, glm::vec3(-f_CenterX, -f_CenterY, -f_CenterZ));

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -g_f_ZoomDistance));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 200.0f);

    glUniformMatrix4fv(glGetUniformLocation(g_ShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(g_ShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(g_ShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glm::vec3 lightPos(20.0f, 20.0f, 20.0f);
    glm::vec3 viewPos(0.0f, 0.0f, g_f_ZoomDistance);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

    glUniform3fv(glGetUniformLocation(g_ShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(g_ShaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));
    glUniform3fv(glGetUniformLocation(g_ShaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
    glUniform1f(glGetUniformLocation(g_ShaderProgram, "textureExposure"), 1.0f);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_TextureID);
    glUniform1i(glGetUniformLocation(g_ShaderProgram, "texture1"), 0);
    glUniform1i(glGetUniformLocation(g_ShaderProgram, "useTexture"), g_TextureID != 0);
    
    glBindVertexArray(g_VAO);
    
    for (const auto& material : g_BuildingMesh.materials) {
        bool isRoof = (material.name == "roof") && g_TextureID_Roof != 0;
        glUniform1i(glGetUniformLocation(g_ShaderProgram, "isRoof"), isRoof ? 1 : 0);
        glUniform1f(glGetUniformLocation(g_ShaderProgram, "roofUVScale"), isRoof ? 3.0f : 1.0f);
        if (isRoof) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_TextureID_Roof);
            glUniform1i(glGetUniformLocation(g_ShaderProgram, "texture1"), 0);
            glUniform1i(glGetUniformLocation(g_ShaderProgram, "useTexture"), 1);
        } else {
            glUniform1i(glGetUniformLocation(g_ShaderProgram, "useTexture"), g_TextureID != 0);
            if (g_TextureID != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, g_TextureID);
                glUniform1i(glGetUniformLocation(g_ShaderProgram, "texture1"), 0);
            }
        }
        glUniform3fv(glGetUniformLocation(g_ShaderProgram, "objColor"), 1, glm::value_ptr(material.color));
        glDrawElements(GL_TRIANGLES, material.indexCount, GL_UNSIGNED_INT, 
                      (void*)(material.firstIndex * sizeof(unsigned int)));
    }

    glBindVertexArray(0);
    
    bool b_UseSideWindows = g_b_UseSideWindowTexture && g_TextureID_WindowSide != 0;
    bool b_HasAnyWindowTexture = (g_TextureID_Windows != 0) || b_UseSideWindows;
    bool hasWindows = !g_BuildingMesh.windowWalls.empty() && b_HasAnyWindowTexture;
    bool hasDoors = !g_BuildingMesh.doors.empty() && g_TextureID_Doors != 0;
    bool hasIvy = g_b_ShowIvy && g_b_HasIvy && g_TextureID_Ivy != 0;

    if (hasWindows || hasDoors || hasIvy) {
        glm::vec3 viewPosWorld = glm::vec3(0.0f, 0.0f, g_f_ZoomDistance);
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (hasWindows) {
            for (size_t i = 0; i < g_BuildingMesh.windowWalls.size() && i < g_VAO_Windows.size(); ++i) {
                const auto& windowWall = g_BuildingMesh.windowWalls[i];

                GLuint ui_WindowTexture = g_TextureID_Windows;
                if (b_UseSideWindows) {
                    glm::vec3 vec_LocalNormal = windowWall.wallNormal;
                    float f_Len = glm::length(vec_LocalNormal);
                    if (f_Len > 1e-4f) {
                        vec_LocalNormal /= f_Len;
                        if (glm::dot(vec_LocalNormal, glm::vec3(1.0f, 0.0f, 0.0f)) > 0.7f) {
                            ui_WindowTexture = g_TextureID_WindowSide;
                        }
                    }
                }

                if (ui_WindowTexture == 0) {
                    continue;
                }

                glm::vec3 wallCenterWorld = glm::vec3(model * glm::vec4(windowWall.wallCenter, 1.0f));
                glm::vec3 wallNormalWorld = glm::normalize(normalMatrix * windowWall.wallNormal);
                glm::vec3 viewDir = glm::normalize(viewPosWorld - wallCenterWorld);

                if (glm::dot(wallNormalWorld, viewDir) > 0.0f) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, ui_WindowTexture);
                    glUniform1i(glGetUniformLocation(g_ShaderProgram, "texture1"), 0);
                    glUniform1i(glGetUniformLocation(g_ShaderProgram, "useTexture"), 1);
                    glUniform1i(glGetUniformLocation(g_ShaderProgram, "isRoof"), 0);
                    glUniform1f(glGetUniformLocation(g_ShaderProgram, "roofUVScale"), 1.0f);
                    glUniform1f(glGetUniformLocation(g_ShaderProgram, "textureExposure"), 1.0f);
                    glBindVertexArray(g_VAO_Windows[i]);
                    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(windowWall.indices.size()), GL_UNSIGNED_INT, 0);
                }
            }
        }

        if (hasIvy && g_VAO_Ivy) {
            glm::vec3 ivyCenterWorld = glm::vec3(model * glm::vec4(g_IvyWallCenter, 1.0f));
            glm::vec3 ivyNormalWorld = glm::normalize(normalMatrix * g_IvyWallNormal);
            glm::vec3 viewDirIvy = glm::normalize(viewPosWorld - ivyCenterWorld);

            if (glm::dot(ivyNormalWorld, viewDirIvy) > 0.0f) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, g_TextureID_Ivy);
                glUniform1i(glGetUniformLocation(g_ShaderProgram, "texture1"), 0);
                glUniform1i(glGetUniformLocation(g_ShaderProgram, "useTexture"), 1);
                glUniform1i(glGetUniformLocation(g_ShaderProgram, "isRoof"), 0);
                glUniform1f(glGetUniformLocation(g_ShaderProgram, "roofUVScale"), 1.0f);
                glUniform1f(glGetUniformLocation(g_ShaderProgram, "textureExposure"), 1.0f);
                glBindVertexArray(g_VAO_Ivy);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }

        if (hasDoors) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_TextureID_Doors);
            glUniform1i(glGetUniformLocation(g_ShaderProgram, "texture1"), 0);
            glUniform1i(glGetUniformLocation(g_ShaderProgram, "useTexture"), 1);
            glUniform1i(glGetUniformLocation(g_ShaderProgram, "isRoof"), 0);
            glUniform1f(glGetUniformLocation(g_ShaderProgram, "roofUVScale"), 1.0f);
            glUniform1f(glGetUniformLocation(g_ShaderProgram, "textureExposure"), 1.0f);

            for (size_t i = 0; i < g_BuildingMesh.doors.size() && i < g_VAO_Doors.size(); ++i) {
                const auto& door = g_BuildingMesh.doors[i];

                glm::vec3 doorCenterWorld = glm::vec3(model * glm::vec4(door.wallCenter, 1.0f));
                glm::vec3 doorNormalWorld = glm::normalize(normalMatrix * door.wallNormal);
                glm::vec3 viewDir = glm::normalize(viewPosWorld - doorCenterWorld);

                if (glm::dot(doorNormalWorld, viewDir) > 0.0f) {
                    glBindVertexArray(g_VAO_Doors[i]);
                    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(door.indices.size()), GL_UNSIGNED_INT, 0);
                }
            }
        }

        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    // UI overlay on top
    RenderUI();

    SDL_GL_SwapWindow(g_p_Window);
}

// ===== CLEANUP =====
void Cleanup() {
    if (g_VAO) glDeleteVertexArrays(1, &g_VAO);
    if (g_VBO) glDeleteBuffers(1, &g_VBO);
    if (g_VBO_Normal) glDeleteBuffers(1, &g_VBO_Normal);
    if (g_VBO_TexCoord) glDeleteBuffers(1, &g_VBO_TexCoord);
    if (g_EBO) glDeleteBuffers(1, &g_EBO);
    for (const auto& fac : g_FacadeTextures) {
        if (fac.textureId) glDeleteTextures(1, &fac.textureId);
    }
    g_FacadeTextures.clear();
    g_TextureID = 0;

    for (auto& opt : g_WindowTextures) {
        if (opt.textureId) glDeleteTextures(1, &opt.textureId);
    }
    g_WindowTextures.clear();
    g_TextureID_Windows = 0;

    for (auto& opt : g_DoorTextures) {
        if (opt.textureId) glDeleteTextures(1, &opt.textureId);
    }
    g_DoorTextures.clear();
    g_TextureID_Doors = 0;

    if (g_VAO_Ivy) glDeleteVertexArrays(1, &g_VAO_Ivy);
    if (g_VBO_Ivy) glDeleteBuffers(1, &g_VBO_Ivy);
    if (g_VBO_Ivy_Normal) glDeleteBuffers(1, &g_VBO_Ivy_Normal);
    if (g_VBO_Ivy_TexCoord) glDeleteBuffers(1, &g_VBO_Ivy_TexCoord);
    if (g_EBO_Ivy) glDeleteBuffers(1, &g_EBO_Ivy);
    if (g_TextureID_Ivy) glDeleteTextures(1, &g_TextureID_Ivy);
    g_VAO_Ivy = g_VBO_Ivy = g_VBO_Ivy_Normal = g_VBO_Ivy_TexCoord = g_EBO_Ivy = 0;
    g_TextureID_Ivy = 0;
    g_b_HasIvy = false;
    
    for (auto vao : g_VAO_Windows) glDeleteVertexArrays(1, &vao);
    for (auto vbo : g_VBO_Windows) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Windows_Normal) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Windows_TexCoord) glDeleteBuffers(1, &vbo);
    for (auto ebo : g_EBO_Windows) glDeleteBuffers(1, &ebo);
    for (auto vao : g_VAO_Doors) glDeleteVertexArrays(1, &vao);
    for (auto vbo : g_VBO_Doors) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Doors_Normal) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Doors_TexCoord) glDeleteBuffers(1, &vbo);
    for (auto ebo : g_EBO_Doors) glDeleteBuffers(1, &ebo);
    if (g_TextureID_Roof) glDeleteTextures(1, &g_TextureID_Roof);
    if (g_UIVAO) glDeleteVertexArrays(1, &g_UIVAO);
    if (g_UIVBO) glDeleteBuffers(1, &g_UIVBO);
    if (g_UIShaderProgram) glDeleteProgram(g_UIShaderProgram);
    
    if (g_ShaderProgram) glDeleteProgram(g_ShaderProgram);

    if (g_GLContext) SDL_GL_DeleteContext(g_GLContext);
    if (g_p_Window) SDL_DestroyWindow(g_p_Window);
    SDL_Quit();
}

// ===== MAIN =====
int main(int argc, char* argv[]) {
    std::cout << "=== Building Generator Test ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  LEFT MOUSE - Drag to rotate building" << std::endl;
    std::cout << "  SCROLL     - Zoom in/out" << std::endl;
    std::cout << std::endl;
    std::cout << "Building Examples:" << std::endl;
    std::cout << "  1, 2, 3    - Predefined building shapes" << std::endl;
    std::cout << "  4          - Custom building (editable)" << std::endl;
    std::cout << "  SPACE      - Toggle flat/gabled roof" << std::endl;
    std::cout << std::endl;
    std::cout << "Edit Building Parameters:" << std::endl;
    std::cout << "  H / G      - Increase/Decrease building height" << std::endl;
    std::cout << "  R / F      - Increase/Decrease roof height" << std::endl;
    std::cout << "  W / Q      - Increase/Decrease base width (custom only)" << std::endl;
    std::cout << "  E / A      - Increase/Decrease base depth (custom only)" << std::endl;
    std::cout << std::endl;
    std::cout << "View Controls:" << std::endl;
    std::cout << "  Arrow keys - Rotate view" << std::endl;
    std::cout << "  +/-        - Zoom in/out" << std::endl;
    std::cout << "  ESC        - Exit" << std::endl;
    std::cout << std::endl;

    if (!InitSDLAndOpenGL()) {
        return 1;
    }

    g_ShaderProgram = CreateShaderProgram();
    g_UIShaderProgram = CreateUIShader();
    InitGlyphs();
    BuildButtons();
    
    LoadFacadeTextures();
    LoadWindowTextures();
    LoadDoorTextures();
    g_TextureID_Roof = LoadTexture("assets/textures/210_clay roof texture seamless.jpg");
    g_TextureID_Ivy = LoadTexture("assets/textures/ivy.png");
    if (g_TextureID_Ivy) {
        glBindTexture(GL_TEXTURE_2D, g_TextureID_Ivy);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    RegenerateAndUpload();

    while (!g_b_ShouldClose) {
        HandleEvents();
        Render();
        SDL_Delay(16);
    }

    Cleanup();
    
    std::cout << "Program exited successfully." << std::endl;
    return 0;
}
