#include "Resistor.h"

#include "../core/Pin.h"

#include <iostream>
#include <stdexcept>



Resistor::Resistor(
    int id,
    float x,
    float y,
    float resistance
)
:
Component(id, x, y)
{

    if(resistance < 0)
    {
        throw std::invalid_argument("Resistance cannot be negative.");
    }

    this->resistance = resistance;


    addPin(
        Pin(
            1,
            "P1",
            x - 20,
            y,
            PinType::POWER,
            true,
            PinDirection::LEFT
        )
    );


    // Second terminal

    addPin(
        Pin(
            2,
            "P2",
            x + 20,
            y,
            PinType::POWER,
            true,
            PinDirection::RIGHT
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



std::string Resistor::getType() const
{
    return "Resistor";
}



std::unique_ptr<Component> Resistor::clone(
    int newID
) const
{
    auto copy = std::make_unique<Resistor>(newID, x, y, resistance);
    copy->setLabel(getLabel());
    copy->setRotation(getRotation());
    if(isMirroredHorizontally()) copy->mirrorHorizontal();
    if(isMirroredVertically()) copy->mirrorVertical();
    return copy;
}



std::vector<PropertyDescriptor> Resistor::getProperties() const
{
    auto properties = Component::getProperties();
    properties.push_back({"resistance", "Resistance", PropertyKind::Number, std::to_string(resistance), "ohm", true, {}});
    return properties;
}



bool Resistor::setProperty(
    const std::string& key,
    const std::string& value,
    std::string& error
)
{
    if(key == "resistance")
    {
        try
        {
            const float parsed = std::stof(value);
            if(parsed < 0)
            {
                error = "Resistance cannot be negative.";
                return false;
            }
            resistance = parsed;
            return true;
        }
        catch(const std::exception&)
        {
            error = "Resistance must be numeric.";
            return false;
        }
    }

    return Component::setProperty(key, value, error);
}



void Resistor::setResistance(
    float newResistance
)
{
    if(newResistance < 0)
    {
        throw std::invalid_argument("Resistance cannot be negative.");
    }
    resistance = newResistance;
}
