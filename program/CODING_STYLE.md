# Scotland Yard++ Coding Style

## Naming Conventions

### Variable Prefixes

**Format:** `prefix_type_VariableName`

**Basic types:**
```cpp
int i_PlayerCount = 4;
float f_DeltaTime = 0.016f;
double d_PreciseValue = 1.234;
bool b_IsActive = true;
char c_Initial = 'A';
std::string s_PlayerName = "Alice";
```

**Containers:**
```cpp
std::vector<int> vec_Scores;
std::map<int, Player> map_Players;
std::set<int> set_Visited;
std::queue<Task> queue_Tasks;
```

**Pointers:**
```cpp
Player* p_Player;                      // Non-owning (don't delete!)
std::unique_ptr<Player> u_Player;      // Owns, auto-deletes
std::shared_ptr<Texture> s_Texture;    // Shared ownership
std::weak_ptr<Object> w_Parent;        // Doesn't own
```

**Threading:**
```cpp
std::mutex mtx_Data;
std::condition_variable cv_Signal;
std::thread t_Worker;
std::function<void()> fn_Callback;
```

**Scope prefixes:**
```cpp
int m_i_Health;                           // Member variable
static int s_i_Count;                     // Static variable
thread_local int t_i_ThreadId;            // Thread-local
int g_i_WindowWidth;                      // Global
const int k_MaxPlayers = 8;               // Constant
```

### Functions and Classes

```cpp
void UpdateGameState();
class PlayerController {};
enum class PlayerType { Detective, MrX };  // Enum class
```

**HEAP:**
```cpp
auto u_Player = std::make_unique<Player>();     // Single owner
auto s_Texture = std::make_shared<Texture>();   // Multiple owners
```

## Memory Management

### Smart Pointers

```cpp
// OWNING - Responsible for deletion
std::unique_ptr<Player> u_Player;      // One owner only
std::shared_ptr<Texture> s_Texture;    // Multiple owners

// NON-OWNING - Just looking, don't delete
Player* p_Player;                      // Raw pointer
std::weak_ptr<Object> w_Parent;        // Weak reference

// NEVER do this:
Player* p_Bad = new Player();          // BAD! Use unique_ptr
delete p_Bad;
```

### RAII Pattern

Resources clean up automatically:

```cpp
// GOOD - automatic cleanup
void LoadTexture() {
    auto u_Texture = std::make_unique<Texture>();
    u_Texture->Load("file.png");
    // Auto-deleted here
}

// BAD - manual cleanup
void LoadTexture() {
    Texture* p_Texture = new Texture();  // Easy to leak!
    p_Texture->Load("file.png");
    delete p_Texture;  // Might forget or throw exception
}
```

---

## Code Style

### Comments

```cpp
// Organizational comments OK
// INITIALIZATION
Memory::Initialize();

// Explanatory comments only when needed
if (f_DeltaTime > 0.1f) {  // Cap to prevent spiral of death
    f_DeltaTime = 0.1f;
}

// Don't state the obvious
x++;  // NO COMMENT NEEDED
```

## Threading

### Threading Policy

**Prefer ThreadPool over raw threads:**
- Use `Threading::ThreadPool::Submit()` for short-lived async tasks
- Only use `std::thread` for long-lived dedicated threads (e.g., console input loops)

**Thread Safety Requirements:**
- Shared data between threads MUST be protected
- Use `std::atomic<>` for simple counters/flags
- Use `std::mutex` + `std::lock_guard` for complex data structures
- Document which mutex protects which data

**Naming Convention:**
```cpp
std::atomic<int> m_i_RoundNumber{1};     // Atomic for simple types
std::mutex m_mtx_GameState;              // Mutex protecting game state
std::mutex m_mtx_Players;                // Mutex protecting player data
```

**Examples:**
```cpp
// GOOD: Using ThreadPool for short task
auto future = Threading::ThreadPool::Submit([]() {
    return ExpensiveCalculation();
});

// GOOD: Atomic for simple counter
std::atomic<int> m_i_Count{0};
m_i_Count.store(m_i_Count.load() + 1);

// GOOD: Mutex for complex data
{
    std::lock_guard<std::mutex> lock(m_mtx_Players);
    m_vec_Players[idx].MoveTo(newNode);
}

// BAD: Accessing shared data without protection
m_i_NonAtomicCounter++;  // Race condition if accessed from multiple threads!
```

**Critical Rules:**
1. **Never mix locking orders** - Always acquire mutexes in the same order to prevent deadlocks
2. **Keep critical sections small** - Lock, modify, unlock quickly
3. **Document thread ownership** - Comment which thread accesses which data

---

## Text Rendering Guide

### Overview

Text rendering is centralized in the `Application` class using FreeType. All states can access shared font resources via Application getters.

### Architecture

```
Application (owns resources):
├─ FreeType initialization
├─ Character map (ASCII 0-127)
├─ Text shader program
└─ VAO/VBO for text quads

States (use resources):
└─ Get resources via p_App->GetCharacterMap(), GetTextShaderProgram(), etc.
```

### Adding Text to Your State

#### Step 1: Create RenderText Helper Method

Add to your state's header file:

```cpp
class MyState : public Core::IGameState {
private:
    void RenderText(const std::string& s_Text, float f_X, float f_Y,
                    float f_Scale, float f_R, float f_G, float f_B,
                    Core::Application* p_App);
};
```

#### Step 2: Implement RenderText in Your .cpp

```cpp
void MyState::RenderText(const std::string& s_Text, float f_X, float f_Y,
                         float f_Scale, float f_R, float f_G, float f_B,
                         Core::Application* p_App) {
    // Get shared resources from Application
    const auto& characters = p_App->GetCharacterMap();
    GLuint shaderProgram = p_App->GetTextShaderProgram();
    GLuint vao = p_App->GetTextVAO();
    GLuint vbo = p_App->GetTextVBO();

    // Set up shader
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), f_R, f_G, f_B);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);

    // Render each character
    for (auto c : s_Text) {
        auto it = characters.find(c);
        if (it == characters.end()) continue;

        const Core::Character& ch = it->second;

        float f_Xpos = f_X + ch.m_i_BearingX * f_Scale;
        float f_Ypos = f_Y - (ch.m_i_Height - ch.m_i_BearingY) * f_Scale;
        float f_W = ch.m_i_Width * f_Scale;
        float f_H = ch.m_i_Height * f_Scale;

        // Create quad vertices
        float vertices[6][4] = {
            { f_Xpos,       f_Ypos - f_H,   0.0f, 1.0f },
            { f_Xpos,       f_Ypos,         0.0f, 0.0f },
            { f_Xpos + f_W, f_Ypos,         1.0f, 0.0f },
            { f_Xpos,       f_Ypos - f_H,   0.0f, 1.0f },
            { f_Xpos + f_W, f_Ypos,         1.0f, 0.0f },
            { f_Xpos + f_W, f_Ypos - f_H,   1.0f, 1.0f }
        };

        // Render glyph texture
        glBindTexture(GL_TEXTURE_2D, ch.m_TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Advance cursor for next character
        f_X += (ch.m_i_Advance >> 6) * f_Scale;
    }

    // Cleanup
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
```

#### Step 3: Use in Your Render Method

```cpp
void MyState::Render(Core::Application* p_App) {
    int i_WindowWidth = p_App->GetWidth();
    int i_WindowHeight = p_App->GetHeight();

    // Set up projection matrix
    glm::mat4 projection = glm::ortho(0.0f, (float)i_WindowWidth,
                                      0.0f, (float)i_WindowHeight);
    GLuint shaderProgram = p_App->GetTextShaderProgram();
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"),
                       1, GL_FALSE, glm::value_ptr(projection));

    // Enable blending for text transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render text (x, y, scale, r, g, b)
    RenderText("Hello World", 100.0f, 200.0f, 1.0f, 1.0f, 1.0f, 1.0f, p_App);
    RenderText("Score: 42", 100.0f, 150.0f, 0.8f, 0.0f, 1.0f, 0.0f, p_App);

    // Swap buffers
    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}
```

### Helper: Calculate Text Width

Useful for centering text:

```cpp
float MyState::CalculateTextWidth(const std::string& s_Text, float f_Scale,
                                  Core::Application* p_App) {
    const auto& characters = p_App->GetCharacterMap();
    float f_Width = 0.0f;

    for (auto c : s_Text) {
        auto it = characters.find(c);
        if (it != characters.end()) {
            f_Width += (it->second.m_i_Advance >> 6) * f_Scale;
        }
    }

    return f_Width;
}

// Usage: Center text
float f_TextWidth = CalculateTextWidth("MENU", 1.0f, p_App);
float f_X = (i_WindowWidth - f_TextWidth) / 2.0f;
RenderText("MENU", f_X, f_Y, 1.0f, 0.0f, 0.0f, 0.0f, p_App);
```

### Important Notes

**Coordinate system:**
- Origin (0, 0) is **bottom-left** corner
- Y increases upward
- Text baseline is at specified Y coordinate
- Use orthographic projection: `glm::ortho(0, width, 0, height)`

**Character map contains ASCII 0-127:**
- Includes letters (a-z, A-Z)
- Numbers (0-9)
- Common symbols (!, @, #, etc.)
- For extended characters, you'll need to extend Application's font loading

---

## Quick Reference

```cpp
// Variables
int i_Count;
float f_Speed;
std::vector<int> vec_Data;
Player* p_Player;                      // Non-owning
std::unique_ptr<T> u_Object;           // Owning

// Member/Static/Thread-local
int m_i_Health;
static int s_i_InstanceCount;
thread_local int t_i_ThreadId;

// Memory
Player player;                         // Stack
auto u_Player = std::make_unique<Player>();  // Heap
auto s_Player = std::make_shared<Player>();  // Heap shared

// Functions
void UpdateGameState();
class PlayerController {};
```