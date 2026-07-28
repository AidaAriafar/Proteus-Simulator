#ifndef COMPONENT_H
#define COMPONENT_H

#include <string>


class Component
{

protected:

    int id;
    float x;
    float y;


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

};


#endif