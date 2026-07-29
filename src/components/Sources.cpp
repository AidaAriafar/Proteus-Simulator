#include "Sources.h"

#include <iostream>
#include <stdexcept>

namespace
{
bool parseFloat(
    const std::string& value,
    float& parsed,
    std::string& error,
    const std::string& field
)
{
    try
    {
        parsed = std::stof(value);
        return true;
    }
    catch(const std::exception&)
    {
        error = field + " must be numeric.";
        return false;
    }
}

void copyTransform(
    const Component& from,
    Component& to
)
{
    to.setLabel(from.getLabel());
    to.setRotation(from.getRotation());
    if(from.isMirroredHorizontally()) to.mirrorHorizontal();
    if(from.isMirroredVertically()) to.mirrorVertical();
}
}

GND::GND(int id, float x, float y)
:
Component(id, x, y, 40, 30, ComponentCategory::Source)
{
    setLabel("GND" + std::to_string(id));
    addPin(Pin(1, "GND", x, y - 15, PinType::POWER, true, PinDirection::UP));
}

void GND::draw()
{
    std::cout << (isSelected() ? "[SELECTED] " : "") << "Drawing GND at " << x << ", " << y << std::endl;
}

std::string GND::getType() const
{
    return "GND";
}

std::unique_ptr<Component> GND::clone(int newID) const
{
    auto copy = std::make_unique<GND>(newID, x, y);
    copyTransform(*this, *copy);
    return copy;
}

DCVoltageSource::DCVoltageSource(int id, float x, float y, float voltage)
:
Component(id, x, y, 60, 50, ComponentCategory::Source),
voltage(voltage)
{
    addPin(Pin(1, "Positive", x, y - 25, PinType::POWER, true, PinDirection::UP));
    addPin(Pin(2, "Negative", x, y + 25, PinType::POWER, true, PinDirection::DOWN));
}

void DCVoltageSource::draw()
{
    std::cout << (isSelected() ? "[SELECTED] " : "") << "Drawing DC source at " << x << ", " << y << " V=" << voltage << std::endl;
}

std::string DCVoltageSource::getType() const
{
    return "DCVoltageSource";
}

std::unique_ptr<Component> DCVoltageSource::clone(int newID) const
{
    auto copy = std::make_unique<DCVoltageSource>(newID, x, y, voltage);
    copyTransform(*this, *copy);
    return copy;
}

std::vector<PropertyDescriptor> DCVoltageSource::getProperties() const
{
    auto properties = Component::getProperties();
    properties.push_back({"voltage", "Voltage", PropertyKind::Number, std::to_string(voltage), "V", true, {}});
    return properties;
}

bool DCVoltageSource::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    if(key == "voltage")
    {
        return parseFloat(value, voltage, error, "Voltage");
    }
    return Component::setProperty(key, value, error);
}

float DCVoltageSource::getVoltage() const
{
    return voltage;
}

void DCVoltageSource::setVoltage(float newVoltage)
{
    voltage = newVoltage;
}

Battery::Battery(int id, float x, float y, float voltage, float internalResistance)
:
DCVoltageSource(id, x, y, voltage),
internalResistance(internalResistance)
{
    if(internalResistance < 0)
    {
        throw std::invalid_argument("Internal resistance cannot be negative.");
    }
}

void Battery::draw()
{
    std::cout << (isSelected() ? "[SELECTED] " : "") << "Drawing battery at " << x << ", " << y << " V=" << voltage << std::endl;
}

std::string Battery::getType() const
{
    return "Battery";
}

std::unique_ptr<Component> Battery::clone(int newID) const
{
    auto copy = std::make_unique<Battery>(newID, x, y, voltage, internalResistance);
    copyTransform(*this, *copy);
    return copy;
}

std::vector<PropertyDescriptor> Battery::getProperties() const
{
    auto properties = DCVoltageSource::getProperties();
    properties.push_back({"internal_resistance", "Internal Resistance", PropertyKind::Number, std::to_string(internalResistance), "ohm", true, {}});
    return properties;
}

bool Battery::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    if(key == "internal_resistance")
    {
        float parsed = 0;
        if(!parseFloat(value, parsed, error, "Internal resistance")) return false;
        if(parsed < 0)
        {
            error = "Internal resistance cannot be negative.";
            return false;
        }
        internalResistance = parsed;
        return true;
    }
    return DCVoltageSource::setProperty(key, value, error);
}

float Battery::getInternalResistance() const
{
    return internalResistance;
}

PulseSource::PulseSource(int id, float x, float y)
:
Component(id, x, y, 70, 50, ComponentCategory::Source),
lowVoltage(0.0f),
highVoltage(5.0f),
frequency(1.0f),
dutyCycle(50.0f),
phase(0.0f)
{
    addPin(Pin(1, "OUT", x + 35, y, PinType::OUTPUT, true, PinDirection::RIGHT));
    addPin(Pin(2, "GND", x - 35, y, PinType::POWER, true, PinDirection::LEFT));
}

void PulseSource::draw()
{
    std::cout << (isSelected() ? "[SELECTED] " : "") << "Drawing pulse source at " << x << ", " << y << std::endl;
}

std::string PulseSource::getType() const
{
    return "PulseSource";
}

std::unique_ptr<Component> PulseSource::clone(int newID) const
{
    auto copy = std::make_unique<PulseSource>(newID, x, y);
    copy->lowVoltage = lowVoltage;
    copy->highVoltage = highVoltage;
    copy->frequency = frequency;
    copy->dutyCycle = dutyCycle;
    copy->phase = phase;
    copyTransform(*this, *copy);
    return copy;
}

std::vector<PropertyDescriptor> PulseSource::getProperties() const
{
    auto properties = Component::getProperties();
    properties.push_back({"low_voltage", "Low Voltage", PropertyKind::Number, std::to_string(lowVoltage), "V", true, {}});
    properties.push_back({"high_voltage", "High Voltage", PropertyKind::Number, std::to_string(highVoltage), "V", true, {}});
    properties.push_back({"frequency", "Frequency", PropertyKind::Number, std::to_string(frequency), "Hz", true, {}});
    properties.push_back({"duty_cycle", "Duty Cycle", PropertyKind::Number, std::to_string(dutyCycle), "%", true, {}});
    properties.push_back({"phase", "Phase", PropertyKind::Number, std::to_string(phase), "deg", true, {}});
    return properties;
}

bool PulseSource::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    float parsed = 0;
    if(key == "low_voltage")
    {
        if(!parseFloat(value, parsed, error, "Low voltage")) return false;
        lowVoltage = parsed;
        return true;
    }
    if(key == "high_voltage")
    {
        if(!parseFloat(value, parsed, error, "High voltage")) return false;
        highVoltage = parsed;
        return true;
    }
    if(key == "frequency")
    {
        if(!parseFloat(value, parsed, error, "Frequency")) return false;
        if(parsed <= 0)
        {
            error = "Frequency must be positive.";
            return false;
        }
        frequency = parsed;
        return true;
    }
    if(key == "duty_cycle")
    {
        if(!parseFloat(value, parsed, error, "Duty cycle")) return false;
        if(parsed < 0 || parsed > 100)
        {
            error = "Duty cycle must be between 0 and 100.";
            return false;
        }
        dutyCycle = parsed;
        return true;
    }
    if(key == "phase")
    {
        if(!parseFloat(value, parsed, error, "Phase")) return false;
        phase = parsed;
        return true;
    }
    return Component::setProperty(key, value, error);
}

float PulseSource::getLowVoltage() const { return lowVoltage; }
float PulseSource::getHighVoltage() const { return highVoltage; }
float PulseSource::getFrequency() const { return frequency; }
float PulseSource::getDutyCycle() const { return dutyCycle; }
float PulseSource::getPhase() const { return phase; }
