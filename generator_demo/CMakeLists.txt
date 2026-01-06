cmake_minimum_required(VERSION 3.10)
project(SimpleWindow)

set(CMAKE_CXX_STANDARD 11)

find_package(SDL2 REQUIRED)

add_executable(SimpleWindow 
    SimpleWindow.cpp
    MapGenerator.cpp
)

target_link_libraries(SimpleWindow PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_include_directories(SimpleWindow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    set_target_properties(SimpleWindow PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()