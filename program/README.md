# Program - Scotland Yard++ Game Engine

This directory contains the game engine and framework code for Scotland Yard++.

---

## Quick Start

### Build

```bash
# From this directory (program/)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Run

```bash
./bin/ScotlandYardPlusPlus     # Linux/macOS
.\bin\Release\ScotlandYardPlusPlus.exe  # Windows
```

### Command Line Arguments
```bash
# Traning mode (no graphics)
./ScotlandYardPlusPlus --training --steps 50000
./ScotlandYardPlusPlus -t --steps 1000
```

---

## Documentation

- **[BUILD.md](BUILD.md)** - Build instructions
- **[CODING_STYLE.md](CODING_STYLE.md)** - Naming conventions and code standards

---

## Directory Structure

```
program/
├── src/                          # Implementation files
│   ├── main.cpp                  # Entry point
│   ├── Application.cpp           # Main application
│   ├── StateManager.cpp          # State management
│   ├── MemoryManager.cpp         # Memory tracking
│   ├── ThreadPool.cpp            # Thread pool
│   ├── NeuralNetworkManager.cpp  # AI integration
│   ├── MenuState.cpp             # Menu state
│   └── GameState.cpp             # Game state
│
├── include/                      # Header files
│   ├── Application.h
│   ├── StateManager.h
│   ├── IGameState.h
│   ├── MemoryManager.h
│   ├── ThreadPool.h
│   ├── NeuralNetworkManager.h
│   ├── MenuState.h
│   └── GameState.h
│
├── CMakeLists.txt                # Build configuration
├── BUILD.md                      # Build instructions
├── CODING_STYLE.md               # Code conventions
└── README.md                     # This file lol
```

---

## Architecture

### Core

#### Application ([Application.h](include/Application.h))
Main application class managing:
- SDL window and OpenGL context
- Main game loop
- Frame timing
- State management coordination

#### State Manager ([StateManager.h](include/StateManager.h))
Stack-based state system:
- Push/pop states (e.g., Menu → Game → Pause)
- State lifecycle (enter/exit/pause/resume)
- Current state update and render

#### Memory Manager ([MemoryManager.h](include/MemoryManager.h))
Tagged memory allocation:
- Track memory by category (Graphics, AI, Game Logic, etc.)
- Memory statistics and leak detection
- Smart pointer helpers

#### Thread Pool ([ThreadPool.h](include/ThreadPool.h))
Worker thread pool:
- Submit async tasks
- Used by AI for inference
- Background operations

#### Neural Network Manager ([NeuralNetworkManager.h](include/NeuralNetworkManager.h))
AI integration (placeholder):
- Load neural network models
- Run inference (sync/async)
- Batch predictions

### Game States

#### Menu State ([MenuState.h](include/MenuState.h))
Main menu:
- New Game / Load Game / Settings / Exit
- Entry point after startup

#### Game State ([GameState.h](include/GameState.h))
Active gameplay:
- Game logic updates
- Map rendering
- Player input handling

---

## Building

See [BUILD.md](BUILD.md) for detailed instructions.

### Quick Build Commands

**Windows (vcpkg):**
```powershell
vcpkg install sdl2 glew
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

**Linux:**
```bash
sudo apt-get install cmake libsdl2-dev libglew-dev
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**macOS:**
```bash
brew install cmake sdl2 glew
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

---

## Coding Style

See [CODING_STYLE.md](CODING_STYLE.md) for complete guidelines.

---

## Integration with Game Code

The framework is designed to be integrated with game-specific code:

### From GameState

```cpp
// In GameState::Update()
void GameState::Update(float deltaTime) {
    // Call game logic from game_core/
    // UpdatePlayerTurns();
    // CheckWinConditions();
}

// In GameState::Render()
void GameState::Render() {
    // Call map rendering from map/
    // RenderMap();
    // RenderPlayers();
}
```

### Using AI System

```cpp
// In game AI code
#include "NeuralNetworkManager.h"

NetworkInput input;
input.vecFeatures = EncodeGameState();

auto future = AI::NeuralNetworkManager::PredictAsync(input);
// Do other work...
NetworkOutput output = future.get();
```

### Using Thread Pool

```cpp
#include "ThreadPool.h"

// Submit background task
auto future = Threading::ThreadPool::Submit([]() {
    return ExpensiveCalculation();
});

// Get result when needed
int result = future.get();
```

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| SDL2 | 2.0.0+ | Window, input, events |
| OpenGL | 3.3+ | Graphics rendering |
| GLEW | 2.0+ | OpenGL extension loading |

---

## Adding New States

To add a new game state:

1. **Create header** (`include/MyState.h`):
```cpp
#ifndef SCOTLANDYARD_STATES_MYSTATE_H
#define SCOTLANDYARD_STATES_MYSTATE_H

#include "IGameState.h"

namespace ScotlandYard {
namespace States {

class MyState : public Core::IGameState {
public:
    void OnEnter() override;
    void OnExit() override;
    void OnPause() override;
    void OnResume() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleEvent(const SDL_Event& event) override;
};

} // namespace States
} // namespace ScotlandYard

#endif
```

2. **Create implementation** (`src/MyState.cpp`)

3. **Register in Application.cpp:**
```cpp
m_pStateManager->RegisterState("mystate", std::make_unique<States::MyState>());
```

4. **Add to CMakeLists.txt:**
```cmake
set(SOURCES
    ...
    src/MyState.cpp
)

set(HEADERS
    ...
    include/MyState.h
)
```

---
