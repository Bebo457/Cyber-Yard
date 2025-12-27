#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <ctime>

#include "TreeGenerator.h"   // <- Twoje nowe pliki

using namespace ScotlandYard::Core;

// ===== Simple shaders (Phong + texture) =====
static const char* kVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

out vec3 vPos;
out vec3 vNormal;
out vec2 vUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main() {
    vPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vUV = aUV;
    gl_Position = uProj * uView * vec4(vPos, 1.0);
}
)";

static const char* kFS = R"(
#version 330 core
out vec4 FragColor;

in vec3 vPos;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uTex;
uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uLightColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vPos);
    vec3 V = normalize(uViewPos - vPos);

    float diff = max(dot(N, L), 0.0);

    // simple specular
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), 32.0);

    vec3 tex = texture(uTex, vUV).rgb;

    vec3 ambient = 0.25 * uLightColor * tex;
    vec3 diffuse = diff * uLightColor * tex;
    vec3 specular = 0.25 * spec * uLightColor;

    vec3 col = ambient + diffuse + specular;
    FragColor = vec4(col, 1.0);
}
)";

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint CreateProgram(const char* vs, const char* fs) {
    GLuint v = CompileShader(GL_VERTEX_SHADER, vs);
    GLuint f = CompileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return 0;

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static GLuint Create1x1Texture(unsigned char r, unsigned char g, unsigned char b) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char px[3] = { r, g, b };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, px);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

struct GLMesh {
    GLuint vao = 0, vboPos = 0, vboNrm = 0, vboUV = 0, ebo = 0;
    size_t indexCount = 0;
};

static void UploadMesh(const TreeMesh& m, GLMesh& out) {
    if (out.vao == 0) glGenVertexArrays(1, &out.vao);
    glBindVertexArray(out.vao);

    // positions
    if (out.vboPos == 0) glGenBuffers(1, &out.vboPos);
    glBindBuffer(GL_ARRAY_BUFFER, out.vboPos);
    glBufferData(GL_ARRAY_BUFFER, m.vertices.size() * sizeof(glm::vec3), m.vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    // normals
    if (out.vboNrm == 0) glGenBuffers(1, &out.vboNrm);
    glBindBuffer(GL_ARRAY_BUFFER, out.vboNrm);
    glBufferData(GL_ARRAY_BUFFER, m.normals.size() * sizeof(glm::vec3), m.normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);

    // uvs
    if (out.vboUV == 0) glGenBuffers(1, &out.vboUV);
    glBindBuffer(GL_ARRAY_BUFFER, out.vboUV);
    glBufferData(GL_ARRAY_BUFFER, m.texCoords.size() * sizeof(glm::vec2), m.texCoords.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(2);

    // indices
    if (out.ebo == 0) glGenBuffers(1, &out.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(unsigned int), m.indices.data(), GL_STATIC_DRAW);

    out.indexCount = m.indices.size();

    glBindVertexArray(0);
}

static void DrawMaterialGroup(const GLMesh& glmesh, const TreeMesh& mesh,
    int matIndex, GLuint tex, GLuint program)
{
    const auto& mg = mesh.materials.at(matIndex);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(program, "uTex"), 0);

    glBindVertexArray(glmesh.vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)mg.indexCount, GL_UNSIGNED_INT, (void*)(mg.firstIndex * sizeof(unsigned int)));
    glBindVertexArray(0);
}

int main(int argc, char** argv)
{
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed\n";
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* win = SDL_CreateWindow("Tree Visualizer Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!win) {
        std::cerr << "SDL_CreateWindow failed\n";
        SDL_Quit();
        return 1;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "glewInit failed\n";
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    GLuint program = CreateProgram(kVS, kFS);
    if (!program) {
        std::cerr << "Shader program creation failed\n";
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // Create simple 1x1 textures (no asset pipeline needed)
    GLuint texTrunk = Create1x1Texture(110, 78, 50);
    GLuint texCrown = Create1x1Texture(50, 140, 60);

    // Generate a tree mesh
    TreeParams tp;
    tp.trunkSegments = 20;
    tp.crownSegments = 20;
    tp.crownSmallSpheres = 5;

    unsigned int seed = (unsigned int)std::time(nullptr);
    TreeMesh tree = TreeGenerator::GenerateTree(tp, seed);

    GLMesh glTree;
    UploadMesh(tree, glTree);

    // camera controls
    bool running = true;
    bool dragging = false;
    int lastX = 0, lastY = 0;
    float rotX = -25.0f;
    float rotY = 35.0f;
    float dist = 8.0f;

    int w = 1280, h = 720;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                w = e.window.data1;
                h = e.window.data2;
                glViewport(0, 0, w, h);
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                dragging = true;
                lastX = e.button.x;
                lastY = e.button.y;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                dragging = false;
            }
            if (e.type == SDL_MOUSEMOTION && dragging) {
                int dx = e.motion.x - lastX;
                int dy = e.motion.y - lastY;
                lastX = e.motion.x;
                lastY = e.motion.y;
                rotY += dx * 0.3f;
                rotX += dy * 0.3f;
            }
            if (e.type == SDL_MOUSEWHEEL) {
                dist -= e.wheel.y * 0.4f;
                dist = std::max(2.5f, std::min(dist, 25.0f));
            }

            // Press R to regenerate tree (different random)
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r) {
                seed = (unsigned int)std::time(nullptr);
                tree = TreeGenerator::GenerateTree(tp, seed);
                UploadMesh(tree, glTree);
            }
        }

        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        glm::mat4 model(1.0f);
        // keep tree at origin, no scaling

        glm::vec3 camTarget(0.0f, 0.0f, 1.8f);
        glm::mat4 view(1.0f);
        view = glm::translate(view, glm::vec3(0, 0, -dist));
        view = glm::rotate(view, glm::radians(rotX), glm::vec3(1, 0, 0));
        view = glm::rotate(view, glm::radians(rotY), glm::vec3(0, 0, 1));
        view = glm::translate(view, -camTarget);

        glm::mat4 proj = glm::perspective(glm::radians(55.0f), (float)w / (float)h, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(program, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));

        glm::vec3 lightPos(6.0f, 6.0f, 8.0f);
        glm::vec3 viewPos = glm::vec3(glm::inverse(view)[3]); // camera world pos approx
        glUniform3fv(glGetUniformLocation(program, "uLightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(program, "uViewPos"), 1, glm::value_ptr(viewPos));
        glUniform3f(glGetUniformLocation(program, "uLightColor"), 1.0f, 1.0f, 1.0f);

        // Draw trunk and crown by material groups
        // We assume materials[0]=trunk, materials[1]=crown per generator.
        if (tree.materials.size() >= 1) DrawMaterialGroup(glTree, tree, 0, texTrunk, program);
        if (tree.materials.size() >= 2) DrawMaterialGroup(glTree, tree, 1, texCrown, program);

        SDL_GL_SwapWindow(win);
    }

    glDeleteTextures(1, &texTrunk);
    glDeleteTextures(1, &texCrown);

    glDeleteProgram(program);

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
