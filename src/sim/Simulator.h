#ifndef SIMULATOR_H
#define SIMULATOR_H


#include "../core/Component.h"
#include "../drc/DRCChecker.h"
#include "../wiring/Wiring.h"

#include <map>
#include <vector>

enum class LogicLevel
{
    Low,
    High,
    Floating
};

enum class SimState
{
    Stopped,
    Running,
    Paused
};

struct NetValue
{
    LogicLevel level = LogicLevel::Floating;
    float voltage = 0.0f;
};

class Simulator
{
private:
    SimState state;
    double simTime;
    std::map<int, NetValue> netValues;

    void evaluate(const std::vector<Component*>& components, WireManager& wireManager, SimulationLog* log);

public:
    Simulator();

    SimState getState() const;
    double getTime() const;

    void run();
    void pause();
    void stop(const std::vector<Component*>& components);

    void tick(double dt, const std::vector<Component*>& components, WireManager& wireManager, SimulationLog* log);
    void step(const std::vector<Component*>& components, WireManager& wireManager, SimulationLog* log);

    bool valueForWire(int wireID, const WireManager& wireManager,
                      const std::vector<const Component*>& components, NetValue& out) const;
    const std::map<int, NetValue>& values() const;
};

#endif
