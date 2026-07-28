#include <iostream>

#include "components/Switch.h"
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


    Switch sw(
        3,
        500,
        200
    );


    sw.toggle();


    manager.add(&resistor);
    manager.add(&led);
    manager.add(&sw);


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