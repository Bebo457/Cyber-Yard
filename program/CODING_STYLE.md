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

```cpp
// Submit async task
auto future = Threading::ThreadPool::Submit([]() {
    return ExpensiveCalculation();
});

// Thread-safe counter
std::atomic<int> m_i_Count{0};

// Protect data with mutex
std::lock_guard<std::mutex> lock(m_mtx_Data);
m_vec_Data.push_back(value);
```

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