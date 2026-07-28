#ifndef SWITCH_H
#define SWITCH_H


#include "../core/Component.h"


class Switch : public Component
{

private:

    bool state;


public:


    Switch(
        int id,
        float x,
        float y
    );


    void draw() override;


    std::string getType() override;


    void toggle();


    void turnOn();


    void turnOff();


    bool isOn();


};


#endif