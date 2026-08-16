#include "Component.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
std::string rotationToString(Rotation rotation)
{
    return std::to_string(static_cast<int>(rotation));
}

PinDirection rotateDirection(
    PinDirection direction,
    Rotation rotation
)
{
    const int turns = static_cast<int>(rotation) / 90;
    for(int i = 0; i < turns; ++i)
    {
        if(direction == PinDirection::LEFT) direction = PinDirection::UP;
        else if(direction == PinDirection::UP) direction = PinDirection::RIGHT;
        else if(direction == PinDirection::RIGHT) direction = PinDirection::DOWN;
        else direction = PinDirection::LEFT;
    }
    return direction;
}

PinDirection mirrorDirectionHorizontal(PinDirection direction)
{
    if(direction == PinDirection::LEFT) return PinDirection::RIGHT;
    if(direction == PinDirection::RIGHT) return PinDirection::LEFT;
    return direction;
}

PinDirection mirrorDirectionVertical(PinDirection direction)
{
    if(direction == PinDirection::UP) return PinDirection::DOWN;
    if(direction == PinDirection::DOWN) return PinDirection::UP;
    return direction;
}
}


Component::Component(
    int id,
    float x,
    float y,
    float width,
    float height,
    ComponentCategory category
)
{
    this->id = id;

    this->x = x;

    this->y = y;

    this->width = width;

    this->height = height;

    this->label = "U" + std::to_string(id);

    this->rotation = Rotation::DEG_0;

    this->mirroredHorizontally = false;

    this->mirroredVertically = false;

    this->category = category;

    this->selected = false;
}


std::vector<PropertyDescriptor> Component::getProperties() const
{
    return {
        {"label", "Label", PropertyKind::Text, label, "", true, {}},
        {"rotation", "Rotation", PropertyKind::Choice, rotationToString(rotation), "deg", true, {"0", "90", "180", "270"}},
        {"mirror_h", "Mirror Horizontal", PropertyKind::Boolean, mirroredHorizontally ? "true" : "false", "", true, {}},
        {"mirror_v", "Mirror Vertical", PropertyKind::Boolean, mirroredVertically ? "true" : "false", "", true, {}}
    };
}


bool Component::setProperty(
    const std::string& key,
    const std::string& value,
    std::string& error
)
{
    if(key == "label")
    {
        if(value.empty())
        {
            error = "Label cannot be empty.";
            return false;
        }
        label = value;
        return true;
    }

    if(key == "rotation")
    {
        if(value == "0") setRotation(Rotation::DEG_0);
        else if(value == "90") setRotation(Rotation::DEG_90);
        else if(value == "180") setRotation(Rotation::DEG_180);
        else if(value == "270") setRotation(Rotation::DEG_270);
        else
        {
            error = "Rotation must be 0, 90, 180 or 270 degrees.";
            return false;
        }
        return true;
    }

    if(key == "mirror_h")
    {
        mirroredHorizontally = (value == "true" || value == "1");
        updatePinTransforms();
        return true;
    }

    if(key == "mirror_v")
    {
        mirroredVertically = (value == "true" || value == "1");
        updatePinTransforms();
        return true;
    }

    error = "Unknown property: " + key;
    return false;
}


int Component::getID() const
{
    return id;
}


float Component::getX() const
{
    return x;
}


float Component::getY() const
{
    return y;
}


void Component::setPosition(
    float newX,
    float newY
)
{
    x = newX;
    y = newY;
    updatePinTransforms();
}


void Component::moveBy(
    float deltaX,
    float deltaY
)
{
    setPosition(x + deltaX, y + deltaY);
}


Rect Component::getBoundingBox() const
{
    return {x - width / 2.0f, y - height / 2.0f, width, height};
}


bool Component::contains(
    float pointX,
    float pointY
) const
{
    const Rect box = getBoundingBox();
    return pointX >= box.x && pointX <= box.x + box.width &&
           pointY >= box.y && pointY <= box.y + box.height;
}


bool Component::intersects(
    const Rect& rectangle
) const
{
    const Rect box = getBoundingBox();
    return !(box.x + box.width < rectangle.x ||
             rectangle.x + rectangle.width < box.x ||
             box.y + box.height < rectangle.y ||
             rectangle.y + rectangle.height < box.y);
}


std::string Component::getLabel() const
{
    return label;
}


void Component::setLabel(
    const std::string& newLabel
)
{
    label = newLabel;
}


Rotation Component::getRotation() const
{
    return rotation;
}


void Component::setRotation(
    Rotation newRotation
)
{
    rotation = newRotation;
    updatePinTransforms();
}


void Component::rotateClockwise()
{
    if(rotation == Rotation::DEG_0) setRotation(Rotation::DEG_90);
    else if(rotation == Rotation::DEG_90) setRotation(Rotation::DEG_180);
    else if(rotation == Rotation::DEG_180) setRotation(Rotation::DEG_270);
    else setRotation(Rotation::DEG_0);
}


void Component::mirrorHorizontal()
{
    mirroredHorizontally = !mirroredHorizontally;
    updatePinTransforms();
}


void Component::mirrorVertical()
{
    mirroredVertically = !mirroredVertically;
    updatePinTransforms();
}


bool Component::isMirroredHorizontally() const
{
    return mirroredHorizontally;
}


bool Component::isMirroredVertically() const
{
    return mirroredVertically;
}


ComponentCategory Component::getCategory() const
{
    return category;
}


bool Component::isSelected() const
{
    return selected;
}


void Component::setSelected(
    bool value
)
{
    selected = value;
}


void Component::addPin(Pin pin)
{
    pin.setLocalPosition(pin.getX() - x, pin.getY() - y);
    pins.push_back(pin);
    updatePinTransforms();
}


std::vector<Pin>& Component::getPins()
{
    return pins;
}


const std::vector<Pin>& Component::getPins() const
{
    return pins;
}


void Component::updatePinTransforms()
{
    for(auto& pin : pins)
    {
        float localX = pin.getLocalX();
        float localY = pin.getLocalY();
        PinDirection direction = pin.getBaseDirection();

        if(mirroredHorizontally)
        {
            localX = -localX;
            direction = mirrorDirectionHorizontal(direction);
        }

        if(mirroredVertically)
        {
            localY = -localY;
            direction = mirrorDirectionVertical(direction);
        }

        const float sourceX = localX;
        const float sourceY = localY;
        if(rotation == Rotation::DEG_90)
        {
            localX = -sourceY;
            localY = sourceX;
        }
        else if(rotation == Rotation::DEG_180)
        {
            localX = -sourceX;
            localY = -sourceY;
        }
        else if(rotation == Rotation::DEG_270)
        {
            localX = sourceY;
            localY = -sourceX;
        }

        direction = rotateDirection(direction, rotation);
        pin.setPosition(x + localX, y + localY);
        pin.setDirection(direction);
    }
}
