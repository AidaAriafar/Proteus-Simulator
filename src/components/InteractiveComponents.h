#ifndef INTERACTIVE_COMPONENTS_H
#define INTERACTIVE_COMPONENTS_H

#include "../core/Component.h"

#include <array>

class PushButton : public Component
{
private:
    bool pressed;

public:
    PushButton(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    void press();
    void release();
    bool isPressed() const;
};

class SevenSegmentDisplay : public Component
{
private:
    std::array<bool, 7> segments;

public:
    SevenSegmentDisplay(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    void setSegmentState(std::size_t index, bool enabled);
    bool getSegmentState(std::size_t index) const;
    std::array<bool, 7> getSegments() const;
};

#endif
