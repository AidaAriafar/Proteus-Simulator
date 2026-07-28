#include "ComponentManager.h"


void ComponentManager::add(Component* component)
{
    components.push_back(component);
}



void ComponentManager::drawAll()
{

    for(auto component : components)
    {
        component->draw();
    }

}