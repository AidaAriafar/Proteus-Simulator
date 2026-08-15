#include "ComponentLibrary.h"

#include "../components/InteractiveComponents.h"
#include "../components/LED.h"
#include "../components/PassiveComponents.h"
#include "../components/Resistor.h"
#include "../components/Sources.h"
#include "../components/Switch.h"
#include "../components/Digital.h"

#include <algorithm>
#include <memory>


ComponentLibrary::ComponentLibrary()
{
    registerComponent("GND");
    registerComponent("DCVoltageSource");
    registerComponent("Battery");
    registerComponent("PulseSource");
    registerComponent("Resistor");
    registerComponent("Capacitor");
    registerComponent("Inductor");
    registerComponent("Switch");
    registerComponent("PushButton");
    registerComponent("LED");
    registerComponent("SevenSegmentDisplay");
    registerComponent("GateAND");
    registerComponent("GateOR");
    registerComponent("GateNOT");
    registerComponent("GateXOR");
    registerComponent("GateNAND");
    registerComponent("DFlipFlop");
    registerComponent("ADC");
    registerComponent("DAC");
    registerComponent("Voltmeter");
    registerComponent("Ammeter");
}


void ComponentLibrary::registerComponent(
    const std::string& name
)
{
    if(!exists(name))
    {
        if(name == "GND")
        {
            components.push_back({name, "GND", ComponentCategory::Source});
            factories.push_back([](int id, float x, float y){ return std::make_unique<GND>(id, x, y); });
        }
        else if(name == "DCVoltageSource")
        {
            components.push_back({name, "DC Voltage Source", ComponentCategory::Source});
            factories.push_back([](int id, float x, float y){ return std::make_unique<DCVoltageSource>(id, x, y); });
        }
        else if(name == "Battery")
        {
            components.push_back({name, "Battery", ComponentCategory::Source});
            factories.push_back([](int id, float x, float y){ return std::make_unique<Battery>(id, x, y); });
        }
        else if(name == "PulseSource")
        {
            components.push_back({name, "Pulse Source", ComponentCategory::Source});
            factories.push_back([](int id, float x, float y){ return std::make_unique<PulseSource>(id, x, y); });
        }
        else if(name == "Resistor")
        {
            components.push_back({name, "Resistor", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<Resistor>(id, x, y, 1000); });
        }
        else if(name == "Capacitor")
        {
            components.push_back({name, "Capacitor", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<Capacitor>(id, x, y); });
        }
        else if(name == "Inductor")
        {
            components.push_back({name, "Inductor", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<Inductor>(id, x, y); });
        }
        else if(name == "Switch")
        {
            components.push_back({name, "Switch", ComponentCategory::Interactive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<Switch>(id, x, y); });
        }
        else if(name == "PushButton")
        {
            components.push_back({name, "Push Button", ComponentCategory::Interactive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<PushButton>(id, x, y); });
        }
        else if(name == "LED")
        {
            components.push_back({name, "Coloured LED", ComponentCategory::Output});
            factories.push_back([](int id, float x, float y){ return std::make_unique<LED>(id, x, y); });
        }
        else if(name == "GateAND")
        {
            components.push_back({name, "AND Gate", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<LogicGate>(id, x, y, GateKind::AND); });
        }
        else if(name == "GateOR")
        {
            components.push_back({name, "OR Gate", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<LogicGate>(id, x, y, GateKind::OR); });
        }
        else if(name == "GateNOT")
        {
            components.push_back({name, "NOT Gate", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<LogicGate>(id, x, y, GateKind::NOT); });
        }
        else if(name == "GateXOR")
        {
            components.push_back({name, "XOR Gate", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<LogicGate>(id, x, y, GateKind::XOR); });
        }
        else if(name == "GateNAND")
        {
            components.push_back({name, "NAND Gate", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<LogicGate>(id, x, y, GateKind::NAND); });
        }
        else if(name == "DFlipFlop")
        {
            components.push_back({name, "D Flip-Flop", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<DFlipFlop>(id, x, y); });
        }
        else if(name == "ADC")
        {
            components.push_back({name, "ADC (3-bit)", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<ADConverter>(id, x, y); });
        }
        else if(name == "DAC")
        {
            components.push_back({name, "DAC (3-bit)", ComponentCategory::Passive});
            factories.push_back([](int id, float x, float y){ return std::make_unique<DAConverter>(id, x, y); });
        }
        else if(name == "Voltmeter")
        {
            components.push_back({name, "Voltmeter", ComponentCategory::Output});
            factories.push_back([](int id, float x, float y){ return std::make_unique<Voltmeter>(id, x, y); });
        }
        else if(name == "Ammeter")
        {
            components.push_back({name, "Ammeter", ComponentCategory::Output});
            factories.push_back([](int id, float x, float y){ return std::make_unique<Ammeter>(id, x, y); });
        }
        else if(name == "SevenSegmentDisplay")
        {
            components.push_back({name, "Seven Segment Display", ComponentCategory::Output});
            factories.push_back([](int id, float x, float y){ return std::make_unique<SevenSegmentDisplay>(id, x, y); });
        }
    }
}


std::vector<std::string>
ComponentLibrary::getComponents() const
{
    std::vector<std::string> names;
    for(const auto& component : components)
    {
        names.push_back(component.name);
    }
    return names;
}


std::vector<LibraryItem>
ComponentLibrary::getItems() const
{
    return components;
}


bool ComponentLibrary::exists(
    const std::string& name
) const
{
    return std::find_if(
        components.begin(),
        components.end(),
        [&name](const LibraryItem& item)
        {
            return item.name == name;
        }
    ) != components.end();
}


std::unique_ptr<Component>
ComponentLibrary::createComponent(
    const std::string& name,
    int id,
    float x,
    float y
) const
{
    for(std::size_t index = 0; index < components.size(); ++index)
    {
        if(components[index].name == name)
        {
            return factories[index](id, x, y);
        }
    }
    return nullptr;
}
