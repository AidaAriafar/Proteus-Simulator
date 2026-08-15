#include "CircuitSerializer.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
std::string escape(const std::string& text)
{
    std::string out;
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
        }
        if (c == '\n')
        {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out;
}

// --- extremely small JSON reader (objects, arrays, strings, numbers) -----
struct JValue
{
    enum class Type
    {
        Null,
        Number,
        Boolean,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    double number = 0.0;
    bool boolean = false;
    std::string text;
    std::vector<JValue> items;
    std::vector<std::pair<std::string, JValue>> fields;

    const JValue* get(const std::string& key) const
    {
        for (const auto& field : fields)
        {
            if (field.first == key)
            {
                return &field.second;
            }
        }
        return nullptr;
    }

    double num(const std::string& key, double fallback = 0.0) const
    {
        const JValue* value = get(key);
        return value != nullptr && value->type == Type::Number ? value->number : fallback;
    }

    std::string str(const std::string& key, const std::string& fallback = "") const
    {
        const JValue* value = get(key);
        return value != nullptr && value->type == Type::String ? value->text : fallback;
    }
};

struct JParser
{
    const std::string& source;
    std::size_t pos = 0;
    bool failed = false;

    explicit JParser(const std::string& text) : source(text) {}

    void skip()
    {
        while (pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos])) != 0)
        {
            ++pos;
        }
    }

    bool consume(char expected)
    {
        skip();
        if (pos < source.size() && source[pos] == expected)
        {
            ++pos;
            return true;
        }
        return false;
    }

    JValue parseValue()
    {
        skip();
        if (failed || pos >= source.size())
        {
            failed = true;
            return {};
        }
        const char c = source[pos];
        if (c == '{')
        {
            return parseObject();
        }
        if (c == '[')
        {
            return parseArray();
        }
        if (c == '"')
        {
            return parseString();
        }
        if (source.compare(pos, 4, "true") == 0)
        {
            pos += 4;
            JValue value;
            value.type = JValue::Type::Boolean;
            value.boolean = true;
            return value;
        }
        if (source.compare(pos, 5, "false") == 0)
        {
            pos += 5;
            JValue value;
            value.type = JValue::Type::Boolean;
            return value;
        }
        if (source.compare(pos, 4, "null") == 0)
        {
            pos += 4;
            return {};
        }
        return parseNumber();
    }

    JValue parseString()
    {
        JValue value;
        value.type = JValue::Type::String;
        ++pos; // opening quote
        while (pos < source.size() && source[pos] != '"')
        {
            if (source[pos] == '\\' && pos + 1 < source.size())
            {
                ++pos;
                if (source[pos] == 'n')
                {
                    value.text += '\n';
                }
                else
                {
                    value.text += source[pos];
                }
            }
            else
            {
                value.text += source[pos];
            }
            ++pos;
        }
        if (pos >= source.size())
        {
            failed = true;
        }
        ++pos; // closing quote
        return value;
    }

    JValue parseNumber()
    {
        const std::size_t start = pos;
        while (pos < source.size() &&
               (std::isdigit(static_cast<unsigned char>(source[pos])) != 0 || source[pos] == '-' ||
                source[pos] == '+' || source[pos] == '.' || source[pos] == 'e' || source[pos] == 'E'))
        {
            ++pos;
        }
        JValue value;
        value.type = JValue::Type::Number;
        try
        {
            value.number = std::stod(source.substr(start, pos - start));
        }
        catch (...)
        {
            failed = true;
        }
        return value;
    }

    JValue parseArray()
    {
        JValue value;
        value.type = JValue::Type::Array;
        ++pos; // [
        skip();
        if (consume(']'))
        {
            return value;
        }
        while (!failed)
        {
            value.items.push_back(parseValue());
            skip();
            if (consume(']'))
            {
                break;
            }
            if (!consume(','))
            {
                failed = true;
            }
        }
        return value;
    }

    JValue parseObject()
    {
        JValue value;
        value.type = JValue::Type::Object;
        ++pos; // {
        skip();
        if (consume('}'))
        {
            return value;
        }
        while (!failed)
        {
            skip();
            if (pos >= source.size() || source[pos] != '"')
            {
                failed = true;
                break;
            }
            const JValue key = parseString();
            if (!consume(':'))
            {
                failed = true;
                break;
            }
            value.fields.emplace_back(key.text, parseValue());
            skip();
            if (consume('}'))
            {
                break;
            }
            if (!consume(','))
            {
                failed = true;
            }
        }
        return value;
    }
};

int rotationToDegrees(Rotation rotation)
{
    return static_cast<int>(rotation);
}

Rotation degreesToRotation(int degrees)
{
    switch (((degrees % 360) + 360) % 360)
    {
    case 90: return Rotation::DEG_90;
    case 180: return Rotation::DEG_180;
    case 270: return Rotation::DEG_270;
    default: return Rotation::DEG_0;
    }
}
}

std::string CircuitSerializer::toJSON(const ComponentManager& manager, const WireManager& wires, const CanvasSettings& canvas)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"format\": \"ProteusSimulatorCircuit\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"canvas\": {\"pageSize\": \"" << escape(canvas.pageSize) << "\", \"width\": " << canvas.widthUnits
        << ", \"height\": " << canvas.heightUnits << "},\n";

    out << "  \"components\": [\n";
    const auto components = manager.getAll();
    for (std::size_t index = 0; index < components.size(); ++index)
    {
        const Component* component = components[index];
        out << "    {\"id\": " << component->getID()
            << ", \"type\": \"" << escape(component->getType()) << "\""
            << ", \"x\": " << component->getX()
            << ", \"y\": " << component->getY()
            << ", \"rotation\": " << rotationToDegrees(component->getRotation())
            << ", \"mirrorH\": " << (component->isMirroredHorizontally() ? "true" : "false")
            << ", \"mirrorV\": " << (component->isMirroredVertically() ? "true" : "false")
            << ", \"label\": \"" << escape(component->getLabel()) << "\""
            << ", \"properties\": {";
        const auto properties = component->getProperties();
        bool first = true;
        for (const auto& property : properties)
        {
            if (!property.editable || property.key == "label")
            {
                continue;
            }
            if (!first)
            {
                out << ", ";
            }
            first = false;
            out << "\"" << escape(property.key) << "\": \"" << escape(property.value) << "\"";
        }
        out << "}}";
        out << (index + 1 < components.size() ? ",\n" : "\n");
    }
    out << "  ],\n";

    out << "  \"junctions\": [\n";
    const auto& junctions = wires.getJunctions();
    for (std::size_t index = 0; index < junctions.size(); ++index)
    {
        out << "    {\"id\": " << junctions[index].id << ", \"x\": " << junctions[index].x
            << ", \"y\": " << junctions[index].y << "}";
        out << (index + 1 < junctions.size() ? ",\n" : "\n");
    }
    out << "  ],\n";

    out << "  \"wires\": [\n";
    const auto& allWires = wires.getWires();
    auto writeAnchor = [&](const WireAnchor& anchor)
    {
        if (anchor.kind == AnchorKind::PinAnchor)
        {
            out << "{\"kind\": \"pin\", \"component\": " << anchor.componentID
                << ", \"pin\": " << anchor.pinIndex << "}";
        }
        else
        {
            out << "{\"kind\": \"junction\", \"junction\": " << anchor.junctionID << "}";
        }
    };
    for (std::size_t index = 0; index < allWires.size(); ++index)
    {
        out << "    {\"id\": " << allWires[index].id << ", \"a\": ";
        writeAnchor(allWires[index].a);
        out << ", \"b\": ";
        writeAnchor(allWires[index].b);
        out << "}";
        out << (index + 1 < allWires.size() ? ",\n" : "\n");
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

bool CircuitSerializer::fromJSON(const std::string& json, ComponentManager& manager, WireManager& wires,
                                 const ComponentLibrary& library, CanvasSettings& canvas, std::string& error)
{
    JParser parser(json);
    const JValue root = parser.parseValue();
    if (parser.failed || root.type != JValue::Type::Object)
    {
        error = "File is not valid circuit JSON";
        return false;
    }
    if (root.str("format") != "ProteusSimulatorCircuit")
    {
        error = "Unrecognised file format";
        return false;
    }

    manager.clear();
    wires.clearAll();

    if (const JValue* canvasValue = root.get("canvas"))
    {
        canvas.pageSize = canvasValue->str("pageSize", canvas.pageSize);
        canvas.widthUnits = static_cast<float>(canvasValue->num("width", canvas.widthUnits));
        canvas.heightUnits = static_cast<float>(canvasValue->num("height", canvas.heightUnits));
    }

    if (const JValue* components = root.get("components"))
    {
        for (const JValue& item : components->items)
        {
            const std::string type = item.str("type");
            const int id = static_cast<int>(item.num("id", -1));
            auto component = library.createComponent(
                type, id, static_cast<float>(item.num("x")), static_cast<float>(item.num("y")));
            if (component == nullptr)
            {
                error = "Unknown component type in file: " + type;
                return false;
            }
            component->setRotation(degreesToRotation(static_cast<int>(item.num("rotation"))));
            if (const JValue* mirrorH = item.get("mirrorH"); mirrorH != nullptr && mirrorH->boolean)
            {
                component->mirrorHorizontal();
            }
            if (const JValue* mirrorV = item.get("mirrorV"); mirrorV != nullptr && mirrorV->boolean)
            {
                component->mirrorVertical();
            }
            component->setLabel(item.str("label", component->getLabel()));
            if (const JValue* properties = item.get("properties"))
            {
                for (const auto& field : properties->fields)
                {
                    std::string ignored;
                    component->setProperty(field.first, field.second.text, ignored);
                }
            }
            manager.addOwned(std::move(component));
        }
    }

    if (const JValue* junctions = root.get("junctions"))
    {
        for (const JValue& item : junctions->items)
        {
            wires.restoreJunction(
                static_cast<int>(item.num("id")),
                static_cast<float>(item.num("x")),
                static_cast<float>(item.num("y")));
        }
    }

    if (const JValue* wireArray = root.get("wires"))
    {
        auto readAnchor = [](const JValue& value) -> WireAnchor
        {
            WireAnchor anchor;
            if (value.str("kind") == "junction")
            {
                anchor.kind = AnchorKind::JunctionAnchor;
                anchor.componentID = -1;
                anchor.pinIndex = -1;
                anchor.junctionID = static_cast<int>(value.num("junction"));
            }
            else
            {
                anchor.kind = AnchorKind::PinAnchor;
                anchor.componentID = static_cast<int>(value.num("component"));
                anchor.pinIndex = static_cast<int>(value.num("pin"));
                anchor.junctionID = -1;
            }
            return anchor;
        };
        for (const JValue& item : wireArray->items)
        {
            const JValue* a = item.get("a");
            const JValue* b = item.get("b");
            if (a == nullptr || b == nullptr)
            {
                continue;
            }
            wires.restoreWire(static_cast<int>(item.num("id")), readAnchor(*a), readAnchor(*b));
        }
    }
    return true;
}

bool CircuitSerializer::saveToFile(const std::string& path, const std::string& json, std::string& error)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open())
    {
        error = "Cannot open file for writing: " + path;
        return false;
    }
    file << json;
    return true;
}

bool CircuitSerializer::loadFromFile(const std::string& path, std::string& json, std::string& error)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        error = "Cannot open file: " + path;
        return false;
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    json = stream.str();
    return true;
}
