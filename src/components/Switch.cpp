#include "Switch.h"

#include "../core/Pin.h"

#include <iostream>



Switch::Switch(
    int id,
    float x,
    float y
)
:
Component(id,x,y)
{

    state = false;


    // Input pin

    addPin(
        Pin(
            1,
            "Input",
            x - 20,
            y,
            PinType::INPUT
        )
    );


    // Output pin

    addPin(
        Pin(
            2,
            "Output",
            x + 20,
            y,
            PinType::OUTPUT
        )
    );

}



void Switch::draw()
{

    std::cout
    << "Drawing Switch at "
    << x
    << ", "
    << y
    << " State: "
    << (state ? "ON" : "OFF")
    << std::endl;

}



std::string Switch::getType()
{
    return "Switch";
}



void Switch::toggle()
{
    state = !state;
}



void Switch::turnOn()
{
    state = true;
}



void Switch::turnOff()
{
    state = false;
}



bool Switch::isOn()
{
    return state;
}