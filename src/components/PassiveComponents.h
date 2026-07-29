#ifndef PASSIVE_COMPONENTS_H
#define PASSIVE_COMPONENTS_H

#include "../core/Component.h"

class Capacitor : public Component
{
private:
    float capacitance;

public:
    Capacitor(int id, float x, float y, float capacitance = 0.000001f);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    float getCapacitance() const;
};

class Inductor : public Component
{
private:
    float inductance;

public:
    Inductor(int id, float x, float y, float inductance = 0.001f);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    float getInductance() const;
};

#endif
