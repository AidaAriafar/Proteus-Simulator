#include "Wiring.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace
{
struct UnionFind
{
    std::vector<int> parent;

    explicit UnionFind(std::size_t n) : parent(n)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            parent[i] = static_cast<int>(i);
        }
    }

    int find(int a)
    {
        while (parent[a] != a)
        {
            parent[a] = parent[parent[a]];
            a = parent[a];
        }
        return a;
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
}

WireManager::WireManager()
    : nextWireID(1)
    , nextJunctionID(1)
{
}

void WireManager::setComponentLookup(std::function<const Component*(int)> lookup)
{
    componentLookup = std::move(lookup);
}

bool WireManager::resolveAnchor(const WireAnchor& anchor, WPoint& out) const
{
    if (anchor.kind == AnchorKind::JunctionAnchor)
    {
        const Junction* junction = getJunction(anchor.junctionID);
        if (junction == nullptr)
        {
            return false;
        }
        out = {junction->x, junction->y};
        return true;
    }
    if (!componentLookup)
    {
        return false;
    }
    const Component* component = componentLookup(anchor.componentID);
    if (component == nullptr)
    {
        return false;
    }
    const auto& pins = component->getPins();
    if (anchor.pinIndex < 0 || static_cast<std::size_t>(anchor.pinIndex) >= pins.size())
    {
        return false;
    }
    out = {pins[static_cast<std::size_t>(anchor.pinIndex)].getX(),
           pins[static_cast<std::size_t>(anchor.pinIndex)].getY()};
    return true;
}

int WireManager::addWirePinToPin(int compA, int pinA, int compB, int pinB)
{
    if (compA == compB && pinA == pinB)
    {
        return -1;
    }
    Wire wire;
    wire.id = nextWireID++;
    wire.a = {AnchorKind::PinAnchor, compA, pinA, -1};
    wire.b = {AnchorKind::PinAnchor, compB, pinB, -1};
    wires.push_back(wire);
    return wire.id;
}

int WireManager::addWirePinToJunction(int compA, int pinA, int junctionID)
{
    if (getJunction(junctionID) == nullptr)
    {
        return -1;
    }
    Wire wire;
    wire.id = nextWireID++;
    wire.a = {AnchorKind::PinAnchor, compA, pinA, -1};
    wire.b = {AnchorKind::JunctionAnchor, -1, -1, junctionID};
    wires.push_back(wire);
    return wire.id;
}

int WireManager::createJunctionOnWire(int wireID, float x, float y)
{
    Wire* host = getWire(wireID);
    if (host == nullptr)
    {
        return -1;
    }
    const auto polyline = routeWire(*host);
    WPoint best = {x, y};
    float bestDistance = 1e9f;
    for (std::size_t i = 0; i + 1 < polyline.size(); ++i)
    {
        WPoint closest;
        const float distance = distancePointToSegment({x, y}, polyline[i], polyline[i + 1], &closest);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = closest;
        }
    }

    Junction junction;
    junction.id = nextJunctionID++;
    junction.x = best.x;
    junction.y = best.y;
    junctions.push_back(junction);

    Wire second;
    second.id = nextWireID++;
    second.a = {AnchorKind::JunctionAnchor, -1, -1, junction.id};
    second.b = host->b;
    host->b = {AnchorKind::JunctionAnchor, -1, -1, junction.id};
    wires.push_back(second);
    return junction.id;
}

const std::vector<Wire>& WireManager::getWires() const
{
    return wires;
}

const std::vector<Junction>& WireManager::getJunctions() const
{
    return junctions;
}

const Junction* WireManager::getJunction(int id) const
{
    for (const auto& junction : junctions)
    {
        if (junction.id == id)
        {
            return &junction;
        }
    }
    return nullptr;
}

Wire* WireManager::getWire(int id)
{
    for (auto& wire : wires)
    {
        if (wire.id == id)
        {
            return &wire;
        }
    }
    return nullptr;
}

bool WireManager::findPinNear(float x, float y, float radius, int& componentID, int& pinIndex,
                              const std::vector<const Component*>& components) const
{
    float bestDistance = radius;
    bool found = false;
    for (const Component* component : components)
    {
        const auto& pins = component->getPins();
        for (std::size_t index = 0; index < pins.size(); ++index)
        {
            const float dx = pins[index].getX() - x;
            const float dy = pins[index].getY() - y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= bestDistance)
            {
                bestDistance = distance;
                componentID = component->getID();
                pinIndex = static_cast<int>(index);
                found = true;
            }
        }
    }
    return found;
}

int WireManager::findWireNear(float x, float y, float radius, WPoint* snapped) const
{
    int bestID = -1;
    float bestDistance = radius;
    for (const auto& wire : wires)
    {
        const auto polyline = routeWire(wire);
        for (std::size_t i = 0; i + 1 < polyline.size(); ++i)
        {
            WPoint closest;
            const float distance = distancePointToSegment({x, y}, polyline[i], polyline[i + 1], &closest);
            if (distance <= bestDistance)
            {
                bestDistance = distance;
                bestID = wire.id;
                if (snapped != nullptr)
                {
                    *snapped = closest;
                }
            }
        }
    }
    return bestID;
}

std::vector<WPoint> WireManager::routeWire(const Wire& wire) const
{
    WPoint a;
    WPoint b;
    if (!resolveAnchor(wire.a, a) || !resolveAnchor(wire.b, b))
    {
        return {};
    }
    return routePreview(a, b);
}

std::vector<WPoint> WireManager::routePreview(const WPoint& from, const WPoint& to) const
{
    std::vector<WPoint> points;
    points.push_back(from);
    const float dx = std::fabs(to.x - from.x);
    const float dy = std::fabs(to.y - from.y);
    if (dx > 0.5f && dy > 0.5f)
    {
        const float midX = (from.x + to.x) * 0.5f;
        points.push_back({midX, from.y});
        points.push_back({midX, to.y});
    }
    else if (dx > 0.5f || dy > 0.5f)
    {
    }
    points.push_back(to);
    return points;
}

std::vector<WPoint> WireManager::junctionDots() const
{
    std::vector<WPoint> dots;
    for (const auto& junction : junctions)
    {
        dots.push_back({junction.x, junction.y});
    }
    std::map<std::pair<int, int>, int> pinUse;
    for (const auto& wire : wires)
    {
        if (wire.a.kind == AnchorKind::PinAnchor)
        {
            ++pinUse[{wire.a.componentID, wire.a.pinIndex}];
        }
        if (wire.b.kind == AnchorKind::PinAnchor)
        {
            ++pinUse[{wire.b.componentID, wire.b.pinIndex}];
        }
    }
    for (const auto& entry : pinUse)
    {
        if (entry.second >= 2 && componentLookup)
        {
            const Component* component = componentLookup(entry.first.first);
            if (component != nullptr)
            {
                const auto& pins = component->getPins();
                if (static_cast<std::size_t>(entry.first.second) < pins.size())
                {
                    dots.push_back({pins[static_cast<std::size_t>(entry.first.second)].getX(),
                                    pins[static_cast<std::size_t>(entry.first.second)].getY()});
                }
            }
        }
    }
    return dots;
}

void WireManager::selectWireAt(float x, float y, float radius, bool additive)
{
    const int hit = findWireNear(x, y, radius, nullptr);
    if (!additive)
    {
        for (auto& wire : wires)
        {
            wire.selected = false;
        }
    }
    if (hit >= 0)
    {
        Wire* wire = getWire(hit);
        if (wire != nullptr)
        {
            wire->selected = !additive ? true : !wire->selected;
        }
    }
}

void WireManager::clearSelection()
{
    for (auto& wire : wires)
    {
        wire.selected = false;
    }
}

void WireManager::deleteSelectedWires()
{
    wires.erase(
        std::remove_if(wires.begin(), wires.end(), [](const Wire& wire) { return wire.selected; }),
        wires.end());
    junctions.erase(
        std::remove_if(
            junctions.begin(),
            junctions.end(),
            [this](const Junction& junction)
            {
                for (const auto& wire : wires)
                {
                    if ((wire.a.kind == AnchorKind::JunctionAnchor && wire.a.junctionID == junction.id) ||
                        (wire.b.kind == AnchorKind::JunctionAnchor && wire.b.junctionID == junction.id))
                    {
                        return false;
                    }
                }
                return true;
            }),
        junctions.end());
}

void WireManager::deleteWire(int wireID)
{
    for (auto& wire : wires)
    {
        wire.selected = wire.id == wireID;
    }
    deleteSelectedWires();
}

void WireManager::removeWiresForComponent(int componentID)
{
    for (auto& wire : wires)
    {
        const bool touches =
            (wire.a.kind == AnchorKind::PinAnchor && wire.a.componentID == componentID) ||
            (wire.b.kind == AnchorKind::PinAnchor && wire.b.componentID == componentID);
        wire.selected = touches;
    }
    deleteSelectedWires();
}

void WireManager::syncPinConnectionFlags(const std::vector<Component*>& components) const
{
    std::set<std::pair<int, int>> connected;
    for (const auto& wire : wires)
    {
        if (wire.a.kind == AnchorKind::PinAnchor)
        {
            connected.insert({wire.a.componentID, wire.a.pinIndex});
        }
        if (wire.b.kind == AnchorKind::PinAnchor)
        {
            connected.insert({wire.b.componentID, wire.b.pinIndex});
        }
    }
    for (Component* component : components)
    {
        auto& pins = component->getPins();
        for (std::size_t index = 0; index < pins.size(); ++index)
        {
            if (connected.count({component->getID(), static_cast<int>(index)}) > 0)
            {
                pins[index].connect();
            }
            else
            {
                pins[index].disconnect();
            }
        }
    }
}

std::vector<Net> WireManager::buildNets(const std::vector<const Component*>& components) const
{
    std::map<std::pair<int, int>, int> pinNode;
    std::map<int, int> junctionNode;
    int nodeCount = 0;
    for (const Component* component : components)
    {
        const auto& pins = component->getPins();
        for (std::size_t index = 0; index < pins.size(); ++index)
        {
            pinNode[{component->getID(), static_cast<int>(index)}] = nodeCount++;
        }
    }
    for (const auto& junction : junctions)
    {
        junctionNode[junction.id] = nodeCount++;
    }

    UnionFind unionFind(static_cast<std::size_t>(nodeCount));
    auto anchorNode = [&](const WireAnchor& anchor) -> int
    {
        if (anchor.kind == AnchorKind::PinAnchor)
        {
            const auto it = pinNode.find({anchor.componentID, anchor.pinIndex});
            return it == pinNode.end() ? -1 : it->second;
        }
        const auto it = junctionNode.find(anchor.junctionID);
        return it == junctionNode.end() ? -1 : it->second;
    };

    for (const auto& wire : wires)
    {
        const int nodeA = anchorNode(wire.a);
        const int nodeB = anchorNode(wire.b);
        if (nodeA >= 0 && nodeB >= 0)
        {
            unionFind.unite(nodeA, nodeB);
        }
    }

    std::set<int> wired;
    for (const auto& wire : wires)
    {
        const int nodeA = anchorNode(wire.a);
        const int nodeB = anchorNode(wire.b);
        if (nodeA >= 0)
        {
            wired.insert(nodeA);
        }
        if (nodeB >= 0)
        {
            wired.insert(nodeB);
        }
    }

    std::map<int, Net> byRoot;
    int nextNetID = 1;
    auto netFor = [&](int node) -> Net&
    {
        const int root = unionFind.find(node);
        auto it = byRoot.find(root);
        if (it == byRoot.end())
        {
            Net net;
            net.id = nextNetID++;
            it = byRoot.emplace(root, net).first;
        }
        return it->second;
    };

    for (const auto& entry : pinNode)
    {
        if (wired.count(entry.second) > 0)
        {
            netFor(entry.second).pins.push_back(entry.first);
        }
    }
    for (const auto& entry : junctionNode)
    {
        if (wired.count(entry.second) > 0)
        {
            netFor(entry.second).junctionIDs.push_back(entry.first);
        }
    }
    for (const auto& wire : wires)
    {
        const int nodeA = anchorNode(wire.a);
        if (nodeA >= 0)
        {
            netFor(nodeA).wireIDs.push_back(wire.id);
        }
    }

    std::vector<Net> nets;
    for (auto& entry : byRoot)
    {
        nets.push_back(std::move(entry.second));
    }
    return nets;
}

void WireManager::clearAll()
{
    wires.clear();
    junctions.clear();
    nextWireID = 1;
    nextJunctionID = 1;
}

void WireManager::restoreJunction(int id, float x, float y)
{
    junctions.push_back({id, x, y});
    nextJunctionID = std::max(nextJunctionID, id + 1);
}

void WireManager::restoreWire(int id, const WireAnchor& a, const WireAnchor& b)
{
    Wire wire;
    wire.id = id;
    wire.a = a;
    wire.b = b;
    wires.push_back(wire);
    nextWireID = std::max(nextWireID, id + 1);
}

int WireManager::peekNextWireID() const
{
    return nextWireID;
}

float WireManager::distancePointToSegment(const WPoint& p, const WPoint& a, const WPoint& b, WPoint* closest)
{
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float lengthSquared = abx * abx + aby * aby;
    float t = 0.0f;
    if (lengthSquared > 1e-9f)
    {
        t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / lengthSquared;
        t = std::max(0.0f, std::min(1.0f, t));
    }
    const WPoint proj = {a.x + t * abx, a.y + t * aby};
    if (closest != nullptr)
    {
        *closest = proj;
    }
    const float dx = p.x - proj.x;
    const float dy = p.y - proj.y;
    return std::sqrt(dx * dx + dy * dy);
}
