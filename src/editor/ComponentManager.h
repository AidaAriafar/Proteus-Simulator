#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

#include "../core/Component.h"
#include "../library/ComponentLibrary.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class ComponentManager
{
private:
    std::vector<std::unique_ptr<Component>> ownedComponents;
    std::vector<Component*> externalComponents;
    std::unordered_set<int> selectedIDs;
    int nextID;
    float gridSize;
    std::unordered_map<int, std::pair<float, float>> dragStartPositions;
    std::function<void(int)> componentDeletedCallback;
    std::function<void(int)> componentMovedCallback;

    Component* findMutable(int id);
    const Component* findConst(int id) const;
    std::vector<Component*> allMutable();
    std::vector<const Component*> allConst() const;
    void syncSelectionFlags();

public:
    ComponentManager();

    void add(Component* component);
    Component* addOwned(std::unique_ptr<Component> component);
    Component* placeComponent(const ComponentLibrary& library, const std::string& type, float x, float y);

    void setGridSize(float newGridSize);
    float snap(float value) const;
    std::pair<float, float> snapPoint(float x, float y) const;

    bool selectAt(float x, float y);
    void clearSelection();
    void selectInRectangle(const Rect& rectangle);
    std::vector<int> getSelectedIDs() const;

    Component* getComponent(int id);
    const Component* getComponent(int id) const;
    std::size_t componentCount() const;

    void beginDrag();
    void dragSelected(float deltaX, float deltaY);
    void endDrag();
    void rotateSelected();
    void mirrorSelectedHorizontal();
    void mirrorSelectedVertical();
    void deleteSelected();

    void setComponentDeletedCallback(std::function<void(int)> callback);
    void setComponentMovedCallback(std::function<void(int)> callback);

    void drawAll();
};

#endif
