#include "Component.h"



Component::Component(
    int id,
    float x,
    float y
)
{
    this->id = id;

    this->x = x;

    this->y = y;
}



Component::~Component()
{

}



int Component::getID()
{
    return id;
}



float Component::getX()
{
    return x;
}



float Component::getY()
{
    return y;
}



void Component::addPin(Pin pin)
{
    pins.push_back(pin);
}



std::vector<Pin>& Component::getPins()
{
    return pins;
}