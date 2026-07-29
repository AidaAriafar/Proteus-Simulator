cmake_minimum_required(VERSION 3.20)

project(AidaComponentSystem)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()


add_executable(
AidaTest

src/test_component.cpp

src/core/Component.cpp
src/core/Pin.cpp

src/components/Resistor.cpp
src/components/LED.cpp
src/components/Switch.cpp
src/components/Sources.cpp
src/components/PassiveComponents.cpp
src/components/InteractiveComponents.cpp

src/editor/ComponentManager.cpp
src/editor/PropertiesPanel.cpp

src/library/ComponentLibrary.cpp
src/drc/DRCChecker.cpp

)

add_test(NAME AidaTest COMMAND AidaTest)
