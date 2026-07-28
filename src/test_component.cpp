#include <iostream>

#include "components/Resistor.h"
#include "editor/ComponentManager.h"


int main()
{

    ComponentManager manager;


    Resistor resistor(
        1,
        100,
        200,
        1000
    );


    manager.add(&resistor);


    manager.drawAll();


    std::cout << "Component system works!" << std::endl;


    return 0;
}