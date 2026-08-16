#ifndef WIRING_H
#define WIRING_H


#include "../core/Component.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

struct WPoint
{
    float x;
    float y;
};

enum class AnchorKind
{
    PinAnchor,
    JunctionAnchor
};

struct WireAnchor
{
    AnchorKind kind;
    int componentID;
    int pinIndex;
    int junctionID;
};

struct Junction
{
    int id;
    float x;
    float y;
};

struct Wire
{
    int id;
    WireAnchor a;
    WireAnchor b;
    bool selected = false;
};

struct Net
{
    int id;
    std::vector<std::pair<int, int>> pins;
    std::vector<int> wireIDs;
    std::vector<int> junctionIDs;
};

class WireManager
{
private:
    std::vector<Wire> wires;
    std::vector<Junction> junctions;
    int nextWireID;
    int nextJunctionID;

    std::function<const Component*(int)> componentLookup;

    bool resolveAnchor(const WireAnchor& anchor, WPoint& out) const;

public:
    WireManager();

    void setComponentLookup(std::function<const Component*(int)> lookup);

    int addWirePinToPin(int compA, int pinA, int compB, int pinB);
    int addWirePinToJunction(int compA, int pinA, int junctionID);
    int createJunctionOnWire(int wireID, float x, float y);

    const std::vector<Wire>& getWires() const;
    const std::vector<Junction>& getJunctions() const;
    const Junction* getJunction(int id) const;
    Wire* getWire(int id);

    bool findPinNear(float x, float y, float radius, int& componentID, int& pinIndex,
                     const std::vector<const Component*>& components) const;

    int findWireNear(float x, float y, float radius, WPoint* snapped = nullptr) const;

    std::vector<WPoint> routeWire(const Wire& wire) const;
    std::vector<WPoint> routePreview(const WPoint& from, const WPoint& to) const;

    std::vector<WPoint> junctionDots() const;

    void selectWireAt(float x, float y, float radius, bool additive);
    void clearSelection();
    void deleteSelectedWires();
    void deleteWire(int wireID);
    void removeWiresForComponent(int componentID);

    void syncPinConnectionFlags(const std::vector<Component*>& components) const;

    std::vector<Net> buildNets(const std::vector<const Component*>& components) const;

    void clearAll();
    void restoreJunction(int id, float x, float y);
    void restoreWire(int id, const WireAnchor& a, const WireAnchor& b);
    int peekNextWireID() const;

    static float distancePointToSegment(const WPoint& p, const WPoint& a, const WPoint& b, WPoint* closest = nullptr);
};

#endif
