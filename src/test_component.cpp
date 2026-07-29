#include "components/InteractiveComponents.h"
#include "components/LED.h"
#include "components/PassiveComponents.h"
#include "components/Resistor.h"
#include "components/Sources.h"
#include "components/Switch.h"
#include "drc/DRCChecker.h"
#include "editor/ComponentManager.h"
#include "editor/PropertiesPanel.h"
#include "library/ComponentLibrary.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
bool near(float left, float right)
{
    return std::fabs(left - right) < 0.001f;
}

const Pin& pinAt(const Component& component, std::size_t index)
{
    return component.getPins().at(index);
}

bool hasRule(const std::vector<DRCFinding>& findings, const std::string& rule)
{
    for(const auto& finding : findings)
    {
        if(finding.ruleCode == rule) return true;
    }
    return false;
}

class OptionalProbe : public Component
{
public:
    explicit OptionalProbe(int id = 99)
    :
    Component(id, 0, 0)
    {
        addPin(Pin(1, "Optional", 0, 0, PinType::INPUT, false, PinDirection::RIGHT));
    }

    void draw() override {}
    std::string getType() const override { return "OptionalProbe"; }
    std::unique_ptr<Component> clone(int newID) const override
    {
        return std::make_unique<OptionalProbe>(newID);
    }
};

void testLibraryAndComponents()
{
    ComponentLibrary library;
    assert(library.exists("Resistor"));
    assert(library.exists("Capacitor"));
    assert(library.exists("Inductor"));
    assert(library.exists("GND"));
    assert(library.exists("DCVoltageSource"));
    assert(library.exists("Battery"));
    assert(library.exists("PulseSource"));
    assert(library.exists("Switch"));
    assert(library.exists("PushButton"));
    assert(library.exists("LED"));
    assert(library.exists("SevenSegmentDisplay"));

    auto first = library.createComponent("Resistor", 1, 100, 100);
    auto second = library.createComponent("Resistor", 2, 100, 100);
    assert(first != nullptr);
    assert(second != nullptr);
    assert(first->getID() != second->getID());
    assert(first.get() != second.get());

    Resistor resistor(10, 0, 0, 1000);
    Capacitor capacitor(11, 0, 0, 0.000002f);
    Inductor inductor(12, 0, 0, 0.003f);
    DCVoltageSource source(13, 0, 0, 5.0f);
    Battery battery(14, 0, 0, 9.0f, 0.5f);
    PulseSource pulse(15, 0, 0);
    assert(near(resistor.getResistance(), 1000));
    assert(near(capacitor.getCapacitance(), 0.000002f));
    assert(near(inductor.getInductance(), 0.003f));
    assert(near(source.getVoltage(), 5.0f));
    assert(near(battery.getInternalResistance(), 0.5f));
    assert(near(pulse.getHighVoltage(), 5.0f));

    bool threw = false;
    try { Resistor invalid(16, 0, 0, -1.0f); }
    catch(const std::invalid_argument&) { threw = true; }
    assert(threw);

    std::string error;
    assert(!capacitor.setProperty("capacitance", "-1", error));
    assert(!pulse.setProperty("duty_cycle", "101", error));

    Switch sw(20, 0, 0);
    assert(!sw.isOn());
    sw.toggle();
    assert(sw.isOn());
    sw.turnOff();
    assert(!sw.isOn());

    PushButton button(21, 0, 0);
    assert(!button.isPressed());
    button.press();
    assert(button.isPressed());
    button.release();
    assert(!button.isPressed());

    LED led(22, 0, 0);
    led.setColor("green");
    led.turnOn();
    assert(led.isOn());
    assert(led.getColor() == "green");

    SevenSegmentDisplay display(23, 0, 0);
    display.setSegmentState(0, true);
    display.setSegmentState(3, true);
    assert(display.getSegmentState(0));
    assert(!display.getSegmentState(1));
    assert(display.getSegmentState(3));

    auto clone = resistor.clone(30);
    assert(clone->getID() == 30);
    assert(clone.get() != &resistor);
    assert(clone->getType() == "Resistor");
    clone->setProperty("resistance", "2200", error);
    assert(near(resistor.getResistance(), 1000));
}

void testTransforms()
{
    Resistor resistor(1, 100, 100, 1000);
    assert(near(pinAt(resistor, 0).getX(), 80));
    assert(near(pinAt(resistor, 0).getY(), 100));

    resistor.setRotation(Rotation::DEG_90);
    assert(near(pinAt(resistor, 0).getX(), 100));
    assert(near(pinAt(resistor, 0).getY(), 80));

    resistor.setRotation(Rotation::DEG_180);
    assert(near(pinAt(resistor, 0).getX(), 120));
    assert(near(pinAt(resistor, 0).getY(), 100));

    resistor.setRotation(Rotation::DEG_270);
    assert(near(pinAt(resistor, 0).getX(), 100));
    assert(near(pinAt(resistor, 0).getY(), 120));

    resistor.setRotation(Rotation::DEG_0);
    resistor.mirrorHorizontal();
    assert(near(pinAt(resistor, 0).getX(), 120));
    assert(near(pinAt(resistor, 0).getY(), 100));

    resistor.mirrorVertical();
    assert(near(pinAt(resistor, 0).getX(), 120));
    assert(near(pinAt(resistor, 0).getY(), 100));

    resistor.setRotation(Rotation::DEG_90);
    assert(near(pinAt(resistor, 0).getX(), 100));
    assert(near(pinAt(resistor, 0).getY(), 120));
}

void testEditorAndProperties()
{
    ComponentLibrary library;
    ComponentManager manager;
    manager.setGridSize(10);

    Component* resistor = manager.placeComponent(library, "Resistor", 13, 17);
    Component* led = manager.placeComponent(library, "LED", 80, 20);
    assert(resistor != nullptr);
    assert(led != nullptr);
    assert(resistor->getID() != led->getID());
    assert(near(resistor->getX(), 10));
    assert(near(resistor->getY(), 20));

    assert(manager.selectAt(10, 20));
    assert(manager.getSelectedIDs().size() == 1);
    assert(resistor->isSelected());

    manager.selectInRectangle({0, 0, 100, 50});
    assert(manager.getSelectedIDs().size() == 2);
    assert(resistor->isSelected());
    assert(led->isSelected());

    manager.beginDrag();
    manager.dragSelected(13, 16);
    manager.endDrag();
    assert(near(resistor->getX(), 20));
    assert(near(resistor->getY(), 40));
    assert(near(led->getX(), 90));
    assert(near(led->getY(), 40));

    manager.rotateSelected();
    assert(resistor->getRotation() == Rotation::DEG_90);
    manager.mirrorSelectedHorizontal();
    assert(resistor->isMirroredHorizontally());
    manager.mirrorSelectedVertical();
    assert(resistor->isMirroredVertically());

    PropertiesPanel panel;
    std::string error;
    panel.open(resistor);
    assert(panel.isOpen());
    assert(panel.setPendingValue("label", "R_TEST", error));
    assert(panel.setPendingValue("resistance", "330", error));
    assert(panel.apply(error));
    assert(resistor->getLabel() == "R_TEST");
    assert(near(static_cast<Resistor*>(resistor)->getResistance(), 330));

    panel.setPendingValue("resistance", "470", error);
    panel.cancel();
    assert(near(static_cast<Resistor*>(resistor)->getResistance(), 330));

    int deletedID = -1;
    manager.setComponentDeletedCallback([&deletedID](int id){ deletedID = id; });
    manager.selectAt(resistor->getX(), resistor->getY());
    const int selectedID = manager.getSelectedIDs().front();
    manager.deleteSelected();
    assert(deletedID == selectedID);
    assert(manager.getComponent(selectedID) == nullptr);
    assert(manager.getSelectedIDs().empty());
}

void testDRC()
{
    DRCChecker checker;
    SimulationLog log;

    DCVoltageSource source(1, 0, 0, 5);
    GND ground(2, 0, 0);
    std::vector<const Component*> directComponents{&source, &ground};
    auto directFindings = checker.check(directComponents, {{1, 1, 7}, {2, 1, 7}, {1, 2, 8}}, &log);
    assert(hasRule(directFindings, "DRC_SHORT_SOURCE_GND"));

    log.clear();
    Resistor resistor(3, 0, 0, 1000);
    std::vector<const Component*> safeComponents{&source, &ground, &resistor};
    auto safeFindings = checker.check(
        safeComponents,
        {{1, 1, 1}, {3, 1, 1}, {3, 2, 2}, {2, 1, 2}, {1, 2, 2}},
        &log);
    assert(!hasRule(safeFindings, "DRC_SHORT_SOURCE_GND"));
    assert(!hasRule(safeFindings, "DRC_FLOATING_PIN"));

    LED led(4, 0, 0);
    std::vector<const Component*> floatingComponents{&led};
    auto floatingFindings = checker.check(floatingComponents, {}, nullptr);
    assert(hasRule(floatingFindings, "DRC_FLOATING_PIN"));

    OptionalProbe optional;
    std::vector<const Component*> optionalComponents{&optional};
    auto optionalFindings = checker.check(optionalComponents, {}, nullptr);
    assert(!hasRule(optionalFindings, "DRC_FLOATING_PIN"));

    log.clear();
    auto multipleFindings = checker.check(directComponents, {{1, 1, 7}, {2, 1, 7}}, &log);
    assert(multipleFindings.size() >= 2);
    assert(log.getEntries().size() == multipleFindings.size());
    for(std::size_t index = 1; index < log.getEntries().size(); ++index)
    {
        assert(log.getEntries()[index - 1].sequence < log.getEntries()[index].sequence);
    }
}
}

int main()
{
    testLibraryAndComponents();
    testTransforms();
    testEditorAndProperties();
    testDRC();

    std::cout << "Aida component/editor/DRC tests passed." << std::endl;
    return 0;
}
