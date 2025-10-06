# Build Instructions - Scotland Yard++

## Quick Start

### Windows (Visual Studio)

```powershell
# 1. Install dependencies using vcpkg
vcpkg install sdl2 glew

# 2. Configure CMake
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake

# 3. Build
cmake --build . --config Release

# 4. Run
.\bin\Release\ScotlandYardPlusPlus.exe
```

### Linux

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install cmake build-essential libsdl2-dev libglew-dev

# 2. Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 3. Run
./bin/ScotlandYardPlusPlus
```

### macOS

```bash
# 1. Install dependencies
brew install cmake sdl2 glew

# 2. Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)

# 3. Run
./bin/ScotlandYardPlusPlus
```

---

## Detailed Setup Guide

## Windows

### Option 1: Using vcpkg (Recommended)

1. **Install vcpkg:**
```powershell
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

2. **Install dependencies:**
```powershell
.\vcpkg install sdl2:x64-windows
.\vcpkg install glew:x64-windows
```

3. **Configure project:**
```powershell
cd "YOUR_FOLDER\Scotland-Yard-Plus-Plus"
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

4. **Build:**
```powershell
cmake --build . --config Release
```

### Option 2: Manual Installation

1. **Download SDL2:**
   - Visit: https://www.libsdl.org/download-2.0.php
   - Download: "SDL2-devel-2.x.x-VC.zip"
   - Extract to `C:\Libraries\SDL2`

2. **Download GLEW:**
   - Visit: http://glew.sourceforge.net/
   - Download: "glew-2.x.x-win32.zip"
   - Extract to `C:\Libraries\GLEW`

3. **Configure CMake:**
```powershell
mkdir build
cd build
cmake .. -DSDL2_DIR="C:/Libraries/SDL2/cmake" -DGLEW_DIR="C:/Libraries/GLEW"
```

4. **Build:**
```powershell
cmake --build . --config Release
```

### Visual Studio Users

Open the generated solution file:
```powershell
start ScotlandYardPlusPlus.sln
```

Then build using Visual Studio (Ctrl+Shift+B).

---

## CMake Configuration Options

### Build Types

```bash
# Debug build (with debug symbols, no optimization)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build (optimized, no debug symbols)
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### Custom Dependency Paths

If CMake can't find libraries automatically:

```bash
cmake .. \
    -DSDL2_DIR=/path/to/sdl2 \
    -DGLEW_DIR=/path/to/glew \
    -DCMAKE_PREFIX_PATH="/path/to/libs"
```