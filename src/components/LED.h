#ifndef LED_H
#define LED_H


#include "../core/Component.h"

#include <memory>

class LED : public Component
{

private:

    bool state;
    std::string color;


public:


    LED(
        int id,
        float x,
        float y
    );


    void draw() override;


    std::string getType() const override;


    std::unique_ptr<Component> clone(
        int newID
    ) const override;


    std::vector<PropertyDescriptor> getProperties() const override;


    bool setProperty(
        const std::string& key,
        const std::string& value,
        std::string& error
    ) override;


    void turnOn();


    void turnOff();


    bool isOn();


    std::string getColor() const;


    void setColor(
        const std::string& newColor
    );


};


#endif
