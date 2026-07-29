#include "PassiveComponents.h"

#include <iostream>
#include <stdexcept>

namespace
{
bool parseNonNegative(const std::string& value, float& target, const std::string& field, std::string& error)
{
    try
    {
        const float parsed = std::stof(value);
        if(parsed < 0)
        {
            error = field + " cannot be negative.";
            return false;
        }
        target = parsed;
        return true;
    }
    catch(const std::exception&)
    {
        error = field + " must be numeric.";
        return false;
    }
}

void copyTransform(const Component& from, Component& to)
{
    to.setLabel(from.getLabel());
    to.setRotation(from.getRotation());
    if(from.isMirroredHorizontally()) to.mirrorHorizontal();
    if(from.isMirroredVertically()) to.mirrorVertical();
}
}

Capacitor::Capacitor(int id, float x, float y, float capacitance)
:
Component(id, x, y, 60, 40, ComponentCategory::Passive),
capacitance(capacitance)
{
    if(capacitance < 0)
    {
        throw std::invalid_argument("Capacitance cannot be negative.");
    }
    addPin(Pin(1, "P1", x - 20, y, PinType::POWER, true, PinDirection::LEFT));
    addPin(Pin(2, "P2", x + 20, y, PinType::POWER, true, PinDirection::RIGHT));
}

void Capacitor::draw()
{
    std::cout << (isSelected() ? "[SELECTED] " : "") << "Drawing capacitor at " << x << ", " << y << std::endl;
}

std::string Capacitor::getType() const
{
    return "Capacitor";
}

std::unique_ptr<Component> Capacitor::clone(int newID) const
{
    auto copy = std::make_unique<Capacitor>(newID, x, y, capacitance);
    copyTransform(*this, *copy);
    return copy;
}

std::vector<PropertyDescriptor> Capacitor::getProperties() const
{
    auto properties = Component::getProperties();
    properties.push_back({"capacitance", "Capacitance", PropertyKind::Number, std::to_string(capacitance), "F", true, {}});
    return properties;
}

bool Capacitor::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    if(key == "capacitance")
    {
        return parseNonNegative(value, capacitance, "Capacitance", error);
    }
    return Component::setProperty(key, value, error);
}

float Capacitor::getCapacitance() const
{
    return capacitance;
}

Inductor::Inductor(int id, float x, float y, float inductance)
:
Component(id, x, y, 60, 40, ComponentCategory::Passive),
inductance(inductance)
{
    if(inductance < 0)
    {
        throw std::invalid_argument("Inductance cannot be negative.");
    }
    addPin(Pin(1, "P1", x - 20, y, PinType::POWER, true, PinDirection::LEFT));
    addPin(Pin(2, "P2", x + 20, y, PinType::POWER, true, PinDirection::RIGHT));
}

void Inductor::draw()
{
    std::cout << (isSelected() ? "[SELECTED] " : "") << "Drawing inductor at " << x << ", " << y << std::endl;
}

std::string Inductor::getType() const
{
    return "Inductor";
}

std::unique_ptr<Component> Inductor::clone(int newID) const
{
    auto copy = std::make_unique<Inductor>(newID, x, y, inductance);
    copyTransform(*this, *copy);
    return copy;
}

std::vector<PropertyDescriptor> Inductor::getProperties() const
{
    auto properties = Component::getProperties();
    properties.push_back({"inductance", "Inductance", PropertyKind::Number, std::to_string(inductance), "H", true, {}});
    return properties;
}

bool Inductor::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    if(key == "inductance")
    {
        return parseNonNegative(value, inductance, "Inductance", error);
    }
    return Component::setProperty(key, value, error);
}

float Inductor::getInductance() const
{
    return inductance;
}
