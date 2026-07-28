#include "Resistor.h"

#include <iostream>



Resistor::Resistor(
    int id,
    float x,
    float y,
    float resistance
)
:
Component(id,x,y)
{
    this->resistance=resistance;
}



void Resistor::draw()
{

    std::cout
    <<
    "Drawing resistor at "
    <<
    x
    <<
    ", "
    <<
    y
    <<
    std::endl;

}



std::string Resistor::getType()
{
    return "Resistor";
}