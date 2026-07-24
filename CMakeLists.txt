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

add_executable(korework
    src/main.cpp
    src/offline_runtime.cpp
)

target_include_directories(korework PRIVATE src)
target_link_libraries(korework PRIVATE raylib)
target_compile_definitions(korework PRIVATE KOREWORK_OFFLINE_ONLY=1)

if(MSVC)
    target_compile_options(korework PRIVATE /W4 /permissive-)
else()
    target_compile_options(korework PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(korework PROPERTIES
    OUTPUT_NAME "KOREWORK"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS korework RUNTIME DESTINATION .)
