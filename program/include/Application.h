#ifndef SCOTLANDYARD_CORE_APPLICATION_H
#define SCOTLANDYARD_CORE_APPLICATION_H

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <string>
#include <memory>
#include <map>

namespace ScotlandYard {
namespace Core {

struct Character {
    GLuint m_TextureID;
    int m_i_Width;
    int m_i_Height;
    int m_i_BearingX;
    int m_i_BearingY;
    int m_i_Advance;
};

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
    StateManager* GetStateManager() const { return m_p_StateManager.get(); }

    const std::map<char, Character>& GetCharacterMap() const { return m_map_Characters; }
    GLuint GetTextShaderProgram() const { return m_ShaderProgram_Text; }
    GLuint GetTextVAO() const { return m_VAO_Text; }
    GLuint GetTextVBO() const { return m_VBO_Text; }

    GLuint LoadTexture(const std::string& s_Path);
    void UnloadTexture(GLuint textureID);
    std::string GetAssetPath(const std::string& s_RelativePath) const;

    GLuint GetHUDRoundedShader() const { return m_ShaderProgram_HUDRounded; }
    GLuint GetHUDTextureShader() const { return m_ShaderProgram_HUDTexture; }
    GLuint GetHUDRoundedVAO() const { return m_VAO_HUDRounded; }
    GLuint GetHUDRoundedVBO() const { return m_VBO_HUDRounded; }
    GLuint GetHUDTextureVAO() const { return m_VAO_HUDTexture; }
    GLuint GetHUDTextureVBO() const { return m_VBO_HUDTexture; }

private:
    void HandleEvents();
    void Update(float deltaTime);
    void Render();

    bool InitializeFreeType();
    void ShutdownFreeType();
    bool InitializeHUDResources();
    void ShutdownHUDResources();

private:
    std::string m_s_Title;
    int m_i_Width;
    int m_i_Height;

    SDL_Window* m_p_Window;
    SDL_GLContext m_gl_Context;

    std::unique_ptr<StateManager> m_p_StateManager;

    bool m_b_Running;
    bool m_b_Initialized;
    bool m_b_TrainingMode;

    float m_f_DeltaTime;
    Uint64 m_u64_LastFrameTime;

    std::map<char, Character> m_map_Characters;
    GLuint m_ShaderProgram_Text;
    GLuint m_VAO_Text;
    GLuint m_VBO_Text;

    std::map<std::string, GLuint> m_map_TextureCache;

    GLuint m_ShaderProgram_HUDRounded;
    GLuint m_ShaderProgram_HUDTexture;
    GLuint m_VAO_HUDRounded;
    GLuint m_VBO_HUDRounded;
    GLuint m_VAO_HUDTexture;
    GLuint m_VBO_HUDTexture;
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_APPLICATION_H
