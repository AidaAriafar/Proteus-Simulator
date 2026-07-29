#include "PropertiesPanel.h"

PropertiesPanel::PropertiesPanel()
:
currentComponent(nullptr)
{
}

void PropertiesPanel::open(Component* component)
{
    currentComponent = component;
    pendingValues.clear();
    if(currentComponent == nullptr)
    {
        return;
    }
    for(const auto& property : currentComponent->getProperties())
    {
        pendingValues[property.key] = property.value;
    }
}

bool PropertiesPanel::isOpen() const
{
    return currentComponent != nullptr;
}

std::vector<PropertyDescriptor> PropertiesPanel::getSchema() const
{
    if(currentComponent == nullptr)
    {
        return {};
    }
    auto schema = currentComponent->getProperties();
    for(auto& property : schema)
    {
        const auto pending = pendingValues.find(property.key);
        if(pending != pendingValues.end())
        {
            property.value = pending->second;
        }
    }
    return schema;
}

bool PropertiesPanel::setPendingValue(const std::string& key, const std::string& value, std::string& error)
{
    if(currentComponent == nullptr)
    {
        error = "No component is open.";
        return false;
    }

    for(const auto& property : currentComponent->getProperties())
    {
        if(property.key == key && property.editable)
        {
            pendingValues[key] = value;
            return true;
        }
    }

    error = "Unknown or read-only property: " + key;
    return false;
}

bool PropertiesPanel::apply(std::string& error)
{
    if(currentComponent == nullptr)
    {
        error = "No component is open.";
        return false;
    }

    for(const auto& entry : pendingValues)
    {
        if(!currentComponent->setProperty(entry.first, entry.second, error))
        {
            return false;
        }
    }
    open(currentComponent);
    return true;
}

bool PropertiesPanel::ok(std::string& error)
{
    if(!apply(error))
    {
        return false;
    }
    cancel();
    return true;
}

void PropertiesPanel::cancel()
{
    currentComponent = nullptr;
    pendingValues.clear();
}
