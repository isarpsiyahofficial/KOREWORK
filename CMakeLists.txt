cmake_minimum_required(VERSION 3.24)
project(KOREWORKOffline VERSION 0.1.0 LANGUAGES CXX)

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

add_library(korework_runtime STATIC
    src/offline_runtime.cpp
)
target_include_directories(korework_runtime PUBLIC src)
target_compile_definitions(korework_runtime PUBLIC KOREWORK_OFFLINE_ONLY=1)

add_executable(korework
    src/main.cpp
)
target_include_directories(korework PRIVATE src)
target_link_libraries(korework PRIVATE korework_runtime raylib)
target_compile_definitions(korework PRIVATE KOREWORK_OFFLINE_ONLY=1)

if(MSVC)
    target_compile_options(korework_runtime PRIVATE /W4 /permissive-)
    target_compile_options(korework PRIVATE /W4 /permissive-)
else()
    target_compile_options(korework_runtime PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(korework PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(korework PROPERTIES
    OUTPUT_NAME "KOREWORK"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(korework_runtime_smoke tests/runtime_smoke.cpp)
    target_link_libraries(korework_runtime_smoke PRIVATE korework_runtime)
    add_test(NAME offline-runtime-smoke COMMAND korework_runtime_smoke)
endif()

install(TARGETS korework RUNTIME DESTINATION .)
