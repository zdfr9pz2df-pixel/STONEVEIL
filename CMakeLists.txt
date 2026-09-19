cmake_minimum_required(VERSION 3.24)
project(STONEVEIL VERSION 0.4.1 LANGUAGES CXX)

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

function(stoneveil_warnings target)
  if (MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# One raylib-free source list for runtime and every core regression suite.
add_library(stoneveil_core STATIC
  src/ActionGate.cpp
  src/Authoring.cpp
  src/Campaign.cpp
  src/CampaignInventory.cpp
  src/Character.cpp
  src/CombatSystem.cpp
  src/CombatTuning.cpp
  src/Dungeon.cpp
  src/EnemyType.cpp
  src/EventSystem.cpp
  src/Formation.cpp
  src/LevelBlueprint.cpp
  src/LevelDocument.cpp
  src/LevelOne.cpp
  src/LevelIO.cpp
  src/Lighting.cpp
  src/Material.cpp
  src/Party.cpp
  src/Player.cpp
  src/Portrait.cpp
  src/Roster.cpp
  src/SaveSystem.cpp
  src/WorldAuthoring.cpp
  src/WorldEvents.cpp
)
target_include_directories(stoneveil_core PUBLIC include)
stoneveil_warnings(stoneveil_core)

add_executable(stoneveil
  src/main.cpp
  src/AudioSystem.cpp
  src/Game.cpp
  src/LevelEditor.cpp
  src/Raycaster.cpp
)
target_link_libraries(stoneveil PRIVATE stoneveil_core raylib)
target_compile_definitions(stoneveil PRIVATE
  STONEVEIL_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  STONEVEIL_BUILD_LABEL="STONEVEIL v0.4.1-creator-integration"
)
stoneveil_warnings(stoneveil)
set_target_properties(stoneveil PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET stoneveil POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/content"
          "$<TARGET_FILE_DIR:stoneveil>/content"
)

include(CTest)
if (BUILD_TESTING)
  function(stoneveil_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE stoneveil_core)
    target_compile_definitions(${target} PRIVATE STONEVEIL_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    stoneveil_warnings(${target})
    add_test(NAME ${target} COMMAND ${target})
  endfunction()
  stoneveil_test(stoneveil_core_tests tests/CoreTests.cpp)
  stoneveil_test(stoneveil_event_tests tests/EventSystemTests.cpp)
  stoneveil_test(stoneveil_compatibility_tests tests/CompatibilityTests.cpp)
  stoneveil_test(stoneveil_document_tests tests/EditorDocumentTests.cpp)
  add_test(NAME stoneveil_editor_validate
           COMMAND stoneveil --validate "${CMAKE_SOURCE_DIR}/content/levels/gatehouse.svl")
endif()
