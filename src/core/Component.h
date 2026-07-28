#ifndef COMPONENT_H
#define COMPONENT_H

#include "Pin.h"

#include <vector>
#include <string>


class Component
{

protected:

    int id;

    float x;
    float y;

    std::vector<Pin> pins;


public:

    Component(
        int id,
        float x,
        float y
    );


    virtual ~Component();


    virtual void draw() = 0;


    virtual std::string getType() = 0;


    int getID();


    float getX();


    float getY();


    // Pin management

    void addPin(Pin pin);


    std::vector<Pin>& getPins();

};


#endif