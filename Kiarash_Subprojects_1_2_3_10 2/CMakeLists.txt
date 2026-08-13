cmake_minimum_required(VERSION 3.20)

project(KiarashSubprojects LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
    kiarash_project_modules
    src/common/ProjectData.cpp
    src/canvas/CanvasServices.cpp
    src/library/ComponentLibraryManagement.cpp
    src/persistence/PersistenceServices.cpp
    src/startup/StartupServices.cpp
)

target_include_directories(
    kiarash_project_modules
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(kiarash_project_modules PRIVATE /W4)
else()
    target_compile_options(kiarash_project_modules PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(kiarash_project_tests tests/KiarashProjectTests.cpp)
target_link_libraries(kiarash_project_tests PRIVATE kiarash_project_modules)

if(MSVC)
    target_compile_options(kiarash_project_tests PRIVATE /W4)
else()
    target_compile_options(kiarash_project_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(kiarash_demo examples/kiarash_demo.cpp)
target_link_libraries(kiarash_demo PRIVATE kiarash_project_modules)

if(MSVC)
    target_compile_options(kiarash_demo PRIVATE /W4)
else()
    target_compile_options(kiarash_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME kiarash_project_tests COMMAND kiarash_project_tests)
