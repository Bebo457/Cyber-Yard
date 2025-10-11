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
    // Request 4x MSAA for smoother edges
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    // Request sRGB-capable framebuffer for correct gamma on colors
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
    // Enable multisampling and sRGB framebuffer if supported
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
    // Use trilinear filtering with mipmaps for smoother scaling
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

void Application::Shutdown() {
    if (!m_b_Initialized) return;

    m_p_StateManager.reset();

    if (!m_b_TrainingMode) {
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
