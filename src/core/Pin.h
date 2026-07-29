#ifndef PIN_H
#define PIN_H

#include <string>


enum class PinType
{
    INPUT,
    OUTPUT,
    POWER,
    BIDIRECTIONAL
};

enum class PinDirection
{
    LEFT,
    RIGHT,
    UP,
    DOWN
};


class Pin
{

private:

    int id;

    std::string name;

    float x;
    float y;
    float localX;
    float localY;

    PinType type;
    PinDirection baseDirection;
    PinDirection direction;


    bool connected;
    bool required;


public:


    Pin(
        int id,
        std::string name,
        float x,
        float y,
        PinType type,
        bool required = true,
        PinDirection direction = PinDirection::RIGHT
    );


    int getID() const;


    std::string getName() const;


    float getX() const;


    float getY() const;


    float getLocalX() const;


    float getLocalY() const;


    PinType getType() const;


    PinDirection getDirection() const;


    PinDirection getBaseDirection() const;


    bool isRequired() const;


    bool isConnected() const;


    void setPosition(
        float newX,
        float newY
    );


    void setLocalPosition(
        float newLocalX,
        float newLocalY
    );


    void setDirection(
        PinDirection newDirection
    );


    void connect();


    void disconnect();

};


#endif
