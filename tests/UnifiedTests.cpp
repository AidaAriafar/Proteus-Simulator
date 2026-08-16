
#include "components/Digital.h"
#include "components/LED.h"
#include "components/Switch.h"
#include "editor/ComponentManager.h"
#include "library/ComponentLibrary.h"
#include "persistence/CircuitSerializer.h"
#include "sim/Simulator.h"
#include "wiring/Wiring.h"

#include <cassert>
#include <iostream>

int main()
{
    ComponentLibrary library;
    ComponentManager manager;
    WireManager wires;
    wires.setComponentLookup([&manager](int id) {
        return static_cast<const ComponentManager&>(manager).getComponent(id);
    });
    manager.setComponentDeletedCallback([&wires](int id) { wires.removeWiresForComponent(id); });

    {
        LogicGate gate(99, 0, 0, GateKind::NAND);
        assert(gate.evaluate(true, true) == false);
        assert(gate.evaluate(true, false) == true);
        LogicGate xorGate(98, 0, 0, GateKind::XOR);
        assert(xorGate.evaluate(true, false) == true);
        assert(xorGate.evaluate(true, true) == false);
    }

    {
        ADConverter adc(97, 0, 0);
        const auto bits = adc.convert(5.0f);
        assert(bits[0] && bits[1] && bits[2]);
        DAConverter dac(96, 0, 0);
        assert(dac.convert(true, true, true) > 4.9f);
        assert(dac.convert(false, false, false) < 0.1f);
    }

    Component* source = manager.placeComponent(library, "DCVoltageSource", 100, 100);
    Component* toggle = manager.placeComponent(library, "Switch", 200, 100);
    Component* led = manager.placeComponent(library, "LED", 300, 100);
    Component* ground = manager.placeComponent(library, "GND", 400, 100);
    assert(source && toggle && led && ground);

    const int wire1 = wires.addWirePinToPin(source->getID(), 0, toggle->getID(), 0);
    const int wire2 = wires.addWirePinToPin(toggle->getID(), 1, led->getID(), 0);
    const int wire3 = wires.addWirePinToPin(led->getID(), 1, ground->getID(), 0);
    assert(wire1 > 0 && wire2 > 0 && wire3 > 0);
    (void)wire1; (void)wire2; (void)wire3;

    wires.syncPinConnectionFlags(manager.getAll());
    assert(source->getPins()[0].isConnected());
    const auto nets = wires.buildNets(static_cast<const ComponentManager&>(manager).getAll());
    assert(nets.size() == 3);

    Simulator sim;
    SimulationLog log;
    sim.run();
    sim.tick(0.05, manager.getAll(), wires, &log);
    assert(dynamic_cast<LED*>(led)->isOn() == false);
    dynamic_cast<Switch*>(toggle)->turnOn();
    sim.tick(0.05, manager.getAll(), wires, &log);
    assert(dynamic_cast<LED*>(led)->isOn() == true);
    sim.stop(manager.getAll());
    assert(dynamic_cast<LED*>(led)->isOn() == false);

    Component* probeLED = manager.placeComponent(library, "LED", 300, 260);
    const int hostWire = wires.getWires().front().id;
    const int junction = wires.createJunctionOnWire(hostWire, 150, 100);
    const int junctionWire = wires.addWirePinToJunction(probeLED->getID(), 0, junction);
    assert(junction > 0 && junctionWire > 0);
    (void)junctionWire;
    assert(!wires.junctionDots().empty());

    CanvasSettings canvas;
    const std::string json = CircuitSerializer::toJSON(manager, wires, canvas);
    ComponentManager restoredManager;
    WireManager restoredWires;
    restoredWires.setComponentLookup([&restoredManager](int id) {
        return static_cast<const ComponentManager&>(restoredManager).getComponent(id);
    });
    CanvasSettings restoredCanvas;
    std::string error;
    const bool ok = CircuitSerializer::fromJSON(json, restoredManager, restoredWires, library, restoredCanvas, error);
    if (!ok) { std::cerr << "round trip failed: " << error << "\n"; return 1; }
    assert(restoredManager.componentCount() == manager.componentCount());
    assert(restoredWires.getWires().size() == wires.getWires().size());
    assert(restoredWires.getJunctions().size() == wires.getJunctions().size());

    manager.selectAt(toggle->getX(), toggle->getY());
    const std::size_t before = wires.getWires().size();
    manager.deleteSelected();
    assert(wires.getWires().size() < before);

    {
        ComponentManager clockManager;
        WireManager clockWires;
        clockWires.setComponentLookup([&clockManager](int id) {
            return static_cast<const ComponentManager&>(clockManager).getComponent(id);
        });
        Component* clock = clockManager.placeComponent(library, "PulseSource", 100, 100);
        Component* load = clockManager.placeComponent(library, "Capacitor", 300, 100);
        assert(clock && load);
        const int clockWire = clockWires.addWirePinToPin(clock->getID(), 0, load->getID(), 0);
        Simulator clockSim;
        clockSim.run();
        bool sawHigh = false, sawLow = false;
        for (int step = 0; step < 20; ++step)
        {
            clockSim.step(clockManager.getAll(), clockWires, nullptr);
            NetValue value;
            std::vector<const Component*> constComponents;
            for (const Component* c : static_cast<const ComponentManager&>(clockManager).getAll())
            {
                constComponents.push_back(c);
            }
            if (clockSim.valueForWire(clockWire, clockWires, constComponents, value))
            {
                if (value.voltage > 4.0f) { sawHigh = true; }
                if (value.voltage < 1.0f) { sawLow = true; }
            }
        }
        assert(sawHigh && sawLow);
    }

    std::cout << "UnifiedTests: all assertions passed\n";
    return 0;
}
