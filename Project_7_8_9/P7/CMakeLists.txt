cmake_minimum_required(VERSION 3.20)
project(untitled2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG SDL2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

add_executable(untitled2 ../Project7.cpp)
target_link_libraries(untitled2 PRIVATE SDL2::SDL2)

if(WIN32 AND TARGET SDL2::SDL2)
    add_custom_command(TARGET untitled2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2>
                $<TARGET_FILE_DIR:untitled2>
    )
endif()
