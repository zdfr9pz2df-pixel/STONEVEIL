cmake_minimum_required(VERSION 3.24)
project(STONEVEIL VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(stoneveil
  src/main.cpp
  src/Game.cpp
  src/Dungeon.cpp
  src/EventSystem.cpp
  src/Authoring.cpp
)

target_include_directories(stoneveil PRIVATE include)
target_link_libraries(stoneveil PRIVATE raylib)

if (MSVC)
  target_compile_options(stoneveil PRIVATE /W4 /permissive-)
else()
  target_compile_options(stoneveil PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(stoneveil PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
