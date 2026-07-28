#include <iostream>

#include "components/LED.h"
#include "components/Resistor.h"
#include "editor/ComponentManager.h"


int main()
{

    ComponentManager manager;


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


    manager.add(&resistor);
    manager.add(&led);


    manager.drawAll();


    std::cout
        << "LED pins: "
        << led.getPins().size()
        << std::endl;


    std::cout
        << "Resistor pins: "
        << resistor.getPins().size()
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