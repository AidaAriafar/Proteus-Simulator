cmake_minimum_required(VERSION 3.20)

project(AidaComponentSystem)


set(CMAKE_CXX_STANDARD 17)


add_executable(
AidaTest

src/test_component.cpp

src/core/Component.cpp
src/core/Pin.cpp

src/components/Resistor.cpp
src/components/LED.cpp
src/components/Switch.cpp

src/editor/ComponentManager.cpp

src/library/ComponentLibrary.cpp

)