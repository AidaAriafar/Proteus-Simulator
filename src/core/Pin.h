#ifndef PIN_H
#define PIN_H

#include <string>


enum class PinType
{
    INPUT,
    OUTPUT,
    POWER
};


class Pin
{

private:

    int id;

    std::string name;

    float x;
    float y;

    PinType type;


    bool connected;


public:


    Pin(
        int id,
        std::string name,
        float x,
        float y,
        PinType type
    );


    int getID();


    std::string getName();


    float getX();


    float getY();


    PinType getType();


    bool isConnected();


    void connect();


    void disconnect();

};


#endif