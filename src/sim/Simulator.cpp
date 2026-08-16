#include "Simulator.h"

#include "../components/Digital.h"
#include "../components/InteractiveComponents.h"
#include "../components/LED.h"
#include "../components/Resistor.h"
#include "../components/Sources.h"
#include "../components/Switch.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace
{
constexpr float kHighThreshold = 2.5f;

struct SuperNet
{
    NetValue value;
    bool drivenHigh = false;
    bool drivenLow = false;
    float sourceVoltage = 0.0f;
    float seriesResistance = 0.0f;
};

struct GroupUnion
{
    std::map<int, int> parent;

    void ensure(int id)
    {
        if (parent.find(id) == parent.end())
        {
            parent[id] = id;
        }
    }

    int find(int id)
    {
        ensure(id);
        while (parent[id] != id)
        {
            parent[id] = parent[parent[id]];
            id = parent[id];
        }
        return id;
    }

    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b)
        {
            parent[a] = b;
        }
    }
};

bool levelHigh(const NetValue& value)
{
    return value.level == LogicLevel::High || value.voltage > kHighThreshold;
}
}

Simulator::Simulator()
    : state(SimState::Stopped)
    , simTime(0.0)
{
}

SimState Simulator::getState() const
{
    return state;
}

double Simulator::getTime() const
{
    return simTime;
}

void Simulator::run()
{
    state = SimState::Running;
}

void Simulator::pause()
{
    if (state == SimState::Running)
    {
        state = SimState::Paused;
    }
}

void Simulator::stop(const std::vector<Component*>& components)
{
    state = SimState::Stopped;
    simTime = 0.0;
    netValues.clear();
    for (Component* component : components)
    {
        if (auto* led = dynamic_cast<LED*>(component))
        {
            led->turnOff();
        }
        else if (auto* flipFlop = dynamic_cast<DFlipFlop*>(component))
        {
            flipFlop->reset();
        }
        else if (auto* segment = dynamic_cast<SevenSegmentDisplay*>(component))
        {
            for (std::size_t i = 0; i < 7; ++i)
            {
                segment->setSegmentState(i, false);
            }
        }
        else if (auto* voltmeter = dynamic_cast<Voltmeter*>(component))
        {
            voltmeter->setReading(0.0f);
        }
        else if (auto* ammeter = dynamic_cast<Ammeter*>(component))
        {
            ammeter->setReading(0.0f);
        }
    }
}

void Simulator::tick(double dt, const std::vector<Component*>& components, WireManager& wireManager, SimulationLog* log)
{
    if (state != SimState::Running)
    {
        return;
    }
    simTime += dt;
    evaluate(components, wireManager, log);
}

void Simulator::step(const std::vector<Component*>& components, WireManager& wireManager, SimulationLog* log)
{
    simTime += 0.1;
    evaluate(components, wireManager, log);
}

void Simulator::evaluate(const std::vector<Component*>& components, WireManager& wireManager, SimulationLog* log)
{
    std::vector<const Component*> constComponents;
    constComponents.reserve(components.size());
    for (Component* component : components)
    {
        constComponents.push_back(component);
    }
    const auto nets = wireManager.buildNets(constComponents);

    std::map<std::pair<int, int>, int> pinNet;
    for (const auto& net : nets)
    {
        for (const auto& pin : net.pins)
        {
            pinNet[pin] = net.id;
        }
    }
    auto netOfPin = [&](const Component* component, int pinIndex) -> int
    {
        const auto it = pinNet.find({component->getID(), pinIndex});
        return it == pinNet.end() ? -1 : it->second;
    };

    GroupUnion groups;
    for (const auto& net : nets)
    {
        groups.ensure(net.id);
    }
    float mergedResistance = 0.0f;
    for (Component* component : components)
    {
        bool conducts = false;
        float resistance = 0.0f;
        if (auto* toggle = dynamic_cast<Switch*>(component))
        {
            conducts = toggle->isOn();
        }
        else if (auto* button = dynamic_cast<PushButton*>(component))
        {
            conducts = button->isPressed();
        }
        else if (auto* resistor = dynamic_cast<Resistor*>(component))
        {
            conducts = true;
            resistance = resistor->getResistance();
        }
        else if (component->getType() == "Inductor" || dynamic_cast<Ammeter*>(component) != nullptr)
        {
            conducts = true;
        }
        if (conducts)
        {
            const int netA = netOfPin(component, 0);
            const int netB = netOfPin(component, 1);
            if (netA >= 0 && netB >= 0)
            {
                groups.unite(netA, netB);
                mergedResistance += resistance;
            }
        }
    }

    std::map<int, SuperNet> superNets;
    auto superOf = [&](int netID) -> SuperNet&
    {
        return superNets[groups.find(netID)];
    };

    std::map<int, NetValue> previous;
    for (int iteration = 0; iteration < 24; ++iteration)
    {
        for (auto& entry : superNets)
        {
            entry.second.drivenHigh = false;
            entry.second.drivenLow = false;
        }

        auto drive = [&](const Component* component, int pinIndex, bool high, float voltage)
        {
            const int netID = netOfPin(component, pinIndex);
            if (netID < 0)
            {
                return;
            }
            SuperNet& super = superOf(netID);
            super.value.level = high ? LogicLevel::High : LogicLevel::Low;
            super.value.voltage = voltage;
            if (high)
            {
                super.drivenHigh = true;
                super.sourceVoltage = std::max(super.sourceVoltage, voltage);
            }
            else
            {
                super.drivenLow = true;
            }
        };
        auto read = [&](const Component* component, int pinIndex) -> NetValue
        {
            const int netID = netOfPin(component, pinIndex);
            if (netID < 0)
            {
                return {};
            }
            return superOf(netID).value;
        };

        for (Component* component : components)
        {
            if (dynamic_cast<GND*>(component) != nullptr)
            {
                drive(component, 0, false, 0.0f);
            }
            else if (auto* source = dynamic_cast<DCVoltageSource*>(component))
            {
                drive(component, 0, source->getVoltage() > kHighThreshold, source->getVoltage());
                if (component->getPins().size() > 1)
                {
                    drive(component, 1, false, 0.0f);
                }
            }
            else if (auto* pulse = dynamic_cast<PulseSource*>(component))
            {
                const float frequency = std::max(0.01f, pulse->getFrequency());
                const double period = 1.0 / static_cast<double>(frequency);
                const double duty = std::min(std::max(static_cast<double>(pulse->getDutyCycle()) / 100.0, 0.0), 1.0);
                const double phaseOffset = static_cast<double>(pulse->getPhase()) / 360.0 * period;
                double phaseTime = std::fmod(simTime + phaseOffset, period);
                if (phaseTime < 0.0)
                {
                    phaseTime += period;
                }
                const bool high = phaseTime < period * duty;
                drive(component, 0, high, high ? pulse->getHighVoltage() : pulse->getLowVoltage());
                if (component->getPins().size() > 1)
                {
                    drive(component, 1, false, 0.0f);
                }
            }
            else if (auto* gate = dynamic_cast<LogicGate*>(component))
            {
                const bool a = levelHigh(read(component, 0));
                const bool b = gate->inputCount() > 1 ? levelHigh(read(component, 1)) : false;
                const bool out = gate->evaluate(a, b);
                const int outPin = gate->inputCount() > 1 ? 2 : 1;
                drive(component, outPin, out, out ? 5.0f : 0.0f);
            }
            else if (auto* flipFlop = dynamic_cast<DFlipFlop*>(component))
            {
                const bool d = levelHigh(read(component, 0));
                const bool clk = levelHigh(read(component, 1));
                flipFlop->clockTick(clk, d);
                const bool q = flipFlop->getQ();
                drive(component, 2, q, q ? 5.0f : 0.0f);
                drive(component, 3, !q, !q ? 5.0f : 0.0f);
            }
            else if (auto* adc = dynamic_cast<ADConverter*>(component))
            {
                const auto bits = adc->convert(read(component, 0).voltage);
                drive(component, 1, bits[0], bits[0] ? 5.0f : 0.0f);
                drive(component, 2, bits[1], bits[1] ? 5.0f : 0.0f);
                drive(component, 3, bits[2], bits[2] ? 5.0f : 0.0f);
            }
            else if (auto* dac = dynamic_cast<DAConverter*>(component))
            {
                const float out = dac->convert(
                    levelHigh(read(component, 0)),
                    levelHigh(read(component, 1)),
                    levelHigh(read(component, 2)));
                drive(component, 3, out > kHighThreshold, out);
            }
        }

        for (Component* component : components)
        {
            if (auto* led = dynamic_cast<LED*>(component))
            {
                const NetValue anode = read(component, 0);
                const NetValue cathode = read(component, 1);
                const bool lit = anode.voltage - cathode.voltage > 1.5f && cathode.level != LogicLevel::Floating;
                if (lit)
                {
                    led->turnOn();
                }
                else
                {
                    led->turnOff();
                }
            }
            else if (auto* segment = dynamic_cast<SevenSegmentDisplay*>(component))
            {
                const std::size_t count = std::min<std::size_t>(7, component->getPins().size());
                for (std::size_t index = 0; index < count; ++index)
                {
                    segment->setSegmentState(index, levelHigh(read(component, static_cast<int>(index))));
                }
            }
            else if (auto* voltmeter = dynamic_cast<Voltmeter*>(component))
            {
                voltmeter->setReading(read(component, 0).voltage - read(component, 1).voltage);
            }
            else if (auto* ammeter = dynamic_cast<Ammeter*>(component))
            {
                const SuperNet* super = nullptr;
                const int netID = netOfPin(component, 0);
                if (netID >= 0)
                {
                    super = &superOf(netID);
                }
                if (super != nullptr && super->drivenHigh && super->drivenLow && mergedResistance > 0.5f)
                {
                    ammeter->setReading(super->sourceVoltage / mergedResistance);
                }
                else
                {
                    ammeter->setReading(0.0f);
                }
            }
        }

        std::map<int, NetValue> current;
        for (const auto& net : nets)
        {
            current[net.id] = superOf(net.id).value;
        }
        const bool stable = [&]()
        {
            if (current.size() != previous.size())
            {
                return false;
            }
            for (const auto& entry : current)
            {
                const auto it = previous.find(entry.first);
                if (it == previous.end() || it->second.level != entry.second.level ||
                    std::fabs(it->second.voltage - entry.second.voltage) > 0.01f)
                {
                    return false;
                }
            }
            return true;
        }();
        previous = current;
        if (stable && iteration > 0)
        {
            break;
        }
    }
    netValues = previous;

    if (log != nullptr)
    {
        for (const auto& entry : superNets)
        {
            const SuperNet& super = entry.second;
            if (super.drivenHigh && super.drivenLow && super.sourceVoltage > kHighThreshold && mergedResistance < 0.5f)
            {
                log->error("Short circuit detected while simulating (source tied to ground without a load)");
                break;
            }
        }
    }
}

bool Simulator::valueForWire(int wireID, const WireManager& wireManager,
                             const std::vector<const Component*>& components, NetValue& out) const
{
    const auto nets = wireManager.buildNets(components);
    for (const auto& net : nets)
    {
        for (const int id : net.wireIDs)
        {
            if (id == wireID)
            {
                const auto it = netValues.find(net.id);
                if (it == netValues.end())
                {
                    return false;
                }
                out = it->second;
                return true;
            }
        }
    }
    return false;
}

const std::map<int, NetValue>& Simulator::values() const
{
    return netValues;
}
