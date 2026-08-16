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


    addPin(
        Pin(
            1,
            "Input",
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
            "Output",
            x + 20,
            y,
            PinType::OUTPUT,
            true,
            PinDirection::RIGHT
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


std::string Switch::getType() const
{
    return "Switch";
}


std::unique_ptr<Component> Switch::clone(
    int newID
) const
{
    auto copy = std::make_unique<Switch>(newID, x, y);
    copy->setLabel(getLabel());
    if(state) copy->turnOn();
    copy->setRotation(getRotation());
    if(isMirroredHorizontally()) copy->mirrorHorizontal();
    if(isMirroredVertically()) copy->mirrorVertical();
    return copy;
}


std::vector<PropertyDescriptor> Switch::getProperties() const
{
    auto properties = Component::getProperties();
    properties.push_back({"closed", "Closed", PropertyKind::Boolean, state ? "true" : "false", "", true, {}});
    return properties;
}


bool Switch::setProperty(
    const std::string& key,
    const std::string& value,
    std::string& error
)
{
    if(key == "closed")
    {
        state = (value == "true" || value == "1" || value == "on");
        return true;
    }

    return Component::setProperty(key, value, error);
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
