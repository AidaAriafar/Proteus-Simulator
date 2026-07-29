#ifndef SWITCH_H
#define SWITCH_H


#include "../core/Component.h"

#include <memory>

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


    void toggle();


    void turnOn();


    void turnOff();


    bool isOn();


};


#endif
