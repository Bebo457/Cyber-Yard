#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

#include "BuildingGenerator.h"

using namespace ScotlandYard::Core;

// ===== SHADER SOURCE CODE =====
const char* k_VertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalize(mat3(transpose(inverse(model))) * aNormal);
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* k_FragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

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
        
    vec3 result = (ambient + diffuse + specular) * objColor;
    FragColor = vec4(result, 1.0);
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
GLuint g_EBO = 0;

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
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &p_Source, nullptr);
    glCompileShader(shader);

    int i_Success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }
    return shader;
}

GLuint CreateShaderProgram() {
    GLuint vertexShader = CompileShader(k_VertexShaderSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = CompileShader(k_FragmentShaderSource, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int i_Success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &i_Success);
    if (!i_Success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
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
    if (g_VAO) glDeleteVertexArrays(1, &g_VAO);
    if (g_VBO) glDeleteBuffers(1, &g_VBO);
    if (g_VBO_Normal) glDeleteBuffers(1, &g_VBO_Normal);
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
    
    glGenBuffers(1, &g_EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 g_BuildingMesh.indices.size() * sizeof(unsigned int),
                 g_BuildingMesh.indices.data(),
                 GL_STATIC_DRAW);
    
    glBindVertexArray(0);
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
                    break;
                case SDLK_1:
                    g_i_BuildingExample = 0;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    break;
                case SDLK_2:
                    g_i_BuildingExample = 1;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    break;
                case SDLK_3:
                    g_i_BuildingExample = 2;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    break;
                case SDLK_4:
                    g_i_BuildingExample = 3;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    break;
                case SDLK_h:
                    g_f_BuildingHeight += 1.0f;
                    std::cout << "Building height: " << g_f_BuildingHeight << std::endl;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    break;
                case SDLK_g:
                    g_f_BuildingHeight -= 1.0f;
                    if (g_f_BuildingHeight < 1.0f) g_f_BuildingHeight = 1.0f;
                    std::cout << "Building height: " << g_f_BuildingHeight << std::endl;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    break;
                case SDLK_r:
                    g_f_RoofHeight += 0.5f;
                    std::cout << "Roof height: " << g_f_RoofHeight << std::endl;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    break;
                case SDLK_f:
                    g_f_RoofHeight -= 0.5f;
                    if (g_f_RoofHeight < 0.5f) g_f_RoofHeight = 0.5f;
                    std::cout << "Roof height: " << g_f_RoofHeight << std::endl;
                    GenerateBuilding();
                    UploadMeshToGPU();
                    break;
                case SDLK_w:
                    g_f_BaseWidth += 1.0f;
                    std::cout << "Base width: " << g_f_BaseWidth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        GenerateBuilding();
                        UploadMeshToGPU();
                    }
                    break;
                case SDLK_q:
                    g_f_BaseWidth -= 1.0f;
                    if (g_f_BaseWidth < 2.0f) g_f_BaseWidth = 2.0f;
                    std::cout << "Base width: " << g_f_BaseWidth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        GenerateBuilding();
                        UploadMeshToGPU();
                    }
                    break;
                case SDLK_e:
                    g_f_BaseDepth += 1.0f;
                    std::cout << "Base depth: " << g_f_BaseDepth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        GenerateBuilding();
                        UploadMeshToGPU();
                    }
                    break;
                case SDLK_a:
                    g_f_BaseDepth -= 1.0f;
                    if (g_f_BaseDepth < 2.0f) g_f_BaseDepth = 2.0f;
                    std::cout << "Base depth: " << g_f_BaseDepth << std::endl;
                    if (g_i_BuildingExample == 3) {
                        GenerateBuilding();
                        UploadMeshToGPU();
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
    
    glBindVertexArray(g_VAO);
    
    for (const auto& material : g_BuildingMesh.materials) {
        glUniform3fv(glGetUniformLocation(g_ShaderProgram, "objColor"), 1, glm::value_ptr(material.color));
        glDrawElements(GL_TRIANGLES, material.indexCount, GL_UNSIGNED_INT, 
                      (void*)(material.firstIndex * sizeof(unsigned int)));
    }

    glBindVertexArray(0);

    SDL_GL_SwapWindow(g_p_Window);
}

// ===== CLEANUP =====
void Cleanup() {
    if (g_VAO) glDeleteVertexArrays(1, &g_VAO);
    if (g_VBO) glDeleteBuffers(1, &g_VBO);
    if (g_VBO_Normal) glDeleteBuffers(1, &g_VBO_Normal);
    if (g_EBO) glDeleteBuffers(1, &g_EBO);
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
    
    GenerateBuilding();
    UploadMeshToGPU();

    while (!g_b_ShouldClose) {
        HandleEvents();
        Render();
        SDL_Delay(16);
    }

    Cleanup();
    
    std::cout << "Program exited successfully." << std::endl;
    return 0;
}
