#include <iostream>
#include "library/ComponentLibrary.h"
#include "components/Switch.h"
#include "components/LED.h"
#include "components/Resistor.h"
#include "editor/ComponentManager.h"

#include <memory>
int main()
{

    ComponentManager manager;
    ComponentLibrary library;


library.registerComponent("Resistor");
library.registerComponent("LED");
library.registerComponent("Switch");


for(const auto& item : library.getComponents())
{
    std::cout
        << "Library Component: "
        << item
        << std::endl;
}


Resistor resistor(
    1,
    100,
    200,
    1000
);


LED led(
    2,
    300,
    200
);


led.turnOn();


Switch sw(
    3,
    500,
    200
);


sw.toggle();


manager.add(&resistor);
manager.add(&led);
manager.add(&sw);


auto libraryLED = library.createComponent(
    "LED",
    4,
    700,
    200
);


if(libraryLED != nullptr)
{
    std::cout
        << "Factory created: "
        << libraryLED->getType()
        << std::endl;

    manager.add(libraryLED.get());
}


auto unknownComponent = library.createComponent(
    "Motor",
    5,
    900,
    200
);


if(unknownComponent == nullptr)
{
    std::cout
        << "Component not found: Motor"
        << std::endl;
}


manager.drawAll();


    std::cout
        << "Resistor pins: "
        << resistor.getPins().size()
        << std::endl;


    std::cout
        << "LED pins: "
        << led.getPins().size()
        << std::endl;


    std::cout
        << "Switch pins: "
        << sw.getPins().size()
        << std::endl;


    std::cout
        << "Switch state: "
        << (sw.isOn() ? "ON" : "OFF")
        << std::endl;


    for(auto& pin : resistor.getPins())
    {
        std::cout
            << pin.getName()
            << " at "
            << pin.getX()
            << ", "
            << pin.getY()
            << std::endl;
    }


    std::cout
        << "Component system works!"
        << std::endl;


    return 0;
}