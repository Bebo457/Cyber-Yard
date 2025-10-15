#include "Application.h"
#include "StateManager.h"
#include "MenuState.h"
#include "GameState.h"
#include <GL/glew.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <filesystem>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION_APP
#include "../external/stb_image.h"

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#endif

namespace ScotlandYard {
namespace Core {

Application::Application(const std::string& title, int width, int height, bool trainingMode)
    : m_s_Title(title)
    , m_i_Width(width)
    , m_i_Height(height)
    , m_p_Window(nullptr)
    , m_gl_Context(nullptr)
    , m_b_Running(false)
    , m_b_Initialized(false)
    , m_b_TrainingMode(trainingMode)
    , m_f_DeltaTime(0.0f)
    , m_u64_LastFrameTime(0)
    , m_ShaderProgram_Text(0)
    , m_VAO_Text(0)
    , m_VBO_Text(0)
    , m_ShaderProgram_HUDRounded(0)
    , m_ShaderProgram_HUDTexture(0)
    , m_VAO_HUDRounded(0)
    , m_VBO_HUDRounded(0)
    , m_VAO_HUDTexture(0)
    , m_VBO_HUDTexture(0)
{
}

Application::~Application() {
    if (m_b_Initialized) {
        Shutdown();
    }
}

bool Application::Initialize() {
    if (m_b_TrainingMode) {
        m_p_StateManager = std::make_unique<StateManager>();
        m_b_Initialized = true;
        return true;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    m_p_Window = SDL_CreateWindow(
        m_s_Title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_i_Width,
        m_i_Height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!m_p_Window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    m_gl_Context = SDL_GL_CreateContext(m_p_Window);
    if (!m_gl_Context) {
        std::cerr << "OpenGL context creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_p_Window);
        SDL_Quit();
        return false;
    }

    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "GLEW initialization failed: " << glewGetErrorString(glewError) << std::endl;
        SDL_GL_DeleteContext(m_gl_Context);
        SDL_DestroyWindow(m_p_Window);
        SDL_Quit();
        return false;
    }

    SDL_GL_SetSwapInterval(1);
    glViewport(0, 0, m_i_Width, m_i_Height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_FRAMEBUFFER_SRGB);

    //FreeType text rendering
    if (!InitializeFreeType()) {
        std::cerr << "FreeType initialization failed" << std::endl;
        SDL_GL_DeleteContext(m_gl_Context);
        SDL_DestroyWindow(m_p_Window);
        SDL_Quit();
        return false;
    }

    // HUD resources
    if (!InitializeHUDResources()) {
        std::cerr << "HUD resources initialization failed" << std::endl;
        ShutdownFreeType();
        SDL_GL_DeleteContext(m_gl_Context);
        SDL_DestroyWindow(m_p_Window);
        SDL_Quit();
        return false;
    }

    m_p_StateManager = std::make_unique<StateManager>();
    m_b_Initialized = true;
    return true;
}

void Application::LoadStates() {
    m_p_StateManager->RegisterState("menu", std::make_unique<States::MenuState>());
    m_p_StateManager->RegisterState("game", std::make_unique<States::GameState>());
    m_p_StateManager->ChangeState("menu");
}

void Application::Run() {
    m_b_Running = true;
    m_u64_LastFrameTime = SDL_GetPerformanceCounter();

    while (m_b_Running && !m_p_StateManager->IsEmpty()) {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        m_f_DeltaTime = (float)(currentTime - m_u64_LastFrameTime) / (float)SDL_GetPerformanceFrequency();
        m_u64_LastFrameTime = currentTime;

        if (m_f_DeltaTime > 0.1f) {
            m_f_DeltaTime = 0.1f;
        }

        HandleEvents();
        Update(m_f_DeltaTime);
        Render();
    }
}

void Application::RunTraining(int maxSteps) {
    const float k_FixedDt = 1.0f / 60.0f;
    m_b_Running = true;

    for (int i = 0; i < maxSteps && m_b_Running && !m_p_StateManager->IsEmpty(); ++i) {
        Update(k_FixedDt);
    }
}

void Application::HandleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_b_Running = false;
        }
        m_p_StateManager->HandleEvent(event, this);
    }
}

void Application::Update(float f_DeltaTime) {
    m_p_StateManager->Update(f_DeltaTime);
}

void Application::Render() {
    if (m_p_StateManager) {
        m_p_StateManager->Render(this);
    }
}

bool Application::InitializeFreeType() {
    const char* p_VertexShaderSrc = R"(
        #version 330 core
        layout (location = 0) in vec4 vertex;
        out vec2 TexCoords;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    const char* p_FragmentShaderSrc = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 color;
        uniform sampler2D text;
        uniform vec3 textColor;
        void main() {
            vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
            color = vec4(textColor, 1.0) * sampled;
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &p_VertexShaderSrc, nullptr);
    glCompileShader(vertexShader);

    GLint i_Success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Text vertex shader compilation failed: " << infoLog << std::endl;
        return false;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &p_FragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Text fragment shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return false;
    }

    m_ShaderProgram_Text = glCreateProgram();
    glAttachShader(m_ShaderProgram_Text, vertexShader);
    glAttachShader(m_ShaderProgram_Text, fragmentShader);
    glLinkProgram(m_ShaderProgram_Text);

    glGetProgramiv(m_ShaderProgram_Text, GL_LINK_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ShaderProgram_Text, 512, nullptr, infoLog);
        std::cerr << "Text shader program linking failed: " << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Create VAO and VBO for text rendering
    glGenVertexArrays(1, &m_VAO_Text);
    glGenBuffers(1, &m_VBO_Text);
    glBindVertexArray(m_VAO_Text);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Text);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Initialize FreeType and load font
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Could not init FreeType Library" << std::endl;
        return false;
    }

    FT_Face face;
    bool b_FontLoaded = false;

#ifdef _WIN32
    if (FT_New_Face(ft, "C:\\Windows\\Fonts\\arial.ttf", 0, &face) == 0) {
        b_FontLoaded = true;
    }
#elif __linux__
    if (FT_New_Face(ft, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &face) == 0) {
        b_FontLoaded = true;
    }
#elif __APPLE__
    if (FT_New_Face(ft, "/System/Library/Fonts/Arial.ttf", 0, &face) == 0) {
        b_FontLoaded = true;
    }
#endif

    if (!b_FontLoaded) {
        std::cerr << "Failed to load system font" << std::endl;
        FT_Done_FreeType(ft);
        return false;
    }

    // Use larger glyphs for better quality when scaling up in UI
    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load ASCII character set
    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "Failed to load Glyph: " << c << std::endl;
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            static_cast<int>(face->glyph->bitmap.width),
            static_cast<int>(face->glyph->bitmap.rows),
            face->glyph->bitmap_left,
            face->glyph->bitmap_top,
            static_cast<int>(face->glyph->advance.x)
        };
        m_map_Characters.insert(std::pair<char, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    return true;
}

void Application::ShutdownFreeType() {
    for (auto& pair : m_map_Characters) {
        glDeleteTextures(1, &pair.second.m_TextureID);
    }
    m_map_Characters.clear();

    if (m_VAO_Text) {
        glDeleteVertexArrays(1, &m_VAO_Text);
        m_VAO_Text = 0;
    }
    if (m_VBO_Text) {
        glDeleteBuffers(1, &m_VBO_Text);
        m_VBO_Text = 0;
    }

    if (m_ShaderProgram_Text) {
        glDeleteProgram(m_ShaderProgram_Text);
        m_ShaderProgram_Text = 0;
    }
}

bool Application::InitializeHUDResources() {
    // HUD Rounded Rectangle Shader
    const char* VS_R = R"(#version 330 core
        layout(location=0) in vec2 aPos;
        out vec2 vPos;
        void main(){ vPos=aPos; gl_Position=vec4(aPos,0.0,1.0); })";

    const char* FS_R = R"(#version 330 core
        in vec2 vPos;
        uniform vec4 uRect;
        uniform float uRadius;
        uniform vec4 uColor;
        out vec4 FragColor;
        float sdRoundBox(in vec2 p, in vec2 b, in float r){
            vec2 d = abs(p) - b + vec2(r);
            return length(max(d,0.0)) - r;
        }
        void main(){
            vec2 c  = 0.5*(uRect.xy + uRect.zw);
            vec2 hs = 0.5*vec2(uRect.z - uRect.x, uRect.w - uRect.y);
            vec2 lp = vPos - c;
            float d = sdRoundBox(lp, hs, uRadius);
            float aa = fwidth(d) * 1.5;
            float alpha = 1.0 - smoothstep(0.0, aa, d);
            vec4 col = vec4(uColor.rgb, uColor.a*alpha);
            if (col.a <= 0.001) discard;
            FragColor = col;
        })";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VS_R, nullptr);
    glCompileShader(vs);

    GLint i_Success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        std::cerr << "HUD rounded vertex shader compilation failed: " << infoLog << std::endl;
        return false;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FS_R, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        std::cerr << "HUD rounded fragment shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(vs);
        return false;
    }

    m_ShaderProgram_HUDRounded = glCreateProgram();
    glAttachShader(m_ShaderProgram_HUDRounded, vs);
    glAttachShader(m_ShaderProgram_HUDRounded, fs);
    glLinkProgram(m_ShaderProgram_HUDRounded);

    glGetProgramiv(m_ShaderProgram_HUDRounded, GL_LINK_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ShaderProgram_HUDRounded, 512, nullptr, infoLog);
        std::cerr << "HUD rounded shader program linking failed: " << infoLog << std::endl;
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // HUD Texture Shader
    const char* VS_TEX = R"(#version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aUV;
        out vec2 vUV;
        void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); })";

    const char* FS_TEX = R"(#version 330 core
        in vec2 vUV; uniform sampler2D uTex; uniform vec4 uColor;
        out vec4 FragColor;
        void main(){ vec4 t = texture(uTex, vUV); FragColor = vec4(uColor.rgb, uColor.a) * t; })";

    vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VS_TEX, nullptr);
    glCompileShader(vs);

    glGetShaderiv(vs, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        std::cerr << "HUD texture vertex shader compilation failed: " << infoLog << std::endl;
        return false;
    }

    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FS_TEX, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        std::cerr << "HUD texture fragment shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(vs);
        return false;
    }

    m_ShaderProgram_HUDTexture = glCreateProgram();
    glAttachShader(m_ShaderProgram_HUDTexture, vs);
    glAttachShader(m_ShaderProgram_HUDTexture, fs);
    glLinkProgram(m_ShaderProgram_HUDTexture);

    glGetProgramiv(m_ShaderProgram_HUDTexture, GL_LINK_STATUS, &i_Success);
    if (!i_Success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ShaderProgram_HUDTexture, 512, nullptr, infoLog);
        std::cerr << "HUD texture shader program linking failed: " << infoLog << std::endl;
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // VAO/VBO for rounded-rect
    glGenVertexArrays(1, &m_VAO_HUDRounded);
    glGenBuffers(1, &m_VBO_HUDRounded);
    glBindVertexArray(m_VAO_HUDRounded);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_HUDRounded);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // VAO/VBO for texture
    glGenVertexArrays(1, &m_VAO_HUDTexture);
    glGenBuffers(1, &m_VBO_HUDTexture);
    glBindVertexArray(m_VAO_HUDTexture);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_HUDTexture);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void Application::ShutdownHUDResources() {
    if (m_VBO_HUDRounded) {
        glDeleteBuffers(1, &m_VBO_HUDRounded);
        m_VBO_HUDRounded = 0;
    }
    if (m_VAO_HUDRounded) {
        glDeleteVertexArrays(1, &m_VAO_HUDRounded);
        m_VAO_HUDRounded = 0;
    }
    if (m_VBO_HUDTexture) {
        glDeleteBuffers(1, &m_VBO_HUDTexture);
        m_VBO_HUDTexture = 0;
    }
    if (m_VAO_HUDTexture) {
        glDeleteVertexArrays(1, &m_VAO_HUDTexture);
        m_VAO_HUDTexture = 0;
    }
    if (m_ShaderProgram_HUDRounded) {
        glDeleteProgram(m_ShaderProgram_HUDRounded);
        m_ShaderProgram_HUDRounded = 0;
    }
    if (m_ShaderProgram_HUDTexture) {
        glDeleteProgram(m_ShaderProgram_HUDTexture);
        m_ShaderProgram_HUDTexture = 0;
    }
}

GLuint Application::LoadTexture(const std::string& s_Path) {
    // Check cache first
    auto it = m_map_TextureCache.find(s_Path);
    if (it != m_map_TextureCache.end()) {
        return it->second;
    }

    // Load using stbi_load
    int i_Width, i_Height, i_Channels;
    unsigned char* p_Data = stbi_load(s_Path.c_str(), &i_Width, &i_Height, &i_Channels, 0);
    if (!p_Data) {
        std::cerr << "[Application] Failed to load texture: " << s_Path << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLenum format = (i_Channels == 4) ? GL_RGBA : ((i_Channels == 3) ? GL_RGB : GL_RED);
    glTexImage2D(GL_TEXTURE_2D, 0, format, i_Width, i_Height, 0, format, GL_UNSIGNED_BYTE, p_Data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(p_Data);

    m_map_TextureCache[s_Path] = textureID;
    return textureID;
}

void Application::UnloadTexture(GLuint textureID) {
    if (textureID == 0) return;

    // Find and remove from cache
    for (auto it = m_map_TextureCache.begin(); it != m_map_TextureCache.end(); ++it) {
        if (it->second == textureID) {
            m_map_TextureCache.erase(it);
            break;
        }
    }

    glDeleteTextures(1, &textureID);
}

std::string Application::GetAssetPath(const std::string& s_RelativePath) const {
    #ifdef ASSETS_DIR
        return std::string(ASSETS_DIR) + "/" + s_RelativePath;
    #else
        return "assets/" + s_RelativePath;
    #endif
}

void Application::Shutdown() {
    if (!m_b_Initialized) return;

    m_p_StateManager.reset();

    if (!m_b_TrainingMode) {
        // Clean up texture cache
        for (auto& pair : m_map_TextureCache) {
            glDeleteTextures(1, &pair.second);
        }
        m_map_TextureCache.clear();

        ShutdownHUDResources();
        ShutdownFreeType();

        if (m_gl_Context) {
            SDL_GL_DeleteContext(m_gl_Context);
            m_gl_Context = nullptr;
        }

        if (m_p_Window) {
            SDL_DestroyWindow(m_p_Window);
            m_p_Window = nullptr;
        }

        SDL_Quit();
    }

    m_b_Initialized = false;
}

} // namespace Core
} // namespace ScotlandYard
