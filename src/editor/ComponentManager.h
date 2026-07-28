#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H


#include "../core/Component.h"

#include <vector>


class ComponentManager
{

private:

    std::vector<Component*> components;


public:

    void add(Component* component);


    void drawAll();

};


#endif