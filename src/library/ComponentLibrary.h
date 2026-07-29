#ifndef COMPONENT_LIBRARY_H
#define COMPONENT_LIBRARY_H

#include "../core/Component.h"

#include <memory>
#include <functional>
#include <string>
#include <vector>

struct LibraryItem
{
    std::string name;
    std::string displayName;
    ComponentCategory category;
};


class ComponentLibrary
{

private:

    std::vector<LibraryItem> components;
    std::vector<std::function<std::unique_ptr<Component>(int, float, float)>> factories;


public:

    ComponentLibrary();


    void registerComponent(
        const std::string& name
    );


    std::vector<std::string> getComponents() const;


    std::vector<LibraryItem> getItems() const;


    bool exists(
        const std::string& name
    ) const;


    std::unique_ptr<Component> createComponent(
        const std::string& name,
        int id,
        float x,
        float y
    ) const;

};


#endif
