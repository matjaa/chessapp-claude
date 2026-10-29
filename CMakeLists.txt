cmake_minimum_required(VERSION 3.28)

project(ChessApp
    VERSION 0.1.0
    DESCRIPTION "Chess against three computer bots, built with SFML 3"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release for single-config generators when nothing is chosen,
# so the Hard bot (depth 4) is fast when built from the command line.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Put every executable in <build>/bin so the assets folder can sit next to it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# SFML 3 (downloaded and built automatically, linked statically)
# ---------------------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE) # show download progress

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # static SFML: no DLLs to copy
set(SFML_BUILD_AUDIO   OFF CACHE BOOL "" FORCE) # not needed, skips audio dependencies
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE) # not needed

# Both SFML and its FreeType dependency are downloaded as single release
# archives. (A Git download would fetch every branch of the repositories,
# which is very slow.)
FetchContent_Declare(SFML
    URL https://github.com/SFML/SFML/archive/refs/tags/3.0.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    EXCLUDE_FROM_ALL
    SYSTEM)

# SFML declares FreeType itself with Git; the first declaration of a name wins,
# so this one is used instead. The patch step is the same one SFML applies.
FetchContent_Declare(Freetype
    URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    PATCH_COMMAND ${CMAKE_COMMAND}
        -DFREETYPE_DIR=${FETCHCONTENT_BASE_DIR}/freetype-src
        -P ${FETCHCONTENT_BASE_DIR}/sfml-src/tools/freetype/PatchFreetype.cmake)

FetchContent_MakeAvailable(SFML)

# ---------------------------------------------------------------------------
# Shared warning flags for our own targets
# ---------------------------------------------------------------------------
add_library(chess_warnings INTERFACE)
if(MSVC)
    target_compile_options(chess_warnings INTERFACE /W4 /permissive- /utf-8)
else()
    target_compile_options(chess_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Chess rules library (no SFML, so it can be tested on its own)
# ---------------------------------------------------------------------------
add_library(chess_core STATIC
    src/core/Types.cpp
    src/core/Position.cpp
    src/core/Game.cpp
    src/core/Notation.cpp
    src/core/Perft.cpp
)
target_include_directories(chess_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(chess_core PRIVATE chess_warnings)

# ---------------------------------------------------------------------------
# Bots: evaluation and alpha-beta search (no SFML either)
# ---------------------------------------------------------------------------
add_library(chess_engine STATIC
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/Bot.cpp
)
target_link_libraries(chess_engine PUBLIC chess_core PRIVATE chess_warnings)

# ---------------------------------------------------------------------------
# Tests: rules + perft and bots  (run: build\bin\Release\core_tests.exe
#                                  and build\bin\Release\engine_tests.exe)
# ---------------------------------------------------------------------------
option(CHESS_BUILD_TESTS "Build the core_tests and engine_tests programs" ON)
if(CHESS_BUILD_TESTS)
    enable_testing()
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE chess_core chess_warnings)
    add_test(NAME core_tests COMMAND core_tests)

    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE chess_engine chess_warnings)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()

# ---------------------------------------------------------------------------
# The game executable
# ---------------------------------------------------------------------------
add_executable(ChessApp
    src/main.cpp
    src/gui/Application.cpp
    src/gui/Assets.cpp
    src/gui/BoardView.cpp
    src/gui/Button.cpp
    src/gui/PieceTextures.cpp
    src/gui/PromotionPicker.cpp
)

target_include_directories(ChessApp PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ChessApp PRIVATE chess_engine SFML::Graphics chess_warnings)

# Copy assets/ next to the executable after every build.
add_custom_command(TARGET ChessApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:ChessApp>/assets
    COMMENT "Copying assets next to the executable")

# When a Visual Studio solution is generated, run the game from its own folder.
set_property(TARGET ChessApp PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:ChessApp>)
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ChessApp)
