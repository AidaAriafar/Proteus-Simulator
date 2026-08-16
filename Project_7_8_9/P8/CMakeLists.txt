cmake_minimum_required(VERSION 3.20)
project(Project8SimulationControl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL2)

add_executable(Project8 Project8.cpp)
target_link_libraries(Project8 PRIVATE SDL2::SDL2main SDL2::SDL2)

if(WIN32)
    add_custom_command(TARGET Project8 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2>
                $<TARGET_FILE_DIR:Project8>
    )
endif()
