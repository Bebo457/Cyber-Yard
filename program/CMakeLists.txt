cmake_minimum_required(VERSION 3.15)
project(ScotlandYardPlusPlus VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#Output
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Dependencies ====================================

# SDL2
find_package(SDL2 REQUIRED)
if(NOT SDL2_FOUND)
    message(FATAL_ERROR "SDL2 not found. Please install SDL2 development libraries")
endif()

find_package(OpenGL REQUIRED)
if(NOT OPENGL_FOUND)
    message(FATAL_ERROR "OpenGL not found. Please install OpenGL development libraries")
endif()

find_package(GLEW REQUIRED)
if(NOT GLEW_FOUND)
    message(FATAL_ERROR "GLEW not found. Please install GLEW development libraries")
endif()

find_package(glm REQUIRED)
if(NOT glm_FOUND)
    message(FATAL_ERROR "GLM not found. Please install")
endif()

find_package(Freetype REQUIRED)
if(NOT FREETYPE_FOUND)
    message(FATAL_ERROR "FreeType not found. Please install FreeType development libraries")
endif()

# SOURCE FILES =========================================
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/StateManager.cpp
    src/MemoryManager.cpp
    src/ThreadPool.cpp
    src/NeuralNetworkManager.cpp
    src/MenuState.cpp
    src/GameState.cpp
    src/HUDOverlay.cpp
    src/Player.cpp
)

set(HEADERS
    include/Application.h
    include/StateManager.h
    include/IGameState.h
    include/MemoryManager.h
    include/ThreadPool.h
    include/NeuralNetworkManager.h
    include/MenuState.h
    include/GameState.h
    include/HUDOverlay.h
    include/Player.h
)

# EXE =================================================

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    glm::glm
    ${FREETYPE_LIBRARIES}
)

# Platform-specific
if(WIN32)
    # Windows-specific settings
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)

    # Link additional Windows libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE
        opengl32
    )

    # Copy SDL2.dll to output directory on Windows
    if(EXISTS "${SDL2_DIR}/../bin/SDL2.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_DIR}/../bin/SDL2.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    target_link_libraries(${PROJECT_NAME} PRIVATE
        pthread
        dl
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    )

elseif(APPLE)
    # macOS-specific settings
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
    )
endif()

# COMPILER WARNINGS============================================

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# BUILD FLAGS ============================================

if(CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "Debug build")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG _DEBUG)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    message(STATUS "Release build")
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)

    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /O2)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -O3)
    endif()
endif()

# INSTALLATION ============================================

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# PRINT CONFIGURATION
message(STATUS "")
message(STATUS "Scotland Yard Build Configuration")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
