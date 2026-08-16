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
    color = "red";


    addPin(
        Pin(
            1,
            "Anode",
            x - 20,
            y,
            PinType::INPUT,
            true,
            PinDirection::LEFT
        )
    );


    addPin(
        Pin(
            2,
            "Cathode",
            x + 20,
            y,
            PinType::POWER,
            true,
            PinDirection::RIGHT
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
    << " Color: "
    << color
    << " State: "
    << (state ? "ON" : "OFF")
    << std::endl;

}


std::string LED::getType() const
{
    return "LED";
}


std::unique_ptr<Component> LED::clone(
    int newID
) const
{
    auto copy = std::make_unique<LED>(newID, x, y);
    copy->setLabel(getLabel());
    copy->setColor(color);
    if(state) copy->turnOn();
    copy->setRotation(getRotation());
    if(isMirroredHorizontally()) copy->mirrorHorizontal();
    if(isMirroredVertically()) copy->mirrorVertical();
    return copy;
}


std::vector<PropertyDescriptor> LED::getProperties() const
{
    auto properties = Component::getProperties();
    properties.push_back({"color", "Colour", PropertyKind::Choice, color, "", true, {"red", "green", "blue", "yellow", "white"}});
    properties.push_back({"state", "State", PropertyKind::Boolean, state ? "true" : "false", "", true, {}});
    return properties;
}


bool LED::setProperty(
    const std::string& key,
    const std::string& value,
    std::string& error
)
{
    if(key == "color")
    {
        if(value != "red" && value != "green" && value != "blue" && value != "yellow" && value != "white")
        {
            error = "Unsupported LED colour.";
            return false;
        }
        color = value;
        return true;
    }

    if(key == "state")
    {
        state = (value == "true" || value == "1" || value == "on");
        return true;
    }

    return Component::setProperty(key, value, error);
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


std::string LED::getColor() const
{
    return color;
}


void LED::setColor(
    const std::string& newColor
)
{
    color = newColor;
}
