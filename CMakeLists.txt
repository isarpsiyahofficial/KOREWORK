cmake_minimum_required(VERSION 3.24)
project(KOREWORKOffline VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_library(korework_content STATIC
    src/content/asset_catalog.cpp
    src/content/ko_asset_resolver.cpp
    src/content/n3_animation.cpp
    src/content/n3_character.cpp
    src/content/n3_skeleton.cpp
    src/content/n3_texture.cpp
    src/content/smd_map.cpp
)
target_include_directories(korework_content PUBLIC src)

add_library(korework_runtime STATIC
    src/offline_runtime.cpp
)
target_include_directories(korework_runtime PUBLIC src)
target_link_libraries(korework_runtime PUBLIC korework_content)
target_compile_definitions(korework_runtime PUBLIC KOREWORK_OFFLINE_ONLY=1)

add_executable(korework
    src/main.cpp
    src/client/n3_character_model.cpp
    src/client/smd_terrain.cpp
)
target_include_directories(korework PRIVATE src)
target_link_libraries(korework PRIVATE korework_runtime raylib)
target_compile_definitions(korework PRIVATE KOREWORK_OFFLINE_ONLY=1)

add_executable(korework_content_probe tools/ko_content_probe.cpp)
target_link_libraries(korework_content_probe PRIVATE korework_content)

if(MSVC)
    target_compile_options(korework_content PRIVATE /W4 /permissive-)
    target_compile_options(korework_runtime PRIVATE /W4 /permissive-)
    target_compile_options(korework PRIVATE /W4 /permissive-)
    target_compile_options(korework_content_probe PRIVATE /W4 /permissive-)
else()
    target_compile_options(korework_content PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework_runtime PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework_content_probe PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(korework PROPERTIES
    OUTPUT_NAME "KOREWORK"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(korework_content_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(korework_runtime_smoke tests/runtime_smoke.cpp)
    target_link_libraries(korework_runtime_smoke PRIVATE korework_runtime)
    add_test(NAME offline-runtime-smoke COMMAND korework_runtime_smoke)

    add_executable(korework_smd_smoke tests/smd_map_smoke.cpp)
    target_link_libraries(korework_smd_smoke PRIVATE korework_content)
    add_test(NAME fire-drake-smd-smoke COMMAND korework_smd_smoke)

    add_executable(korework_texture_smoke tests/n3_texture_smoke.cpp)
    target_link_libraries(korework_texture_smoke PRIVATE korework_content)
    add_test(NAME noah-texture-smoke COMMAND korework_texture_smoke)

    add_executable(korework_skeleton_smoke tests/n3_skeleton_smoke.cpp)
    target_link_libraries(korework_skeleton_smoke PRIVATE korework_content)
    add_test(NAME n3-skeleton-smoke COMMAND korework_skeleton_smoke)

    add_executable(korework_animation_smoke tests/n3_animation_smoke.cpp)
    target_link_libraries(korework_animation_smoke PRIVATE korework_content)
    add_test(NAME n3-animation-smoke COMMAND korework_animation_smoke)
endif()

install(TARGETS korework RUNTIME DESTINATION .)
