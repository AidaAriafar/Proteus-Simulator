#ifndef RESISTOR_H
#define RESISTOR_H


#include "../core/Component.h"

#include <memory>


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


    float getResistance()
    {
        return resistance;
    }


    void setResistance(
        float newResistance
    );

};


#endif
