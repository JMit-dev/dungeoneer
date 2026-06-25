cmake_minimum_required(VERSION 3.15)
project(game VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

# Disable examples/tests for all deps
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

# --- raylib (local submodule) ---
FetchContent_Declare(raylib SOURCE_DIR "${CMAKE_SOURCE_DIR}/lib/raylib")
FetchContent_MakeAvailable(raylib)

# --- raylib-aseprite (header-only, all platforms) ---
add_subdirectory(lib/raylib-aseprite)

# --- Game executable ---
set(GAME_SOURCES
    src/main.c
    src/dungeon.c
    src/pathfind.c
)

add_executable(game ${GAME_SOURCES})

target_include_directories(game PRIVATE
    src
    lib/raylib-aseprite/include
)

if(NOT EMSCRIPTEN)
    target_link_libraries(game PRIVATE raylib raylib_aseprite)
else()
    target_link_libraries(game PRIVATE raylib raylib_aseprite)
    set_target_properties(game PROPERTIES SUFFIX ".html")
    target_link_options(game PRIVATE
        -sUSE_GLFW=3
        -sTOTAL_MEMORY=128MB
        -sFORCE_FILESYSTEM=1
        -sMINIFY_HTML=0
        --shell-file "${CMAKE_SOURCE_DIR}/src/minshell.html"
        --preload-file "${CMAKE_SOURCE_DIR}/resources@resources"
    )
endif()

# macOS frameworks
if(APPLE)
    target_link_libraries(game PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework CoreVideo"
    )
endif()

# Copy resources next to executable after build
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:game>/resources"
)
