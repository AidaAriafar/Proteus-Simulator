#include "ComponentManager.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

ComponentManager::ComponentManager()
:
nextID(1),
gridSize(10.0f)
{
}

void ComponentManager::add(Component* component)
{
    if(component != nullptr)
    {
        externalComponents.push_back(component);
        nextID = std::max(nextID, component->getID() + 1);
    }
}

Component* ComponentManager::addOwned(std::unique_ptr<Component> component)
{
    if(component == nullptr)
    {
        return nullptr;
    }
    nextID = std::max(nextID, component->getID() + 1);
    ownedComponents.push_back(std::move(component));
    return ownedComponents.back().get();
}

Component* ComponentManager::placeComponent(const ComponentLibrary& library, const std::string& type, float x, float y)
{
    const auto snapped = snapPoint(x, y);
    auto component = library.createComponent(type, nextID++, snapped.first, snapped.second);
    return addOwned(std::move(component));
}

void ComponentManager::setGridSize(float newGridSize)
{
    if(newGridSize <= 0)
    {
        throw std::invalid_argument("Grid size must be positive.");
    }
    gridSize = newGridSize;
}

float ComponentManager::snap(float value) const
{
    return std::round(value / gridSize) * gridSize;
}

std::pair<float, float> ComponentManager::snapPoint(float x, float y) const
{
    return {snap(x), snap(y)};
}

bool ComponentManager::selectAt(float x, float y)
{
    clearSelection();
    auto components = allMutable();
    for(auto iterator = components.rbegin(); iterator != components.rend(); ++iterator)
    {
        Component* component = *iterator;
        if(component->contains(x, y))
        {
            selectedIDs.insert(component->getID());
            syncSelectionFlags();
            return true;
        }
    }
    syncSelectionFlags();
    return false;
}

void ComponentManager::clearSelection()
{
    selectedIDs.clear();
    syncSelectionFlags();
}

void ComponentManager::selectInRectangle(const Rect& rectangle)
{
    selectedIDs.clear();
    for(Component* component : allMutable())
    {
        if(component->intersects(rectangle))
        {
            selectedIDs.insert(component->getID());
        }
    }
    syncSelectionFlags();
}

std::vector<int> ComponentManager::getSelectedIDs() const
{
    std::vector<int> ids(selectedIDs.begin(), selectedIDs.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

Component* ComponentManager::getComponent(int id)
{
    return findMutable(id);
}

const Component* ComponentManager::getComponent(int id) const
{
    return findConst(id);
}

std::size_t ComponentManager::componentCount() const
{
    return ownedComponents.size() + externalComponents.size();
}

void ComponentManager::beginDrag()
{
    dragStartPositions.clear();
    for(const int id : selectedIDs)
    {
        if(Component* component = findMutable(id))
        {
            dragStartPositions[id] = {component->getX(), component->getY()};
        }
    }
}

void ComponentManager::dragSelected(float deltaX, float deltaY)
{
    if(dragStartPositions.empty())
    {
        beginDrag();
    }
    for(const auto& entry : dragStartPositions)
    {
        if(Component* component = findMutable(entry.first))
        {
            component->setPosition(entry.second.first + deltaX, entry.second.second + deltaY);
        }
    }
}

void ComponentManager::endDrag()
{
    for(const int id : selectedIDs)
    {
        if(Component* component = findMutable(id))
        {
            const auto snapped = snapPoint(component->getX(), component->getY());
            component->setPosition(snapped.first, snapped.second);
            if(componentMovedCallback) componentMovedCallback(id);
        }
    }
    dragStartPositions.clear();
}

void ComponentManager::rotateSelected()
{
    for(const int id : selectedIDs)
    {
        if(Component* component = findMutable(id))
        {
            component->rotateClockwise();
        }
    }
}

void ComponentManager::mirrorSelectedHorizontal()
{
    for(const int id : selectedIDs)
    {
        if(Component* component = findMutable(id))
        {
            component->mirrorHorizontal();
        }
    }
}

void ComponentManager::mirrorSelectedVertical()
{
    for(const int id : selectedIDs)
    {
        if(Component* component = findMutable(id))
        {
            component->mirrorVertical();
        }
    }
}

void ComponentManager::deleteSelected()
{
    for(const int id : selectedIDs)
    {
        if(componentDeletedCallback) componentDeletedCallback(id);
    }

    ownedComponents.erase(
        std::remove_if(
            ownedComponents.begin(),
            ownedComponents.end(),
            [this](const std::unique_ptr<Component>& component)
            {
                return selectedIDs.count(component->getID()) != 0;
            }),
        ownedComponents.end());

    selectedIDs.clear();
    syncSelectionFlags();
}

void ComponentManager::setComponentDeletedCallback(std::function<void(int)> callback)
{
    componentDeletedCallback = std::move(callback);
}

void ComponentManager::setComponentMovedCallback(std::function<void(int)> callback)
{
    componentMovedCallback = std::move(callback);
}

void ComponentManager::drawAll()
{
    for(auto& component : ownedComponents)
    {
        component->draw();
    }
    for(auto* component : externalComponents)
    {
        if(component != nullptr)
        {
            component->draw();
        }
    }
}

Component* ComponentManager::findMutable(int id)
{
    for(auto& component : ownedComponents)
    {
        if(component->getID() == id) return component.get();
    }
    for(auto* component : externalComponents)
    {
        if(component != nullptr && component->getID() == id) return component;
    }
    return nullptr;
}

const Component* ComponentManager::findConst(int id) const
{
    for(const auto& component : ownedComponents)
    {
        if(component->getID() == id) return component.get();
    }
    for(const auto* component : externalComponents)
    {
        if(component != nullptr && component->getID() == id) return component;
    }
    return nullptr;
}

std::vector<Component*> ComponentManager::allMutable()
{
    std::vector<Component*> result;
    for(auto& component : ownedComponents)
    {
        result.push_back(component.get());
    }
    for(auto* component : externalComponents)
    {
        if(component != nullptr) result.push_back(component);
    }
    return result;
}

std::vector<const Component*> ComponentManager::allConst() const
{
    std::vector<const Component*> result;
    for(const auto& component : ownedComponents)
    {
        result.push_back(component.get());
    }
    for(const auto* component : externalComponents)
    {
        if(component != nullptr) result.push_back(component);
    }
    return result;
}

void ComponentManager::syncSelectionFlags()
{
    for(Component* component : allMutable())
    {
        component->setSelected(selectedIDs.count(component->getID()) != 0);
    }
}

std::vector<Component*> ComponentManager::getAll()
{
    return allMutable();
}

std::vector<const Component*> ComponentManager::getAll() const
{
    return allConst();
}

void ComponentManager::clear()
{
    ownedComponents.clear();
    externalComponents.clear();
    selectedIDs.clear();
    dragStartPositions.clear();
    nextID = 1;
}
