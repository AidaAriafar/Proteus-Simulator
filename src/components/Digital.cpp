#include "Digital.h"

#include <cmath>
#include <sstream>

namespace
{
std::string gateName(GateKind kind)
{
    switch (kind)
    {
    case GateKind::AND: return "AND";
    case GateKind::OR: return "OR";
    case GateKind::NOT: return "NOT";
    case GateKind::XOR: return "XOR";
    case GateKind::NAND: return "NAND";
    }
    return "GATE";
}
}

LogicGate::LogicGate(int id, float x, float y, GateKind kind)
    : Component(id, x, y, 60, 40, ComponentCategory::Passive)
    , kind(kind)
{
    setLabel(gateName(kind));
    if (kind == GateKind::NOT)
    {
        addPin(Pin(1, "IN", x - 30, y, PinType::INPUT, true, PinDirection::LEFT));
        addPin(Pin(2, "OUT", x + 30, y, PinType::OUTPUT, true, PinDirection::RIGHT));
    }
    else
    {
        addPin(Pin(1, "A", x - 30, y - 10, PinType::INPUT, true, PinDirection::LEFT));
        addPin(Pin(2, "B", x - 30, y + 10, PinType::INPUT, true, PinDirection::LEFT));
        addPin(Pin(3, "OUT", x + 30, y, PinType::OUTPUT, true, PinDirection::RIGHT));
    }
}

void LogicGate::draw()
{
}

std::string LogicGate::getType() const
{
    return "Gate" + gateName(kind);
}

std::unique_ptr<Component> LogicGate::clone(int newID) const
{
    auto copy = std::make_unique<LogicGate>(newID, x, y, kind);
    copy->setLabel(label);
    copy->setRotation(rotation);
    return copy;
}

GateKind LogicGate::getKind() const
{
    return kind;
}

bool LogicGate::evaluate(bool a, bool b) const
{
    switch (kind)
    {
    case GateKind::AND: return a && b;
    case GateKind::OR: return a || b;
    case GateKind::NOT: return !a;
    case GateKind::XOR: return a != b;
    case GateKind::NAND: return !(a && b);
    }
    return false;
}

std::size_t LogicGate::inputCount() const
{
    return kind == GateKind::NOT ? 1 : 2;
}

DFlipFlop::DFlipFlop(int id, float x, float y)
    : Component(id, x, y, 70, 60, ComponentCategory::Passive)
    , q(false)
    , lastClock(false)
{
    setLabel("DFF");
    addPin(Pin(1, "D", x - 35, y - 15, PinType::INPUT, true, PinDirection::LEFT));
    addPin(Pin(2, "CLK", x - 35, y + 15, PinType::INPUT, true, PinDirection::LEFT));
    addPin(Pin(3, "Q", x + 35, y - 15, PinType::OUTPUT, true, PinDirection::RIGHT));
    addPin(Pin(4, "QB", x + 35, y + 15, PinType::OUTPUT, true, PinDirection::RIGHT));
}

void DFlipFlop::draw()
{
}

std::string DFlipFlop::getType() const
{
    return "DFlipFlop";
}

std::unique_ptr<Component> DFlipFlop::clone(int newID) const
{
    auto copy = std::make_unique<DFlipFlop>(newID, x, y);
    copy->setLabel(label);
    copy->setRotation(rotation);
    return copy;
}

bool DFlipFlop::clockTick(bool clockLevel, bool dLevel)
{
    bool changed = false;
    if (clockLevel && !lastClock)
    {
        if (q != dLevel)
        {
            q = dLevel;
            changed = true;
        }
    }
    lastClock = clockLevel;
    return changed;
}

void DFlipFlop::reset()
{
    q = false;
    lastClock = false;
}

bool DFlipFlop::getQ() const
{
    return q;
}

ADConverter::ADConverter(int id, float x, float y)
    : Component(id, x, y, 80, 70, ComponentCategory::Passive)
    , refVoltage(5.0f)
{
    setLabel("ADC");
    addPin(Pin(1, "AIN", x - 40, y, PinType::INPUT, true, PinDirection::LEFT));
    addPin(Pin(2, "B2", x + 40, y - 20, PinType::OUTPUT, true, PinDirection::RIGHT));
    addPin(Pin(3, "B1", x + 40, y, PinType::OUTPUT, true, PinDirection::RIGHT));
    addPin(Pin(4, "B0", x + 40, y + 20, PinType::OUTPUT, true, PinDirection::RIGHT));
}

void ADConverter::draw()
{
}

std::string ADConverter::getType() const
{
    return "ADC";
}

std::unique_ptr<Component> ADConverter::clone(int newID) const
{
    auto copy = std::make_unique<ADConverter>(newID, x, y);
    copy->refVoltage = refVoltage;
    copy->setLabel(label);
    copy->setRotation(rotation);
    return copy;
}

std::vector<PropertyDescriptor> ADConverter::getProperties() const
{
    auto properties = Component::getProperties();
    std::ostringstream stream;
    stream << refVoltage;
    properties.push_back({"refVoltage", "Reference Voltage", PropertyKind::Number, stream.str(), "V", true, {}});
    return properties;
}

bool ADConverter::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    if (key == "refVoltage")
    {
        try
        {
            const float parsed = std::stof(value);
            if (parsed <= 0.0f)
            {
                error = "Reference voltage must be positive";
                return false;
            }
            refVoltage = parsed;
            return true;
        }
        catch (...)
        {
            error = "Invalid number";
            return false;
        }
    }
    return Component::setProperty(key, value, error);
}

float ADConverter::getRefVoltage() const
{
    return refVoltage;
}

std::array<bool, 3> ADConverter::convert(float inputVoltage) const
{
    float clamped = inputVoltage;
    if (clamped < 0.0f)
    {
        clamped = 0.0f;
    }
    if (clamped > refVoltage)
    {
        clamped = refVoltage;
    }
    int code = static_cast<int>(std::round(clamped / refVoltage * 7.0f));
    if (code > 7)
    {
        code = 7;
    }
    return {(code & 4) != 0, (code & 2) != 0, (code & 1) != 0};
}

DAConverter::DAConverter(int id, float x, float y)
    : Component(id, x, y, 80, 70, ComponentCategory::Passive)
    , refVoltage(5.0f)
{
    setLabel("DAC");
    addPin(Pin(1, "B2", x - 40, y - 20, PinType::INPUT, true, PinDirection::LEFT));
    addPin(Pin(2, "B1", x - 40, y, PinType::INPUT, true, PinDirection::LEFT));
    addPin(Pin(3, "B0", x - 40, y + 20, PinType::INPUT, true, PinDirection::LEFT));
    addPin(Pin(4, "AOUT", x + 40, y, PinType::OUTPUT, true, PinDirection::RIGHT));
}

void DAConverter::draw()
{
}

std::string DAConverter::getType() const
{
    return "DAC";
}

std::unique_ptr<Component> DAConverter::clone(int newID) const
{
    auto copy = std::make_unique<DAConverter>(newID, x, y);
    copy->refVoltage = refVoltage;
    copy->setLabel(label);
    copy->setRotation(rotation);
    return copy;
}

std::vector<PropertyDescriptor> DAConverter::getProperties() const
{
    auto properties = Component::getProperties();
    std::ostringstream stream;
    stream << refVoltage;
    properties.push_back({"refVoltage", "Reference Voltage", PropertyKind::Number, stream.str(), "V", true, {}});
    return properties;
}

bool DAConverter::setProperty(const std::string& key, const std::string& value, std::string& error)
{
    if (key == "refVoltage")
    {
        try
        {
            const float parsed = std::stof(value);
            if (parsed <= 0.0f)
            {
                error = "Reference voltage must be positive";
                return false;
            }
            refVoltage = parsed;
            return true;
        }
        catch (...)
        {
            error = "Invalid number";
            return false;
        }
    }
    return Component::setProperty(key, value, error);
}

float DAConverter::getRefVoltage() const
{
    return refVoltage;
}

float DAConverter::convert(bool b2, bool b1, bool b0) const
{
    const int code = (b2 ? 4 : 0) + (b1 ? 2 : 0) + (b0 ? 1 : 0);
    return refVoltage * static_cast<float>(code) / 7.0f;
}

Voltmeter::Voltmeter(int id, float x, float y)
    : Component(id, x, y, 60, 60, ComponentCategory::Output)
    , reading(0.0f)
{
    setLabel("V");
    addPin(Pin(1, "+", x - 30, y, PinType::INPUT, true, PinDirection::LEFT));
    addPin(Pin(2, "-", x + 30, y, PinType::INPUT, true, PinDirection::RIGHT));
}

void Voltmeter::draw()
{
}

std::string Voltmeter::getType() const
{
    return "Voltmeter";
}

std::unique_ptr<Component> Voltmeter::clone(int newID) const
{
    auto copy = std::make_unique<Voltmeter>(newID, x, y);
    copy->setLabel(label);
    copy->setRotation(rotation);
    return copy;
}

void Voltmeter::setReading(float value)
{
    reading = value;
}

float Voltmeter::getReading() const
{
    return reading;
}

Ammeter::Ammeter(int id, float x, float y)
    : Component(id, x, y, 60, 60, ComponentCategory::Output)
    , reading(0.0f)
{
    setLabel("A");
    addPin(Pin(1, "+", x - 30, y, PinType::POWER, true, PinDirection::LEFT));
    addPin(Pin(2, "-", x + 30, y, PinType::POWER, true, PinDirection::RIGHT));
}

void Ammeter::draw()
{
}

std::string Ammeter::getType() const
{
    return "Ammeter";
}

std::unique_ptr<Component> Ammeter::clone(int newID) const
{
    auto copy = std::make_unique<Ammeter>(newID, x, y);
    copy->setLabel(label);
    copy->setRotation(rotation);
    return copy;
}

void Ammeter::setReading(float value)
{
    reading = value;
}

float Ammeter::getReading() const
{
    return reading;
}
