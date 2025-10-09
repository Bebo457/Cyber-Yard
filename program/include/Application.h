#ifndef SCOTLANDYARD_CORE_APPLICATION_H
#define SCOTLANDYARD_CORE_APPLICATION_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <memory>

namespace ScotlandYard {
namespace Core {

class StateManager;

class Application {
public:
    Application(const std::string& title, int width, int height, bool trainingMode = false);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize();
    void LoadStates();
    void Run();
    void RunTraining(int maxSteps = 10000);
    void Shutdown();

    void RequestExit() { m_b_Running = false; }
    int GetWidth() const { return m_i_Width; }
    int GetHeight() const { return m_i_Height; }
    float GetDeltaTime() const { return m_f_DeltaTime; }
    bool IsTrainingMode() const { return m_b_TrainingMode; }
    SDL_Renderer* GetRenderer() const { return m_p_Renderer; }
    TTF_Font* GetFont() const { return m_p_Font; }
    StateManager* GetStateManager() const { return m_p_StateManager.get(); }

private:
    void HandleEvents();
    void Update(float deltaTime);
    void Render();
    std::string FindSystemFont();

private:
    std::string m_s_Title;
    int m_i_Width;
    int m_i_Height;

    SDL_Window* m_p_Window;
    SDL_GLContext m_gl_Context;
    SDL_Renderer* m_p_Renderer;
    TTF_Font* m_p_Font;

    std::unique_ptr<StateManager> m_p_StateManager;

    bool m_b_Running;
    bool m_b_Initialized;
    bool m_b_TrainingMode;

    float m_f_DeltaTime;
    Uint64 m_u64_LastFrameTime;
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_APPLICATION_H
