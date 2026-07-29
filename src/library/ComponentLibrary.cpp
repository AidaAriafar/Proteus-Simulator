#include "ComponentLibrary.h"



ComponentLibrary::ComponentLibrary()
{

}



void ComponentLibrary::registerComponent(
    std::string name
)
{
    components.push_back(name);
}



std::vector<std::string> ComponentLibrary::getComponents()
{
    return components;
}



bool ComponentLibrary::exists(
    std::string name
)
{

    for(auto& component : components)
    {

        if(component == name)
            return true;

    }


    return false;

}