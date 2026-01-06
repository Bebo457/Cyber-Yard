cmake_minimum_required(VERSION 3.10)
project(SimpleWindow)

set(CMAKE_CXX_STANDARD 11)

# Znajdź SDL2
find_package(SDL2 REQUIRED)

# Utwórz exe
add_executable(SimpleWindow 
    SimpleWindow.cpp
    MapGenerator.cpp
)

# Linkuj SDL2
target_link_libraries(SimpleWindow ${SDL2_LIBRARIES})
target_include_directories(SimpleWindow PRIVATE ${SDL2_INCLUDE_DIRS})