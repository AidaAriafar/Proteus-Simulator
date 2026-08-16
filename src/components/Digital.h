#ifndef DIGITAL_H
#define DIGITAL_H


#include "../core/Component.h"

#include <array>

enum class GateKind
{
    AND,
    OR,
    NOT,
    XOR,
    NAND
};

class LogicGate : public Component
{
private:
    GateKind kind;

public:
    LogicGate(int id, float x, float y, GateKind kind);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    GateKind getKind() const;
    bool evaluate(bool a, bool b) const;
    std::size_t inputCount() const;
};

class DFlipFlop : public Component
{
private:
    bool q;
    bool lastClock;

public:
    DFlipFlop(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;

    bool clockTick(bool clockLevel, bool dLevel);
    void reset();
    bool getQ() const;
};

class ADConverter : public Component
{
private:
    float refVoltage;

public:
    ADConverter(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    float getRefVoltage() const;
    std::array<bool, 3> convert(float inputVoltage) const;
};

class DAConverter : public Component
{
private:
    float refVoltage;

public:
    DAConverter(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    std::vector<PropertyDescriptor> getProperties() const override;
    bool setProperty(const std::string& key, const std::string& value, std::string& error) override;
    float getRefVoltage() const;
    float convert(bool b2, bool b1, bool b0) const;
};

class Voltmeter : public Component
{
private:
    float reading;

public:
    Voltmeter(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    void setReading(float value);
    float getReading() const;
};

class Ammeter : public Component
{
private:
    float reading;

public:
    Ammeter(int id, float x, float y);
    void draw() override;
    std::string getType() const override;
    std::unique_ptr<Component> clone(int newID) const override;
    void setReading(float value);
    float getReading() const;
};

#endif
