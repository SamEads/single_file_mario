cmake_minimum_required(VERSION 3.0)
project(single_file_mario C)
set(CMAKE_C_STANDARD 99)

file(COPY assets DESTINATION .)

# raylib
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
    GIT_TAG "master"
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(raylib)

# source files
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "src/*.c")
set(PROJECT_INCLUDE "src/")

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDE} external/stb)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)