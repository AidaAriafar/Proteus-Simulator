#include "ComponentLibrary.h"

#include "../components/LED.h"
#include "../components/Resistor.h"
#include "../components/Switch.h"

#include <algorithm>
#include <memory>


ComponentLibrary::ComponentLibrary()
{
}


void ComponentLibrary::registerComponent(
    const std::string& name
)
{
    if(!exists(name))
    {
        components.push_back(name);
    }
}


std::vector<std::string>
ComponentLibrary::getComponents() const
{
    return components;
}


bool ComponentLibrary::exists(
    const std::string& name
) const
{
    return std::find(
        components.begin(),
        components.end(),
        name
    ) != components.end();
}


std::unique_ptr<Component>
ComponentLibrary::createComponent(
    const std::string& name,
    int id,
    float x,
    float y
) const
{
    if(name == "Resistor")
    {
        return std::make_unique<Resistor>(
            id,
            x,
            y,
            1000
        );
    }


    if(name == "LED")
    {
        return std::make_unique<LED>(
            id,
            x,
            y
        );
    }


    if(name == "Switch")
    {
        return std::make_unique<Switch>(
            id,
            x,
            y
        );
    }


    return nullptr;
}