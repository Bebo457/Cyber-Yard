#include "Application.h"
#include "StateManager.h"
#include "MenuState.h"
#include "GameState.h"
#include <GL/glew.h>
#include <iostream>

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
        m_p_StateManager->HandleEvent(event);
    }
}

void Application::Update(float deltaTime) {
    m_p_StateManager->Update(deltaTime);
}

void Application::Render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_p_StateManager->Render();
    SDL_GL_SwapWindow(m_p_Window);
}

void Application::Shutdown() {
    if (!m_b_Initialized) return;

    m_p_StateManager.reset();

    if (!m_b_TrainingMode) {
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
