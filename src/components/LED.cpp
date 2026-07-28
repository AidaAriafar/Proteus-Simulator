#include "LED.h"

#include "../core/Pin.h"

#include <iostream>



LED::LED(
    int id,
    float x,
    float y
)
:
Component(id,x,y)
{

    state = false;


    addPin(
        Pin(
            1,
            "Anode",
            x - 20,
            y,
            PinType::INPUT
        )
    );


    addPin(
        Pin(
            2,
            "Cathode",
            x + 20,
            y,
            PinType::POWER
        )
    );

}



void LED::draw()
{

    std::cout
    << "Drawing LED at "
    << x
    << ", "
    << y
    << " State: "
    << (state ? "ON" : "OFF")
    << std::endl;

}



std::string LED::getType()
{
    return "LED";
}



void LED::turnOn()
{
    state=true;
}



void LED::turnOff()
{
    state=false;
}



bool LED::isOn()
{
    return state;
}