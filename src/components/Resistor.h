#ifndef RESISTOR_H
#define RESISTOR_H


#include "../core/Component.h"


class Resistor : public Component
{

private:

    float resistance;


public:

    Resistor(
        int id,
        float x,
        float y,
        float resistance
    );


    void draw() override;


    std::string getType() override;


};


#endif