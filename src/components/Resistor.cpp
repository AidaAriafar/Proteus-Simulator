#include "Resistor.h"

#include "../core/Pin.h"

#include <iostream>



Resistor::Resistor(
    int id,
    float x,
    float y,
    float resistance
)
:
Component(id, x, y)
{

    this->resistance = resistance;


    addPin(
        Pin(
            1,
            "P1",
            x - 20,
            y,
            PinType::POWER
        )
    );


    // Second terminal

    addPin(
        Pin(
            2,
            "P2",
            x + 20,
            y,
            PinType::POWER
        )
    );

}



void Resistor::draw()
{

    std::cout
    << "Drawing resistor at "
    << x
    << ", "
    << y
    << std::endl;


}



std::string Resistor::getType()
{
    return "Resistor";
}