#include "InteractiveComponents.h"

#include <iostream>
#include <stdexcept>

namespace
{
void copyTransform(const Component& from, Component& to)
{
    to.setLabel(from.getLabel());
    to.setRotation(from.getRotation());
    if(from.isMirroredHorizontally()) to.mirrorHorizontal();
    if(from.isMirroredVertically()) to.mirrorVertical();
}

bool boolFromString(const std::string& value)
{
    return value == "true" || value == "1" || value == "on" || value == "pressed";
}
}

PushButton::PushButton(int id, float x, float y)
:
Component(id, x, y, 60, 40, ComponentCategory::Interactive),
pressed(false)
{
    addPin(Pin(1, "Input", x - 20, y, PinType::INPUT, true, PinDirection::LEFT));
    addPin(Pin(2, "Output", x + 20, y, PinType::OUTPUT, true, PinDirection::RIGHT));
}

void PushButton::draw()
{
    std::cout << (isSelected() ? "[SELECTED] " : "") << "Drawing push button at " << x << ", " << y
              << " State: " << (pressed ? "PRESSED" : "RELEASED") << std::endl;
}

std::string PushButton::getType() const
{
    return "PushButton";
}

std::unique_ptr<Component> PushButton::clone(int newID) const
{
    auto copy = std::make_unique<PushButton>(newID, x, y);
    if(pressed) copy->press();
    copyTransform(*this, *copy);
    return copy;
}

std::vector<PropertyDescriptor> PushButton::getProperties() const
{
    auto properties = Component::getProperties();
    properties.push_back({"pressed", "Pressed", PropertyKind::Boolean, pressed ? "true" : "false", "", true, {}});
    return properties;
}

bool PushButton::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    if(key == "pressed")
    {
        pressed = boolFromString(value);
        return true;
    }
    return Component::setProperty(key, value, error);
}

void PushButton::press()
{
    pressed = true;
}

void PushButton::release()
{
    pressed = false;
}

bool PushButton::isPressed() const
{
    return pressed;
}

SevenSegmentDisplay::SevenSegmentDisplay(int id, float x, float y)
:
Component(id, x, y, 80, 110, ComponentCategory::Output),
segments{false, false, false, false, false, false, false}
{
    const char* names[7] = {"A", "B", "C", "D", "E", "F", "G"};
    const float pinY[7] = {-45, -30, -15, 0, 15, 30, 45};
    for(int index = 0; index < 7; ++index)
    {
        addPin(Pin(index + 1, names[index], x - 40, y + pinY[index], PinType::INPUT, true, PinDirection::LEFT));
    }
}

void SevenSegmentDisplay::draw()
{
    std::cout << (isSelected() ? "[SELECTED] " : "") << "Drawing seven segment display at " << x << ", " << y << std::endl;
}

std::string SevenSegmentDisplay::getType() const
{
    return "SevenSegmentDisplay";
}

std::unique_ptr<Component> SevenSegmentDisplay::clone(int newID) const
{
    auto copy = std::make_unique<SevenSegmentDisplay>(newID, x, y);
    copy->segments = segments;
    copyTransform(*this, *copy);
    return copy;
}

std::vector<PropertyDescriptor> SevenSegmentDisplay::getProperties() const
{
    auto properties = Component::getProperties();
    for(std::size_t i = 0; i < segments.size(); ++i)
    {
        properties.push_back({"segment_" + std::to_string(i), "Segment " + std::to_string(i), PropertyKind::Boolean, segments[i] ? "true" : "false", "", true, {}});
    }
    return properties;
}

bool SevenSegmentDisplay::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    const std::string prefix = "segment_";
    if(key.rfind(prefix, 0) == 0)
    {
        const std::size_t index = static_cast<std::size_t>(std::stoul(key.substr(prefix.size())));
        if(index >= segments.size())
        {
            error = "Seven-segment index is out of range.";
            return false;
        }
        segments[index] = boolFromString(value);
        return true;
    }
    return Component::setProperty(key, value, error);
}

void SevenSegmentDisplay::setSegmentState(std::size_t index, bool enabled)
{
    if(index >= segments.size())
    {
        throw std::out_of_range("Seven-segment index is out of range.");
    }
    segments[index] = enabled;
}

bool SevenSegmentDisplay::getSegmentState(std::size_t index) const
{
    if(index >= segments.size())
    {
        throw std::out_of_range("Seven-segment index is out of range.");
    }
    return segments[index];
}

std::array<bool, 7> SevenSegmentDisplay::getSegments() const
{
    return segments;
}
