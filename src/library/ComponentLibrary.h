#ifndef COMPONENT_LIBRARY_H
#define COMPONENT_LIBRARY_H


#include "../core/Component.h"

#include <string>
#include <vector>


class ComponentLibrary
{

private:

    std::vector<std::string> components;


public:


    ComponentLibrary();


    void registerComponent(
        std::string name
    );


    std::vector<std::string> getComponents();


    bool exists(
        std::string name
    );


};


#endif