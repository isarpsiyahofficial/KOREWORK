cmake_minimum_required(VERSION 3.24)
project(KOREWORKOffline VERSION 1.0.0 LANGUAGES CXX)

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
    src/content/n3_equipment.cpp
    src/content/n3_skeleton.cpp
    src/content/n3_texture.cpp
    src/content/npc_looks_table.cpp
    src/content/smd_map.cpp
    src/data/game_data_pack.cpp
    src/data/openko_sql_compiler.cpp
)
target_include_directories(korework_content PUBLIC src)

add_library(korework_runtime STATIC
    src/client/n3_animation_player.cpp
    src/offline_runtime.cpp
)
target_include_directories(korework_runtime PUBLIC src)
target_link_libraries(korework_runtime PUBLIC korework_content)
target_compile_definitions(korework_runtime PUBLIC KOREWORK_OFFLINE_ONLY=1)

add_executable(korework
    src/main.cpp
    src/client/n3_character_attachment.cpp
    src/client/n3_character_model.cpp
    src/client/n3_equipment_model.cpp
    src/client/smd_terrain.cpp
)
target_include_directories(korework PRIVATE src)
target_link_libraries(korework PRIVATE korework_runtime raylib)
target_compile_definitions(korework PRIVATE KOREWORK_OFFLINE_ONLY=1)

add_executable(korework_content_probe tools/ko_content_probe.cpp)
target_link_libraries(korework_content_probe PRIVATE korework_content)
add_executable(korework_equipment_probe tools/ko_equipment_probe.cpp)
target_link_libraries(korework_equipment_probe PRIVATE korework_content)
add_executable(korework_dataset_compiler tools/openko_dataset_compiler.cpp)
target_link_libraries(korework_dataset_compiler PRIVATE korework_content)

if(MSVC)
    target_compile_options(korework_content PRIVATE /W4 /permissive-)
    target_compile_options(korework_runtime PRIVATE /W4 /permissive-)
    target_compile_options(korework PRIVATE /W4 /permissive-)
    target_compile_options(korework_content_probe PRIVATE /W4 /permissive-)
    target_compile_options(korework_equipment_probe PRIVATE /W4 /permissive-)
    target_compile_options(korework_dataset_compiler PRIVATE /W4 /permissive-)
else()
    target_compile_options(korework_content PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework_runtime PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework_content_probe PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework_equipment_probe PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework_dataset_compiler PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(korework PROPERTIES
    OUTPUT_NAME "KOREWORK"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(korework_content_probe korework_equipment_probe korework_dataset_compiler PROPERTIES
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

    add_executable(korework_equipment_smoke tests/n3_equipment_smoke.cpp)
    target_link_libraries(korework_equipment_smoke PRIVATE korework_content)
    add_test(NAME n3-equipment-smoke COMMAND korework_equipment_smoke)

    add_executable(korework_game_data_smoke tests/game_data_pack_smoke.cpp)
    target_link_libraries(korework_game_data_smoke PRIVATE korework_content)
    add_test(NAME sql-free-game-data-pack-smoke COMMAND korework_game_data_smoke)

    add_executable(korework_animation_player_smoke tests/n3_animation_player_smoke.cpp)
    target_link_libraries(korework_animation_player_smoke PRIVATE korework_runtime)
    add_test(NAME n3-runtime-animation-player-smoke COMMAND korework_animation_player_smoke)

    add_executable(korework_sql_compiler_smoke tests/openko_sql_compiler_smoke.cpp)
    target_link_libraries(korework_sql_compiler_smoke PRIVATE korework_content)
    add_test(NAME openko-sql-to-kopack-smoke COMMAND korework_sql_compiler_smoke)

    add_executable(korework_npc_looks_smoke tests/npc_looks_table_smoke.cpp)
    target_link_libraries(korework_npc_looks_smoke PRIVATE korework_content)
    add_test(NAME encrypted-npc-looks-smoke COMMAND korework_npc_looks_smoke)

    add_dependencies(korework_runtime_smoke
        korework_smd_smoke
        korework_texture_smoke
        korework_skeleton_smoke
        korework_animation_smoke
        korework_equipment_smoke
        korework_game_data_smoke
        korework_animation_player_smoke
        korework_sql_compiler_smoke
        korework_npc_looks_smoke
    )
endif()

install(TARGETS korework RUNTIME DESTINATION .)
