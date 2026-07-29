#ifndef SOURCES_H
#define SOURCES_H

#include "../core/Component.h"

class GND : public Component
{
public:
    GND(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
};

class DCVoltageSource : public Component
{
protected:
    float voltage;

public:
    DCVoltageSource(int id, float x, float y, float voltage = 5.0f);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    float getVoltage() const;
    void setVoltage(float newVoltage);
};

class Battery : public DCVoltageSource
{
private:
    float internalResistance;

public:
    Battery(int id, float x, float y, float voltage = 9.0f, float internalResistance = 0.0f);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    float getInternalResistance() const;
};

class PulseSource : public Component
{
private:
    float lowVoltage;
    float highVoltage;
    float frequency;
    float dutyCycle;
    float phase;

public:
    PulseSource(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    float getLowVoltage() const;
    float getHighVoltage() const;
    float getFrequency() const;
    float getDutyCycle() const;
    float getPhase() const;
};

#endif
