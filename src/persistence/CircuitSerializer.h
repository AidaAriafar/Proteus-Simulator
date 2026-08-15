#ifndef CIRCUIT_SERIALIZER_H
#define CIRCUIT_SERIALIZER_H

// Subproject 10: serialization of the ENTIRE canvas - components with
// type, position, rotation, mirroring, label and editable properties,
// plus junctions and wires - into a JSON text file. The same routines
// power Save/Save As/Open and the snapshot based Undo/Redo system.

#include "../editor/ComponentManager.h"
#include "../library/ComponentLibrary.h"
#include "../wiring/Wiring.h"

#include <string>

struct CanvasSettings
{
    std::string pageSize = "A4";
    float widthUnits = 1122.0f;  // A4 landscape @ ~96 dpi
    float heightUnits = 793.0f;
};

class CircuitSerializer
{
public:
    static std::string toJSON(const ComponentManager& manager, const WireManager& wires, const CanvasSettings& canvas);

    static bool fromJSON(const std::string& json, ComponentManager& manager, WireManager& wires,
                         const ComponentLibrary& library, CanvasSettings& canvas, std::string& error);

    static bool saveToFile(const std::string& path, const std::string& json, std::string& error);
    static bool loadFromFile(const std::string& path, std::string& json, std::string& error);
};

#endif
