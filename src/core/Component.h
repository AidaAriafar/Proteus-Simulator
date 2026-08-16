#ifndef COMPONENT_H
#define COMPONENT_H

#include "Pin.h"

#include <memory>
#include <vector>
#include <string>

enum class Rotation
{
    DEG_0 = 0,
    DEG_90 = 90,
    DEG_180 = 180,
    DEG_270 = 270
};

enum class ComponentCategory
{
    Source,
    Passive,
    Interactive,
    Output
};

struct Rect
{
    float x;
    float y;
    float width;
    float height;
};

enum class PropertyKind
{
    Text,
    Number,
    Boolean,
    Choice
};

struct PropertyDescriptor
{
    std::string key;
    std::string displayName;
    PropertyKind kind;
    std::string value;
    std::string unit;
    bool editable;
    std::vector<std::string> choices;
};

class Component
{

protected:

    int id;

    float x;
    float y;
    float width;
    float height;
    std::string label;
    Rotation rotation;
    bool mirroredHorizontally;
    bool mirroredVertically;
    ComponentCategory category;
    bool selected;

    std::vector<Pin> pins;

    void updatePinTransforms();


public:

    Component(
        int id,
        float x,
        float y,
        float width = 60,
        float height = 40,
        ComponentCategory category = ComponentCategory::Passive
    );


    virtual ~Component() = default;


    virtual void draw() = 0;


    virtual std::string getType() const = 0;


    virtual std::unique_ptr<Component> clone(
        int newID
    ) const = 0;


    virtual std::vector<PropertyDescriptor> getProperties() const;


    virtual bool setProperty(
        const std::string& key,
        const std::string& value,
        std::string& error
    );


    int getID() const;


    float getX() const;


    float getY() const;


    void setPosition(
        float newX,
        float newY
    );


    void moveBy(
        float deltaX,
        float deltaY
    );


    Rect getBoundingBox() const;


    bool contains(
        float pointX,
        float pointY
    ) const;


    bool intersects(
        const Rect& rectangle
    ) const;


    std::string getLabel() const;


    void setLabel(
        const std::string& newLabel
    );


    Rotation getRotation() const;


    void setRotation(
        Rotation newRotation
    );


    void rotateClockwise();


    void mirrorHorizontal();


    void mirrorVertical();


    bool isMirroredHorizontally() const;


    bool isMirroredVertically() const;


    ComponentCategory getCategory() const;


    bool isSelected() const;


    void setSelected(
        bool value
    );


    void addPin(Pin pin);


    std::vector<Pin>& getPins();


    const std::vector<Pin>& getPins() const;

};


#endif
