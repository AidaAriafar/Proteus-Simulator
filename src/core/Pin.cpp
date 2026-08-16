#include "Pin.h"


Pin::Pin(
    int id,
    std::string name,
    float x,
    float y,
    PinType type,
    bool required,
    PinDirection direction
)
{
    this->id=id;
    this->name=name;

    this->x=x;
    this->y=y;
    this->localX=x;
    this->localY=y;

    this->type=type;
    this->baseDirection=direction;
    this->direction=direction;

    connected=false;
    this->required=required;
}


int Pin::getID() const
{
    return id;
}


std::string Pin::getName() const
{
    return name;
}


float Pin::getX() const
{
    return x;
}


float Pin::getY() const
{
    return y;
}


float Pin::getLocalX() const
{
    return localX;
}


float Pin::getLocalY() const
{
    return localY;
}


PinType Pin::getType() const
{
    return type;
}


PinDirection Pin::getDirection() const
{
    return direction;
}


PinDirection Pin::getBaseDirection() const
{
    return baseDirection;
}


bool Pin::isRequired() const
{
    return required;
}


bool Pin::isConnected() const
{
    return connected;
}


void Pin::setPosition(
    float newX,
    float newY
)
{
    x = newX;
    y = newY;
}


void Pin::setLocalPosition(
    float newLocalX,
    float newLocalY
)
{
    localX = newLocalX;
    localY = newLocalY;
}


void Pin::setDirection(
    PinDirection newDirection
)
{
    direction = newDirection;
}


void Pin::connect()
{
    connected=true;
}


void Pin::disconnect()
{
    connected=false;
}
