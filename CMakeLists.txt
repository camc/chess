cmake_minimum_required(VERSION 3.11)
project(chess C)

set(CMAKE_C_STANDARD 11)
set(OpenGL_GL_PREFERENCE GLVND)

# Find raylib, download if not found
# find_package(raylib 3.7 QUIET) broken?
if (NOT raylib_FOUND)
    include(FetchContent)

    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/bf2ad9df5fdcaa385b2a7f66fd85632eeebbadaa.zip
    )

    FetchContent_GetProperties(raylib)
    if (NOT raylib_POPULATED)
        set(FETCHCONTENT_QUIET NO)
        FetchContent_Populate(raylib)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
    endif()
endif()

# Find raygui, download if not found
find_package(raygui 2.6 QUIET)
if (NOT raygui_FOUND)
    include(FetchContent)

    FetchContent_Declare(
        raygui
        URL https://github.com/raysan5/raygui/archive/4e2a878e715c4aafa6ad7bd58d851221503c6e60.zip
    )

    FetchContent_GetProperties(raygui)
    if (NOT raygui_POPULATED)
        set(FETCHCONTENT_QUIET NO)
        FetchContent_Populate(raygui)
        set(BUILD_RAYGUI_EXAMPLES OFF CACHE BOOL "" FORCE)
        add_subdirectory(${raygui_SOURCE_DIR}/projects/CMake ${raygui_BINARY_DIR})
    endif()
endif()

include_directories(include ${raygui_SOURCE_DIR}/src)
add_executable(chess src/main.c src/chess.c src/draw.c src/engine.c src/zobrist.c src/openings.c src/tptable.c src/raygui.c src/fen.c src/frontend_state.c src/ui.c src/threadpool.c)
target_link_libraries(chess PRIVATE raylib)

if(MSVC)
    target_compile_options(chess PRIVATE /W4 $<$<CONFIG:Release>:/arch:AVX2 /GL>)
    target_compile_options(raylib PRIVATE $<$<CONFIG:Release>:/arch:AVX2 /GL>)
else()
    target_compile_options(chess PRIVATE -g -Wall -Wextra -pedantic $<$<CONFIG:Release>:-mavx2 -flto>)
    target_compile_options(raylib PRIVATE -g $<$<CONFIG:Release>:-mavx2 -flto>)
endif()

if(CHESS_ENABLE_DEBUG_KEYS)
    target_compile_definitions(chess PRIVATE CHESS_ENABLE_DEBUG_KEYS)
endif()

if(CHESS_TEST)
    target_compile_definitions(chess PRIVATE CHESS_TEST)
endif()

add_custom_command(TARGET chess POST_BUILD COMMAND 
    ${CMAKE_COMMAND} -E copy_directory 
    ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:chess>/res)
