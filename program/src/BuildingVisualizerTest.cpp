#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

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
    vec3 baseColor = useTexture ? texSample.rgb : objColor;
    // Optionally tint roofs slightly warmer if needed
    float alpha = useTexture ? texSample.a : 1.0;
    // Drop nearly transparent fragments to avoid darkening from premultiplied backgrounds
    if (alpha < 0.05)
        discard;
    vec3 result = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(result, alpha);
}
)";

// ===== GLOBALS =====
SDL_Window* g_p_Window = nullptr;
SDL_GLContext g_GLContext = nullptr;
bool g_b_ShouldClose = false;

GLuint g_ShaderProgram = 0;
GLuint g_VAO = 0;
GLuint g_VBO = 0;
GLuint g_VBO_Normal = 0;
GLuint g_VBO_TexCoord = 0;
GLuint g_EBO = 0;
GLuint g_TextureID = 0;
GLuint g_TextureID_Roof = 0;

// Windows
std::vector<GLuint> g_VAO_Windows;
std::vector<GLuint> g_VBO_Windows;
std::vector<GLuint> g_VBO_Windows_Normal;
std::vector<GLuint> g_VBO_Windows_TexCoord;
std::vector<GLuint> g_EBO_Windows;
GLuint g_TextureID_Windows = 0;

// Doors
std::vector<GLuint> g_VAO_Doors;
std::vector<GLuint> g_VBO_Doors;
std::vector<GLuint> g_VBO_Doors_Normal;
std::vector<GLuint> g_VBO_Doors_TexCoord;
std::vector<GLuint> g_EBO_Doors;
GLuint g_TextureID_Door = 0;

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

// Mouse control
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
    // Clear old resources
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
    
    // Create VAO/VBO/EBO for each wall's windows
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

        GLuint vao, vbo, vbo_normal, vbo_texcoord, ebo;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, door.vertices.size() * sizeof(glm::vec3), door.vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        glGenBuffers(1, &vbo_normal);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_normal);
        glBufferData(GL_ARRAY_BUFFER, door.normals.size() * sizeof(glm::vec3), door.normals.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(1);

        glGenBuffers(1, &vbo_texcoord);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_texcoord);
        glBufferData(GL_ARRAY_BUFFER, door.texCoords.size() * sizeof(glm::vec2), door.texCoords.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
        glEnableVertexAttribArray(2);

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, door.indices.size() * sizeof(unsigned int), door.indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);

        g_VAO_Doors.push_back(vao);
        g_VBO_Doors.push_back(vbo);
        g_VBO_Doors_Normal.push_back(vbo_normal);
        g_VBO_Doors_TexCoord.push_back(vbo_texcoord);
        g_EBO_Doors.push_back(ebo);
    }
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
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    UploadDoorsToGPU();
                    break;
                case SDLK_1:
                    g_i_BuildingExample = 0;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    UploadDoorsToGPU();
                    break;
                case SDLK_2:
                    g_i_BuildingExample = 1;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    UploadDoorsToGPU();
                    break;
                case SDLK_3:
                    g_i_BuildingExample = 2;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    UploadDoorsToGPU();
                    break;
                case SDLK_4:
                    g_i_BuildingExample = 3;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    UploadDoorsToGPU();
                    break;
                case SDLK_h:
                    g_f_BuildingHeight += 1.0f;
                    std::cout << "Building height: " << g_f_BuildingHeight << std::endl;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    UploadDoorsToGPU();
                    break;
                case SDLK_g:
                    g_f_BuildingHeight -= 1.0f;
                    if (g_f_BuildingHeight < 1.0f) g_f_BuildingHeight = 1.0f;
                    std::cout << "Building height: " << g_f_BuildingHeight << std::endl;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    break;
                case SDLK_r:
                    g_f_RoofHeight += 0.5f;
                    std::cout << "Roof height: " << g_f_RoofHeight << std::endl;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    break;
                case SDLK_f:
                    g_f_RoofHeight -= 0.5f;
                    if (g_f_RoofHeight < 0.5f) g_f_RoofHeight = 0.5f;
                    std::cout << "Roof height: " << g_f_RoofHeight << std::endl;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    UploadWindowsToGPU();
                    break;
                case SDLK_w:
                    g_f_BaseWidth += 1.0f;
                    std::cout << "Base width: " << g_f_BaseWidth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        GenerateBuilding();
                        UploadMeshToGPU();
                        UploadWindowsToGPU();
                        UploadDoorsToGPU();
                    }
                    break;
                case SDLK_q:
                    g_f_BaseWidth -= 1.0f;
                    if (g_f_BaseWidth < 2.0f) g_f_BaseWidth = 2.0f;
                    std::cout << "Base width: " << g_f_BaseWidth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        GenerateBuilding();
                        UploadMeshToGPU();
                        UploadWindowsToGPU();
                        UploadDoorsToGPU();
                    }
                    break;
                case SDLK_e:
                    g_f_BaseDepth += 1.0f;
                    std::cout << "Base depth: " << g_f_BaseDepth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        GenerateBuilding();
                        UploadMeshToGPU();
                        UploadWindowsToGPU();
                        UploadDoorsToGPU();
                    }
                    break;
                case SDLK_a:
                    g_f_BaseDepth -= 1.0f;
                    if (g_f_BaseDepth < 2.0f) g_f_BaseDepth = 2.0f;
                    std::cout << "Base depth: " << g_f_BaseDepth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        GenerateBuilding();
                        UploadMeshToGPU();
                        UploadWindowsToGPU();
                        UploadDoorsToGPU();
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
                g_b_MousePressed = true;
                g_i_LastMouseX = event.button.x;
                g_i_LastMouseY = event.button.y;
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
    
    if (!g_BuildingMesh.windowWalls.empty() && g_TextureID_Windows != 0) {
        glm::vec3 viewPosWorld = glm::vec3(0.0f, 0.0f, g_f_ZoomDistance);
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_TextureID_Windows);
        glUniform1i(glGetUniformLocation(g_ShaderProgram, "texture1"), 0);
        glUniform1i(glGetUniformLocation(g_ShaderProgram, "useTexture"), 1);
        glUniform1i(glGetUniformLocation(g_ShaderProgram, "isRoof"), 0);
        glUniform1f(glGetUniformLocation(g_ShaderProgram, "roofUVScale"), 1.0f);
        
        for (size_t i = 0; i < g_BuildingMesh.windowWalls.size(); ++i) {
            const auto& windowWall = g_BuildingMesh.windowWalls[i];
            
            glm::vec3 wallCenterWorld = glm::vec3(model * glm::vec4(windowWall.wallCenter, 1.0f));
            glm::vec3 wallNormalWorld = glm::normalize(normalMatrix * windowWall.wallNormal);
            glm::vec3 viewDir = glm::normalize(viewPosWorld - wallCenterWorld);
            float dotProduct = glm::dot(wallNormalWorld, viewDir);
            
            if (dotProduct > 0.0f) {
                glBindVertexArray(g_VAO_Windows[i]);
                glDrawElements(GL_TRIANGLES, windowWall.indices.size(), GL_UNSIGNED_INT, 0);
            }
        }
        
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    if (!g_BuildingMesh.doors.empty() && g_TextureID_Door != 0) {
        glm::vec3 viewPosWorld = glm::vec3(0.0f, 0.0f, g_f_ZoomDistance);
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_TextureID_Door);
        glUniform1i(glGetUniformLocation(g_ShaderProgram, "texture1"), 0);
        glUniform1i(glGetUniformLocation(g_ShaderProgram, "useTexture"), 1);
        glUniform1i(glGetUniformLocation(g_ShaderProgram, "isRoof"), 0);
        glUniform1f(glGetUniformLocation(g_ShaderProgram, "roofUVScale"), 1.0f);

        for (size_t i = 0; i < g_BuildingMesh.doors.size(); ++i) {
            const auto& door = g_BuildingMesh.doors[i];
            glm::vec3 doorCenterWorld = glm::vec3(model * glm::vec4(door.wallCenter, 1.0f));
            glm::vec3 doorNormalWorld = glm::normalize(normalMatrix * door.wallNormal);
            glm::vec3 viewDir = glm::normalize(viewPosWorld - doorCenterWorld);
            float dotProduct = glm::dot(doorNormalWorld, viewDir);
            if (dotProduct > 0.0f) {
                glBindVertexArray(g_VAO_Doors[i]);
                glDrawElements(GL_TRIANGLES, door.indices.size(), GL_UNSIGNED_INT, 0);
            }
        }

        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    SDL_GL_SwapWindow(g_p_Window);
}

// ===== CLEANUP =====
void Cleanup() {
    if (g_VAO) glDeleteVertexArrays(1, &g_VAO);
    if (g_VBO) glDeleteBuffers(1, &g_VBO);
    if (g_VBO_Normal) glDeleteBuffers(1, &g_VBO_Normal);
    if (g_VBO_TexCoord) glDeleteBuffers(1, &g_VBO_TexCoord);
    if (g_EBO) glDeleteBuffers(1, &g_EBO);
    if (g_TextureID) glDeleteTextures(1, &g_TextureID);
    
    for (auto vao : g_VAO_Windows) glDeleteVertexArrays(1, &vao);
    for (auto vbo : g_VBO_Windows) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Windows_Normal) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Windows_TexCoord) glDeleteBuffers(1, &vbo);
    for (auto ebo : g_EBO_Windows) glDeleteBuffers(1, &ebo);
    if (g_TextureID_Windows) glDeleteTextures(1, &g_TextureID_Windows);
    if (g_TextureID_Roof) glDeleteTextures(1, &g_TextureID_Roof);
    for (auto vao : g_VAO_Doors) glDeleteVertexArrays(1, &vao);
    for (auto vbo : g_VBO_Doors) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Doors_Normal) glDeleteBuffers(1, &vbo);
    for (auto vbo : g_VBO_Doors_TexCoord) glDeleteBuffers(1, &vbo);
    for (auto ebo : g_EBO_Doors) glDeleteBuffers(1, &ebo);
    if (g_TextureID_Door) glDeleteTextures(1, &g_TextureID_Door);
    
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
    
    g_TextureID = LoadTexture("assets/textures/Plaster002_2K-JPG_Color.jpg");
    g_TextureID_Roof = LoadTexture("assets/textures/210_clay roof texture seamless.jpg");
    g_TextureID_Windows = LoadTexture("assets/textures/71_glass building windows texture.png");
    g_TextureID_Door = LoadTexture("assets/textures/8_classic door.png");
    
    GenerateBuilding();
    UploadMeshToGPU();
    UploadWindowsToGPU();
    UploadDoorsToGPU();

    while (!g_b_ShouldClose) {
        HandleEvents();
        Render();
        SDL_Delay(16);
    }

    Cleanup();
    
    std::cout << "Program exited successfully." << std::endl;
    return 0;
}
