#ifndef PROPERTIES_PANEL_H
#define PROPERTIES_PANEL_H

#include "../core/Component.h"

#include <string>
#include <unordered_map>
#include <vector>

class PropertiesPanel
{
private:
    Component* currentComponent;
    std::unordered_map<std::string, std::string> pendingValues;

public:
    PropertiesPanel();

    void open(Component* component);
    bool isOpen() const;
    std::vector<PropertyDescriptor> getSchema() const;
    bool setPendingValue(const std::string& key, const std::string& value, std::string& error);
    bool apply(std::string& error);
    bool ok(std::string& error);
    void cancel();
};

#endif
