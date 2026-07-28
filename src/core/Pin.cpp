#include "Pin.h"


Pin::Pin(
    int id,
    std::string name,
    float x,
    float y,
    PinType type
)
{
    this->id=id;
    this->name=name;

    this->x=x;
    this->y=y;

    this->type=type;

    connected=false;
}



int Pin::getID()
{
    return id;
}



std::string Pin::getName()
{
    return name;
}



float Pin::getX()
{
    return x;
}



float Pin::getY()
{
    return y;
}



PinType Pin::getType()
{
    return type;
}



bool Pin::isConnected()
{
    return connected;
}



void Pin::connect()
{
    connected=true;
}



void Pin::disconnect()
{
    connected=false;
}