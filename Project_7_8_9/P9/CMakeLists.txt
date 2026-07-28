cmake_minimum_required(VERSION 3.20)

project(Project9MeasurementDFlipFlop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use the SDL2 package installed by MSYS2 UCRT64.
# This avoids downloading SDL2 from GitHub during CMake configuration.
set(SDL2_DIR "C:/msys64/ucrt64/lib/cmake/SDL2" CACHE PATH "SDL2 CMake package directory")

if(NOT EXISTS "${SDL2_DIR}/SDL2Config.cmake")
    message(FATAL_ERROR
            "SDL2 was not found at ${SDL2_DIR}. Open MSYS2 UCRT64 and run: "
            "pacman -S --needed mingw-w64-ucrt-x86_64-SDL2")
endif()

find_package(SDL2 CONFIG REQUIRED COMPONENTS SDL2 SDL2main)

add_executable(Project9 Project9.cpp)

if(TARGET SDL2::SDL2main)
    target_link_libraries(Project9 PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(Project9 PRIVATE SDL2::SDL2)

if(WIN32)
    add_custom_command(
            TARGET Project9
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:Project9>
            VERBATIM
    )
endif()