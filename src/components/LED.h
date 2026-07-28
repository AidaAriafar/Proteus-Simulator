#ifndef LED_H
#define LED_H


#include "../core/Component.h"


class LED : public Component
{

private:

    bool state;


public:


    LED(
        int id,
        float x,
        float y
    );


    void draw() override;


    std::string getType() override;


    void turnOn();


    void turnOff();


    bool isOn();


};


#endif