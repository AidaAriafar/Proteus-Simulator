cmake_minimum_required(VERSION 3.20)

project(ProteusSimulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

find_package(SDL2 REQUIRED CONFIG)

set(CORE_SOURCES
    src/core/Component.cpp
    src/core/Pin.cpp
    src/components/Resistor.cpp
    src/components/LED.cpp
    src/components/Switch.cpp
    src/components/Sources.cpp
    src/components/PassiveComponents.cpp
    src/components/InteractiveComponents.cpp
    src/components/Digital.cpp
    src/editor/ComponentManager.cpp
    src/editor/PropertiesPanel.cpp
    src/library/ComponentLibrary.cpp
    src/drc/DRCChecker.cpp
    src/wiring/Wiring.cpp
    src/sim/Simulator.cpp
    src/persistence/CircuitSerializer.cpp
)

add_library(proteus_core STATIC ${CORE_SOURCES})
target_include_directories(proteus_core PUBLIC src third_party)

add_executable(ProteusSimulator src/app/main.cpp)
target_link_libraries(ProteusSimulator PRIVATE proteus_core SDL2::SDL2)
target_include_directories(ProteusSimulator PRIVATE ${SDL2_INCLUDE_DIRS})

add_executable(AidaTest src/test_component.cpp)
target_link_libraries(AidaTest PRIVATE proteus_core)
add_test(NAME AidaTest COMMAND AidaTest)

add_executable(UnifiedTests tests/UnifiedTests.cpp)
target_link_libraries(UnifiedTests PRIVATE proteus_core)
add_test(NAME UnifiedTests COMMAND UnifiedTests)
