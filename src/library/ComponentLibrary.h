#ifndef COMPONENT_LIBRARY_H
#define COMPONENT_LIBRARY_H

#include "../core/Component.h"

#include <memory>
#include <string>
#include <vector>


class ComponentLibrary
{

private:

    std::vector<std::string> components;


public:

    ComponentLibrary();


    void registerComponent(
        const std::string& name
    );


    std::vector<std::string> getComponents() const;


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