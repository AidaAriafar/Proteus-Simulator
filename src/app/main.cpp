
#define SDL_MAIN_HANDLED
#include <SDL.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/stb_image_write.h"

#include "Font.h"

#include "../components/Digital.h"
#include "../components/InteractiveComponents.h"
#include "../components/LED.h"
#include "../components/Sources.h"
#include "../components/Switch.h"
#include "../drc/DRCChecker.h"
#include "../editor/ComponentManager.h"
#include "../editor/PropertiesPanel.h"
#include "../library/ComponentLibrary.h"
#include "../persistence/CircuitSerializer.h"
#include "../sim/Simulator.h"
#include "../wiring/Wiring.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr int kToolbarHeight = 40;
constexpr int kLibraryWidth = 220;
constexpr int kStatusHeight = 24;
constexpr int kLogHeight = 120;

struct Camera
{
    float offsetX = 40.0f;
    float offsetY = 60.0f;
    float zoom = 1.0f;

    float worldX(int screenX) const { return (static_cast<float>(screenX) - offsetX) / zoom; }
    float worldY(int screenY) const { return (static_cast<float>(screenY) - offsetY) / zoom; }
    int screenX(float wx) const { return static_cast<int>(wx * zoom + offsetX); }
    int screenY(float wy) const { return static_cast<int>(wy * zoom + offsetY); }
};

enum class Screen
{
    Startup,
    Editor
};

enum class Tool
{
    Select,
    Wire
};

enum class PromptKind
{
    None,
    SaveAs,
    Open,
    NewCustom
};

struct UIButton
{
    SDL_Rect rect;
    std::string label;
    std::string id;
};

struct App
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int windowWidth = 1280;
    int windowHeight = 820;

    Screen screen = Screen::Startup;
    Tool tool = Tool::Select;

    ComponentLibrary library;
    ComponentManager manager;
    WireManager wires;
    Simulator sim;
    SimulationLog log;
    DRCChecker drc;
    PropertiesPanel propertiesPanel;
    CanvasSettings canvas;

    Camera camera;

    std::string searchText;
    bool searchFocused = false;
    std::string pendingPlacement;
    int libraryScroll = 0;
    bool groupOpen[4] = {true, true, true, true};
    std::vector<std::string> activeList;
    std::unique_ptr<Component> previewInstance;
    std::string previewName;

    bool draggingComponents = false;
    bool rectSelecting = false;
    float dragStartWX = 0, dragStartWY = 0;
    float rectWX = 0, rectWY = 0;

    bool panning = false;
    int panStartX = 0, panStartY = 0;
    float panOrigX = 0, panOrigY = 0;

    bool wireDraft = false;
    int wireFromComp = -1, wireFromPin = -1;
    int hoverPinComp = -1, hoverPinIdx = -1;

    int probedWireID = -1;
    std::deque<std::pair<float, float>> scopeSamples;

    bool propsOpen = false;
    int propsRowEditing = -1;
    std::string propsEditText;
    std::string propsError;

    PromptKind prompt = PromptKind::None;
    std::string promptText;

    std::vector<std::string> undoStack;
    std::vector<std::string> redoStack;

    std::string currentPath;
    std::vector<std::string> recents;
    std::string toast;
    Uint32 toastUntil = 0;

    bool showLog = true;
    int heldButtonComponent = -1;

    bool running = true;
    float mouseWX = 0, mouseWY = 0;
};

std::string recentsPath()
{
    const char* home = std::getenv("HOME");
    return std::string(home != nullptr ? home : ".") + "/.proteus_recents.txt";
}

void loadRecents(App& app)
{
    app.recents.clear();
    std::ifstream file(recentsPath());
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty())
        {
            app.recents.push_back(line);
        }
    }
}

void addRecent(App& app, const std::string& path)
{
    app.recents.erase(std::remove(app.recents.begin(), app.recents.end(), path), app.recents.end());
    app.recents.insert(app.recents.begin(), path);
    if (app.recents.size() > 6)
    {
        app.recents.resize(6);
    }
    std::ofstream file(recentsPath(), std::ios::trunc);
    for (const auto& recent : app.recents)
    {
        file << recent << "\n";
    }
}

void showToast(App& app, const std::string& message)
{
    app.toast = message;
    app.toastUntil = SDL_GetTicks() + 2600;
}

std::string snapshot(App& app)
{
    return CircuitSerializer::toJSON(app.manager, app.wires, app.canvas);
}

void applySnapshot(App& app, const std::string& json)
{
    std::string error;
    if (!CircuitSerializer::fromJSON(json, app.manager, app.wires, app.library, app.canvas, error))
    {
        app.log.error("Snapshot restore failed: " + error);
    }
}

void pushUndo(App& app)
{
    app.undoStack.push_back(snapshot(app));
    if (app.undoStack.size() > 200)
    {
        app.undoStack.erase(app.undoStack.begin());
    }
    app.redoStack.clear();
}

void doUndo(App& app)
{
    if (app.undoStack.empty())
    {
        showToast(app, "Nothing to undo");
        return;
    }
    app.redoStack.push_back(snapshot(app));
    applySnapshot(app, app.undoStack.back());
    app.undoStack.pop_back();
    showToast(app, "Undo");
}

void doRedo(App& app)
{
    if (app.redoStack.empty())
    {
        showToast(app, "Nothing to redo");
        return;
    }
    app.undoStack.push_back(snapshot(app));
    applySnapshot(app, app.redoStack.back());
    app.redoStack.pop_back();
    showToast(app, "Redo");
}

std::vector<const Component*> constAll(App& app)
{
    return static_cast<const ComponentManager&>(app.manager).getAll();
}

void setColor(SDL_Renderer* r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255)
{
    SDL_SetRenderDrawColor(r, red, green, blue, alpha);
}

void fillCircle(SDL_Renderer* r, int cx, int cy, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        const int span = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
        SDL_RenderDrawLine(r, cx - span, cy + dy, cx + span, cy + dy);
    }
}

void drawThickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, int thickness)
{
    for (int offset = -thickness / 2; offset <= thickness / 2; ++offset)
    {
        if (std::abs(x2 - x1) >= std::abs(y2 - y1))
        {
            SDL_RenderDrawLine(r, x1, y1 + offset, x2, y2 + offset);
        }
        else
        {
            SDL_RenderDrawLine(r, x1 + offset, y1, x2 + offset, y2);
        }
    }
}

void drawGridAndPage(App& app, SDL_Rect view)
{
    SDL_RenderSetClipRect(app.renderer, &view);
    const int px = app.camera.screenX(0), py = app.camera.screenY(0);
    const int pw = static_cast<int>(app.canvas.widthUnits * app.camera.zoom);
    const int ph = static_cast<int>(app.canvas.heightUnits * app.camera.zoom);
    SDL_Rect page = {px, py, pw, ph};
    setColor(app.renderer, 250, 250, 246);
    SDL_RenderFillRect(app.renderer, &page);

    setColor(app.renderer, 222, 226, 232);
    const float step = 20.0f;
    for (float gx = 0; gx <= app.canvas.widthUnits + 0.1f; gx += step)
    {
        SDL_RenderDrawLine(app.renderer, app.camera.screenX(gx), py, app.camera.screenX(gx), py + ph);
    }
    for (float gy = 0; gy <= app.canvas.heightUnits + 0.1f; gy += step)
    {
        SDL_RenderDrawLine(app.renderer, px, app.camera.screenY(gy), px + pw, app.camera.screenY(gy));
    }
    setColor(app.renderer, 120, 128, 140);
    SDL_RenderDrawRect(app.renderer, &page);
}

void drawSevenSegment(App& app, const SevenSegmentDisplay& display, SDL_Rect box)
{
    const int sx = box.x + box.w / 4, sy = box.y + 6;
    const int sw = box.w / 2, sh = box.h - 12;
    struct Seg { int x1, y1, x2, y2; };
    const Seg segments[7] = {
        {sx, sy, sx + sw, sy},
        {sx + sw, sy, sx + sw, sy + sh / 2},
        {sx + sw, sy + sh / 2, sx + sw, sy + sh},
        {sx, sy + sh, sx + sw, sy + sh},
        {sx, sy + sh / 2, sx, sy + sh},
        {sx, sy, sx, sy + sh / 2},
        {sx, sy + sh / 2, sx + sw, sy + sh / 2},
    };
    for (int index = 0; index < 7; ++index)
    {
        if (display.getSegmentState(static_cast<std::size_t>(index)))
        {
            setColor(app.renderer, 230, 40, 40);
        }
        else
        {
            setColor(app.renderer, 226, 222, 218);
        }
        drawThickLine(app.renderer, segments[index].x1, segments[index].y1, segments[index].x2, segments[index].y2, 3);
    }
}

struct SymbolPainter
{
    SDL_Renderer* renderer;
    const Camera& camera;
    const Component* component;

    void local(float u, float v, int& sx, int& sy) const
    {
        if (component->isMirroredHorizontally()) { u = -u; }
        if (component->isMirroredVertically()) { v = -v; }
        const float su = u, sv = v;
        switch (component->getRotation())
        {
        case Rotation::DEG_90:  u = -sv; v = su;  break;
        case Rotation::DEG_180: u = -su; v = -sv; break;
        case Rotation::DEG_270: u = sv;  v = -su; break;
        default: break;
        }
        sx = camera.screenX(component->getX() + u);
        sy = camera.screenY(component->getY() + v);
    }

    void line(float u1, float v1, float u2, float v2, int thickness = 2) const
    {
        int x1, y1, x2, y2;
        local(u1, v1, x1, y1);
        local(u2, v2, x2, y2);
        drawThickLine(renderer, x1, y1, x2, y2, thickness);
    }

    void poly(const std::vector<std::pair<float, float>>& points, int thickness = 2) const
    {
        for (std::size_t i = 0; i + 1 < points.size(); ++i)
        {
            line(points[i].first, points[i].second, points[i + 1].first, points[i + 1].second, thickness);
        }
    }

    void circle(float cu, float cv, float radius) const
    {
        int cx, cy;
        local(cu, cv, cx, cy);
        const int r = std::max(2, static_cast<int>(radius * camera.zoom));
        int px = cx + r, py = cy;
        for (int step = 1; step <= 24; ++step)
        {
            const float angle = 2.0f * 3.14159265f * step / 24.0f;
            const int nx = cx + static_cast<int>(r * std::cos(angle));
            const int ny = cy + static_cast<int>(r * std::sin(angle));
            SDL_RenderDrawLine(renderer, px, py, nx, ny);
            px = nx; py = ny;
        }
    }

    void dot(float cu, float cv, float radius) const
    {
        int cx, cy;
        local(cu, cv, cx, cy);
        fillCircle(renderer, cx, cy, std::max(2, static_cast<int>(radius * camera.zoom)));
    }

    void text(float cu, float cv, const std::string& value) const
    {
        int cx, cy;
        local(cu, cv, cx, cy);
        appfont::drawText(renderer, value, cx - appfont::textWidth(value) / 2, cy - 4);
    }
};

void drawSymbolShape(App& app, const Component* component)
{
    SymbolPainter paint{app.renderer, app.camera, component};
    const std::string type = component->getType();
    setColor(app.renderer, 40, 46, 56);

    if (type == "Resistor")
    {
        paint.line(-30, 0, -18, 0);
        paint.poly({{-18, 0}, {-14, -8}, {-7, 8}, {0, -8}, {7, 8}, {14, -8}, {18, 0}});
        paint.line(18, 0, 30, 0);
    }
    else if (type == "Capacitor")
    {
        paint.line(-20, 0, -4, 0);
        paint.line(-4, -12, -4, 12, 3);
        paint.line(4, -12, 4, 12, 3);
        paint.line(4, 0, 20, 0);
    }
    else if (type == "Inductor")
    {
        paint.line(-20, 0, -15, 0);
        for (int bump = 0; bump < 3; ++bump)
        {
            const float u0 = -15.0f + bump * 10.0f;
            paint.poly({{u0, 0}, {u0 + 2, -8}, {u0 + 8, -8}, {u0 + 10, 0}});
        }
        paint.line(15, 0, 20, 0);
    }
    else if (type == "DCVoltageSource")
    {
        paint.line(0, -25, 0, -14);
        paint.circle(0, 0, 14);
        paint.line(-4, -6, 4, -6);
        paint.line(0, -10, 0, -2);
        paint.line(-4, 6, 4, 6);
        paint.line(0, 14, 0, 25);
    }
    else if (type == "Battery")
    {
        paint.line(0, -25, 0, -4);
        paint.line(-12, -4, 12, -4, 3);
        paint.line(-6, 4, 6, 4, 3);
        paint.line(0, 4, 0, 25);
        paint.text(18, -8, "+");
    }
    else if (type == "PulseSource")
    {
        paint.line(-35, 0, -24, 0);
        paint.poly({{-24, -16}, {24, -16}, {24, 16}, {-24, 16}, {-24, -16}});
        paint.poly({{-16, 8}, {-8, 8}, {-8, -8}, {0, -8}, {0, 8}, {8, 8}, {8, -8}, {16, -8}});
        paint.line(24, 0, 35, 0);
    }
    else if (type == "GND")
    {
        paint.line(0, -15, 0, 0);
        paint.line(-12, 0, 12, 0, 2);
        paint.line(-8, 5, 8, 5, 2);
        paint.line(-4, 10, 4, 10, 2);
    }
    else if (type == "LED")
    {
        const auto* led = dynamic_cast<const LED*>(component);
        const bool lit = led != nullptr && const_cast<LED*>(led)->isOn();
        paint.line(-20, 0, -8, 0);
        if (lit)
        {
            setColor(app.renderer, 235, 60, 60);
            paint.dot(0, 0, 10);
            setColor(app.renderer, 40, 46, 56);
        }
        paint.poly({{-8, -9}, {-8, 9}, {8, 0}, {-8, -9}});
        paint.line(8, -9, 8, 9, 3);
        paint.line(8, 0, 20, 0);
        paint.line(2, -10, 8, -18);
        paint.line(8, -18, 5, -17);
        paint.line(8, -18, 7, -15);
        paint.line(8, -12, 14, -20);
        paint.line(14, -20, 11, -19);
        paint.line(14, -20, 13, -17);
    }
    else if (type == "Switch")
    {
        const auto* toggle = dynamic_cast<const Switch*>(component);
        const bool closed = toggle != nullptr && const_cast<Switch*>(toggle)->isOn();
        paint.line(-20, 0, -10, 0);
        paint.line(10, 0, 20, 0);
        paint.dot(-10, 0, 2.5f);
        paint.dot(10, 0, 2.5f);
        if (closed)
        {
            paint.line(-10, 0, 10, 0, 3);
        }
        else
        {
            paint.line(-10, 0, 8, -12, 3);
        }
    }
    else if (type == "PushButton")
    {
        const auto* button = dynamic_cast<const PushButton*>(component);
        const bool pressed = button != nullptr && const_cast<PushButton*>(button)->isPressed();
        paint.line(-20, 0, -8, 0);
        paint.line(8, 0, 20, 0);
        paint.dot(-8, 0, 2.5f);
        paint.dot(8, 0, 2.5f);
        const float gap = pressed ? 0.0f : -7.0f;
        paint.line(-10, gap - 4, 10, gap - 4, 3);
        paint.line(0, gap - 4, 0, gap - 12);
        paint.line(-5, gap - 12, 5, gap - 12);
    }
    else if (const auto* gate = dynamic_cast<const LogicGate*>(component))
    {
        const GateKind kind = gate->getKind();
        const bool inverted = kind == GateKind::NOT || kind == GateKind::NAND;
        if (kind == GateKind::NOT)
        {
            paint.line(-30, 0, -16, 0);
        }
        else
        {
            paint.line(-30, -10, -14, -10);
            paint.line(-30, 10, -14, 10);
        }
        if (kind == GateKind::NOT)
        {
            paint.poly({{-16, -13}, {-16, 13}, {12, 0}, {-16, -13}});
        }
        else if (kind == GateKind::AND || kind == GateKind::NAND)
        {
            paint.line(-14, -15, -14, 15);
            paint.line(-14, -15, 2, -15);
            paint.line(-14, 15, 2, 15);
            paint.poly({{2, -15}, {9, -12}, {14, -6}, {16, 0}, {14, 6}, {9, 12}, {2, 15}});
        }
        else
        {
            paint.poly({{-14, -15}, {-10, -8}, {-9, 0}, {-10, 8}, {-14, 15}});
            paint.poly({{-14, -15}, {0, -14}, {9, -9}, {16, 0}});
            paint.poly({{-14, 15}, {0, 14}, {9, 9}, {16, 0}});
            if (kind == GateKind::XOR)
            {
                paint.poly({{-19, -15}, {-15, -8}, {-14, 0}, {-15, 8}, {-19, 15}});
            }
        }
        if (inverted)
        {
            paint.circle(19, 0, 3);
            paint.line(22, 0, 30, 0);
        }
        else
        {
            paint.line(16, 0, 30, 0);
        }
    }
    else if (type == "DFlipFlop")
    {
        paint.line(-35, -15, -22, -15);
        paint.line(-35, 15, -22, 15);
        paint.line(22, -15, 35, -15);
        paint.line(22, 15, 35, 15);
        paint.poly({{-22, -26}, {22, -26}, {22, 26}, {-22, 26}, {-22, -26}});
        paint.poly({{-22, 10}, {-15, 15}, {-22, 20}});
        paint.text(-14, -15, "D");
        paint.text(14, -15, "Q");
        paint.text(13, 15, "-Q");
    }
    else if (type == "ADC" || type == "DAC")
    {
        paint.poly({{-30, -28}, {30, -28}, {30, 28}, {-30, 28}, {-30, -28}});
        paint.line(-30, 28, 30, -28);
        paint.text(0, 0, type);
        if (type == "ADC")
        {
            paint.line(-40, 0, -30, 0);
            paint.line(30, -20, 40, -20);
            paint.line(30, 0, 40, 0);
            paint.line(30, 20, 40, 20);
        }
        else
        {
            paint.line(-40, -20, -30, -20);
            paint.line(-40, 0, -30, 0);
            paint.line(-40, 20, -30, 20);
            paint.line(30, 0, 40, 0);
        }
    }
    else if (type == "Voltmeter" || type == "Ammeter")
    {
        paint.line(0, -25, 0, -14);
        paint.circle(0, 0, 14);
        paint.text(0, 0, type == "Voltmeter" ? "V" : "A");
        paint.line(0, 14, 0, 25);
    }
    else
    {
        const Rect bounds = component->getBoundingBox();
        SDL_Rect box = {
            app.camera.screenX(bounds.x), app.camera.screenY(bounds.y),
            static_cast<int>(bounds.width * app.camera.zoom),
            static_cast<int>(bounds.height * app.camera.zoom)};
        SDL_RenderDrawRect(app.renderer, &box);
    }
}

void drawComponent(App& app, const Component* component)
{
    const Rect bounds = component->getBoundingBox();
    SDL_Rect box = {
        app.camera.screenX(bounds.x),
        app.camera.screenY(bounds.y),
        static_cast<int>(bounds.width * app.camera.zoom),
        static_cast<int>(bounds.height * app.camera.zoom)};

    if (component->isSelected())
    {
        setColor(app.renderer, 205, 226, 255, 140);
        SDL_RenderFillRect(app.renderer, &box);
        setColor(app.renderer, 50, 110, 220);
        SDL_RenderDrawRect(app.renderer, &box);
    }

    if (const auto* display = dynamic_cast<const SevenSegmentDisplay*>(component))
    {
        setColor(app.renderer, 30, 32, 38);
        SDL_RenderFillRect(app.renderer, &box);
        setColor(app.renderer, 90, 96, 108);
        SDL_RenderDrawRect(app.renderer, &box);
        drawSevenSegment(app, *display, box);
    }
    else
    {
        drawSymbolShape(app, component);
    }

    std::string extra;
    if (const auto* voltmeter = dynamic_cast<const Voltmeter*>(component))
    {
        std::ostringstream stream;
        stream.precision(2);
        stream << std::fixed << voltmeter->getReading() << "V";
        extra = stream.str();
    }
    else if (const auto* ammeter = dynamic_cast<const Ammeter*>(component))
    {
        std::ostringstream stream;
        stream.precision(3);
        stream << std::fixed << ammeter->getReading() << "A";
        extra = stream.str();
    }

    setColor(app.renderer, 40, 44, 52);
    const std::string label = component->getLabel();
    appfont::drawText(app.renderer, label, box.x + (box.w - appfont::textWidth(label)) / 2, box.y - 12);
    if (!extra.empty())
    {
        setColor(app.renderer, 20, 110, 40);
        appfont::drawText(app.renderer, extra, box.x + (box.w - appfont::textWidth(extra)) / 2, box.y + box.h + 3);
    }

    for (std::size_t index = 0; index < component->getPins().size(); ++index)
    {
        const Pin& pin = component->getPins()[index];
        const int sx = app.camera.screenX(pin.getX());
        const int sy = app.camera.screenY(pin.getY());
        const bool hovered = app.hoverPinComp == component->getID() && app.hoverPinIdx == static_cast<int>(index);
        if (hovered)
        {
            setColor(app.renderer, 255, 160, 20);
            fillCircle(app.renderer, sx, sy, 6);
        }
        setColor(app.renderer, pin.isConnected() ? 20 : 160, pin.isConnected() ? 150 : 60, 40);
        SDL_Rect pinRect = {sx - 3, sy - 3, 6, 6};
        SDL_RenderFillRect(app.renderer, &pinRect);
    }
}

void drawWires(App& app)
{
    const auto componentsConst = constAll(app);
    for (const auto& wire : app.wires.getWires())
    {
        Uint8 red = 70, green = 74, blue = 82;
        if (app.sim.getState() != SimState::Stopped)
        {
            NetValue value;
            if (app.sim.valueForWire(wire.id, app.wires, componentsConst, value))
            {
                if (value.level == LogicLevel::High)
                {
                    red = 225; green = 45; blue = 45;
                }
                else if (value.level == LogicLevel::Low)
                {
                    red = 45; green = 90; blue = 225;
                }
                else
                {
                    red = 210; green = 180, blue = 40;
                }
            }
        }
        if (wire.selected)
        {
            red = 255; green = 255; blue = 255;
        }
        setColor(app.renderer, red, green, blue);
        const auto polyline = app.wires.routeWire(wire);
        for (std::size_t i = 0; i + 1 < polyline.size(); ++i)
        {
            drawThickLine(
                app.renderer,
                app.camera.screenX(polyline[i].x), app.camera.screenY(polyline[i].y),
                app.camera.screenX(polyline[i + 1].x), app.camera.screenY(polyline[i + 1].y),
                wire.selected ? 3 : 2);
        }
        if (app.probedWireID == wire.id)
        {
            setColor(app.renderer, 20, 200, 120);
            if (!polyline.empty())
            {
                fillCircle(app.renderer, app.camera.screenX(polyline.front().x), app.camera.screenY(polyline.front().y), 4);
            }
        }
    }
    setColor(app.renderer, 40, 44, 52);
    for (const auto& dot : app.wires.junctionDots())
    {
        fillCircle(app.renderer, app.camera.screenX(dot.x), app.camera.screenY(dot.y), 4);
    }
    if (app.wireDraft)
    {
        const Component* from = app.manager.getComponent(app.wireFromComp);
        if (from != nullptr && app.wireFromPin >= 0 &&
            static_cast<std::size_t>(app.wireFromPin) < from->getPins().size())
        {
            const Pin& pin = from->getPins()[static_cast<std::size_t>(app.wireFromPin)];
            const auto preview = app.wires.routePreview({pin.getX(), pin.getY()}, {app.mouseWX, app.mouseWY});
            setColor(app.renderer, 20, 160, 90);
            for (std::size_t i = 0; i + 1 < preview.size(); ++i)
            {
                SDL_RenderDrawLine(
                    app.renderer,
                    app.camera.screenX(preview[i].x), app.camera.screenY(preview[i].y),
                    app.camera.screenX(preview[i + 1].x), app.camera.screenY(preview[i + 1].y));
            }
        }
    }
}

std::vector<UIButton> toolbarButtons(App& app)
{
    std::vector<UIButton> buttons;
    int x = 8;
    auto add = [&](const std::string& id, const std::string& label)
    {
        const int width = appfont::textWidth(label) + 14;
        buttons.push_back({{x, 7, width, 26}, label, id});
        x += width + 6;
    };
    add("select", app.tool == Tool::Select ? "[SELECT]" : "SELECT");
    add("wire", app.tool == Tool::Wire ? "[WIRE]" : "WIRE");
    add("run", "RUN");
    add("pause", "PAUSE");
    add("stop", "STOP");
    add("step", "STEP");
    add("drc", "DRC");
    add("undo", "UNDO");
    add("redo", "REDO");
    add("save", "SAVE");
    add("saveas", "SAVE AS");
    add("open", "OPEN");
    add("export", "EXPORT PNG");
    add("log", app.showLog ? "[LOG]" : "LOG");
    add("menu", "MENU");
    return buttons;
}

enum class LibraryRowKind
{
    Header,
    Item,
    ActiveItem,
    Message
};

struct LibraryRow
{
    SDL_Rect rect;
    LibraryRowKind kind;
    std::string itemName;
    std::string displayText;
};

std::string toLowerCopy(const std::string& value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered;
}

std::vector<LibraryRow> libraryRows(App& app)
{
    std::vector<LibraryRow> rows;
    int y = kToolbarHeight + 34 - app.libraryScroll;
    const char* headers[4] = {"SOURCES", "PASSIVE / DIGITAL", "INTERACTIVE", "OUTPUTS"};
    const ComponentCategory order[4] = {
        ComponentCategory::Source, ComponentCategory::Passive,
        ComponentCategory::Interactive, ComponentCategory::Output};
    const std::string lowered = toLowerCopy(app.searchText);
    const bool searching = !lowered.empty();

    if (!app.activeList.empty())
    {
        rows.push_back({{6, y, kLibraryWidth - 12, 18}, LibraryRowKind::Message, "", "ACTIVE COMPONENTS"});
        y += 20;
        for (const auto& name : app.activeList)
        {
            std::string display = name;
            for (const auto& item : app.library.getItems())
            {
                if (item.name == name) { display = item.displayName; break; }
            }
            rows.push_back({{10, y, kLibraryWidth - 20, 18}, LibraryRowKind::ActiveItem, name, display});
            y += 19;
        }
        y += 6;
    }

    std::size_t matches = 0;
    for (int group = 0; group < 4; ++group)
    {
        const bool categoryMatches = searching &&
            toLowerCopy(headers[group]).find(lowered) != std::string::npos;
        rows.push_back({{6, y, kLibraryWidth - 12, 18}, LibraryRowKind::Header,
                        std::to_string(group), headers[group]});
        y += 20;
        if (!app.groupOpen[group] && !searching)
        {
            continue;
        }
        for (const auto& item : app.library.getItems())
        {
            if (item.category != order[group])
            {
                continue;
            }
            if (searching && !categoryMatches &&
                toLowerCopy(item.displayName).find(lowered) == std::string::npos)
            {
                continue;
            }
            rows.push_back({{10, y, kLibraryWidth - 20, 18}, LibraryRowKind::Item, item.name, item.displayName});
            y += 19;
            ++matches;
        }
        y += 6;
    }
    if (searching && matches == 0)
    {
        rows.push_back({{6, y, kLibraryWidth - 12, 18}, LibraryRowKind::Message, "",
                        "NO MATCHING COMPONENTS"});
    }
    return rows;
}

void drawLibraryPanel(App& app)
{
    SDL_Rect panel = {0, kToolbarHeight, kLibraryWidth, app.windowHeight - kToolbarHeight - kStatusHeight};
    setColor(app.renderer, 32, 36, 44);
    SDL_RenderFillRect(app.renderer, &panel);

    SDL_Rect search = {6, kToolbarHeight + 6, kLibraryWidth - 12, 22};
    setColor(app.renderer, app.searchFocused ? 255 : 210, app.searchFocused ? 255 : 214, app.searchFocused ? 255 : 220);
    SDL_RenderFillRect(app.renderer, &search);
    setColor(app.renderer, 40, 44, 52);
    appfont::drawText(app.renderer, app.searchText.empty() && !app.searchFocused ? "SEARCH..." : app.searchText,
                      search.x + 5, search.y + 7);

    const int previewHeight = 118;
    SDL_Rect listClip = panel;
    listClip.h -= previewHeight;
    SDL_RenderSetClipRect(app.renderer, &listClip);
    for (const auto& row : libraryRows(app))
    {
        if (row.kind == LibraryRowKind::Header)
        {
            const int group = std::stoi(row.itemName);
            setColor(app.renderer, 140, 190, 255);
            const bool open = app.groupOpen[group] || !app.searchText.empty();
            appfont::drawText(app.renderer, std::string(open ? "[-] " : "[+] ") + row.displayText,
                              row.rect.x, row.rect.y + 4);
        }
        else if (row.kind == LibraryRowKind::Message)
        {
            setColor(app.renderer, 235, 195, 80);
            appfont::drawText(app.renderer, row.displayText, row.rect.x, row.rect.y + 4);
        }
        else if (row.kind == LibraryRowKind::ActiveItem)
        {
            if (row.itemName == app.pendingPlacement)
            {
                setColor(app.renderer, 70, 110, 180);
                SDL_RenderFillRect(app.renderer, const_cast<SDL_Rect*>(&row.rect));
            }
            setColor(app.renderer, 170, 235, 190);
            appfont::drawText(app.renderer, row.displayText, row.rect.x + 4, row.rect.y + 5);
            setColor(app.renderer, 240, 110, 110);
            appfont::drawText(app.renderer, "X", row.rect.x + row.rect.w - 12, row.rect.y + 5);
        }
        else
        {
            if (row.itemName == app.pendingPlacement)
            {
                setColor(app.renderer, 70, 110, 180);
                SDL_RenderFillRect(app.renderer, const_cast<SDL_Rect*>(&row.rect));
            }
            setColor(app.renderer, 225, 228, 232);
            appfont::drawText(app.renderer, row.displayText, row.rect.x + 4, row.rect.y + 5);
            const bool inActive = std::find(app.activeList.begin(), app.activeList.end(), row.itemName) != app.activeList.end();
            setColor(app.renderer, inActive ? 90 : 120, inActive ? 96 : 220, inActive ? 108 : 160);
            appfont::drawText(app.renderer, "+", row.rect.x + row.rect.w - 12, row.rect.y + 5);
        }
    }
    SDL_RenderSetClipRect(app.renderer, nullptr);

    SDL_Rect preview = {6, app.windowHeight - kStatusHeight - previewHeight + 4,
                        kLibraryWidth - 12, previewHeight - 10};
    setColor(app.renderer, 250, 250, 246);
    SDL_RenderFillRect(app.renderer, &preview);
    setColor(app.renderer, 120, 128, 140);
    SDL_RenderDrawRect(app.renderer, &preview);
    setColor(app.renderer, 90, 96, 108);
    appfont::drawText(app.renderer, "PREVIEW", preview.x + 4, preview.y + 4);
    if (!app.pendingPlacement.empty())
    {
        if (app.previewName != app.pendingPlacement)
        {
            app.previewInstance = app.library.createComponent(app.pendingPlacement, -1, 0.0f, 0.0f);
            app.previewName = app.pendingPlacement;
        }
        if (app.previewInstance)
        {
            const Camera saved = app.camera;
            app.camera.zoom = 0.9f;
            app.camera.offsetX = static_cast<float>(preview.x + preview.w / 2);
            app.camera.offsetY = static_cast<float>(preview.y + preview.h / 2 + 6);
            SDL_RenderSetClipRect(app.renderer, &preview);
            if (const auto* display = dynamic_cast<const SevenSegmentDisplay*>(app.previewInstance.get()))
            {
                SDL_Rect box = {preview.x + preview.w / 2 - 26, preview.y + 18, 52, 78};
                setColor(app.renderer, 30, 32, 38);
                SDL_RenderFillRect(app.renderer, &box);
                drawSevenSegment(app, *display, box);
            }
            else
            {
                drawSymbolShape(app, app.previewInstance.get());
            }
            SDL_RenderSetClipRect(app.renderer, nullptr);
            app.camera = saved;
        }
        setColor(app.renderer, 120, 220, 160);
        appfont::drawText(app.renderer, "CLICK CANVAS TO PLACE", preview.x + 4, preview.y + preview.h - 24);
        appfont::drawText(app.renderer, "ESC TO CANCEL", preview.x + 4, preview.y + preview.h - 12);
    }
    else
    {
        app.previewInstance.reset();
        app.previewName.clear();
        setColor(app.renderer, 150, 156, 168);
        appfont::drawText(app.renderer, "SELECT A COMPONENT", preview.x + 4, preview.y + 50);
    }
}

void drawToolbar(App& app)
{
    SDL_Rect bar = {0, 0, app.windowWidth, kToolbarHeight};
    setColor(app.renderer, 24, 27, 34);
    SDL_RenderFillRect(app.renderer, &bar);
    for (const auto& button : toolbarButtons(app))
    {
        setColor(app.renderer, 52, 58, 70);
        SDL_Rect rect = button.rect;
        SDL_RenderFillRect(app.renderer, &rect);
        setColor(app.renderer, 210, 214, 222);
        appfont::drawText(app.renderer, button.label, button.rect.x + 7, button.rect.y + 9);
    }
}

void drawStatusBar(App& app)
{
    SDL_Rect bar = {0, app.windowHeight - kStatusHeight, app.windowWidth, kStatusHeight};
    setColor(app.renderer, 24, 27, 34);
    SDL_RenderFillRect(app.renderer, &bar);
    std::ostringstream stream;
    stream.precision(1);
    stream << std::fixed
           << "X: " << app.mouseWX << "  Y: " << app.mouseWY
           << "  |  ZOOM: " << static_cast<int>(app.camera.zoom * 100) << "%"
           << "  |  PAGE: " << app.canvas.pageSize
           << "  |  T = " << app.sim.getTime() << "S  ";
    switch (app.sim.getState())
    {
    case SimState::Running: stream << "[RUNNING]"; break;
    case SimState::Paused: stream << "[PAUSED]"; break;
    case SimState::Stopped: stream << "[STOPPED]"; break;
    }
    if (!app.currentPath.empty())
    {
        stream << "  |  " << app.currentPath;
    }
    setColor(app.renderer, 200, 206, 214);
    appfont::drawText(app.renderer, stream.str(), 8, app.windowHeight - kStatusHeight + 8);
}

void drawLogPanel(App& app)
{
    if (!app.showLog)
    {
        return;
    }
    SDL_Rect panel = {kLibraryWidth, app.windowHeight - kStatusHeight - kLogHeight, app.windowWidth - kLibraryWidth, kLogHeight};
    setColor(app.renderer, 16, 18, 24);
    SDL_RenderFillRect(app.renderer, &panel);
    setColor(app.renderer, 90, 96, 108);
    SDL_RenderDrawRect(app.renderer, &panel);
    setColor(app.renderer, 140, 190, 255);
    appfont::drawText(app.renderer, "SIMULATION LOG / DRC REPORT", panel.x + 8, panel.y + 5);
    const auto& entries = app.log.getEntries();
    const int visible = (kLogHeight - 24) / 13;
    int y = panel.y + 20;
    const std::size_t start = entries.size() > static_cast<std::size_t>(visible) ? entries.size() - static_cast<std::size_t>(visible) : 0;
    for (std::size_t index = start; index < entries.size(); ++index)
    {
        const auto& entry = entries[index];
        if (entry.severity == LogSeverity::Error)
        {
            setColor(app.renderer, 240, 90, 90);
        }
        else if (entry.severity == LogSeverity::Warning)
        {
            setColor(app.renderer, 235, 195, 80);
        }
        else
        {
            setColor(app.renderer, 170, 210, 170);
        }
        appfont::drawText(app.renderer, entry.message.substr(0, 110), panel.x + 8, y);
        y += 13;
    }
}

void drawScope(App& app)
{
    if (app.probedWireID < 0 || app.scopeSamples.empty())
    {
        return;
    }
    SDL_Rect panel = {app.windowWidth - 300, kToolbarHeight + 8, 292, 130};
    setColor(app.renderer, 16, 18, 24, 235);
    SDL_RenderFillRect(app.renderer, &panel);
    setColor(app.renderer, 90, 96, 108);
    SDL_RenderDrawRect(app.renderer, &panel);
    setColor(app.renderer, 140, 190, 255);
    appfont::drawText(app.renderer, "OSCILLOSCOPE (PROBED NET)", panel.x + 8, panel.y + 5);
    setColor(app.renderer, 60, 64, 72);
    SDL_RenderDrawLine(app.renderer, panel.x + 8, panel.y + 70, panel.x + panel.w - 8, panel.y + 70);
    setColor(app.renderer, 60, 230, 140);
    int prevX = -1, prevY = -1;
    const int usable = panel.w - 16;
    const std::size_t count = app.scopeSamples.size();
    for (std::size_t index = 0; index < count; ++index)
    {
        const int x = panel.x + 8 + static_cast<int>(static_cast<float>(index) / static_cast<float>(std::max<std::size_t>(count, 2) - 1) * usable);
        const float voltage = app.scopeSamples[index].second;
        const int y = panel.y + 110 - static_cast<int>(std::min(std::max(voltage, -1.0f), 6.0f) / 6.0f * 80.0f);
        if (prevX >= 0)
        {
            SDL_RenderDrawLine(app.renderer, prevX, prevY, x, y);
        }
        prevX = x;
        prevY = y;
    }
    std::ostringstream stream;
    stream.precision(2);
    stream << std::fixed << app.scopeSamples.back().second << " V";
    setColor(app.renderer, 220, 224, 230);
    appfont::drawText(app.renderer, stream.str(), panel.x + panel.w - 70, panel.y + 5);
}

void drawPropertiesModal(App& app)
{
    if (!app.propsOpen)
    {
        return;
    }
    SDL_Rect modal = {app.windowWidth / 2 - 220, 120, 440, 300};
    setColor(app.renderer, 28, 31, 40);
    SDL_RenderFillRect(app.renderer, &modal);
    setColor(app.renderer, 120, 128, 140);
    SDL_RenderDrawRect(app.renderer, &modal);
    setColor(app.renderer, 140, 190, 255);
    appfont::drawText(app.renderer, "PROPERTIES  (CLICK ROW, TYPE, ENTER = APPLY, ESC = CLOSE)", modal.x + 10, modal.y + 8);
    const auto schema = app.propertiesPanel.getSchema();
    int y = modal.y + 30;
    for (std::size_t index = 0; index < schema.size(); ++index)
    {
        const auto& property = schema[index];
        SDL_Rect row = {modal.x + 8, y, modal.w - 16, 20};
        if (static_cast<int>(index) == app.propsRowEditing)
        {
            setColor(app.renderer, 60, 90, 140);
            SDL_RenderFillRect(app.renderer, &row);
        }
        setColor(app.renderer, 210, 214, 222);
        const std::string value =
            static_cast<int>(index) == app.propsRowEditing ? app.propsEditText + "_" : property.value;
        appfont::drawText(app.renderer, property.displayName + ": " + value +
                          (property.unit.empty() ? "" : " " + property.unit) +
                          (property.editable ? "" : "  (READ ONLY)"),
                          row.x + 4, row.y + 6);
        y += 22;
    }
    if (!app.propsError.empty())
    {
        setColor(app.renderer, 240, 90, 90);
        appfont::drawText(app.renderer, app.propsError, modal.x + 10, modal.y + modal.h - 20);
    }
}

void drawPrompt(App& app)
{
    if (app.prompt == PromptKind::None)
    {
        return;
    }
    SDL_Rect bar = {app.windowWidth / 2 - 260, 70, 520, 52};
    setColor(app.renderer, 28, 31, 40);
    SDL_RenderFillRect(app.renderer, &bar);
    setColor(app.renderer, 120, 128, 140);
    SDL_RenderDrawRect(app.renderer, &bar);
    setColor(app.renderer, 140, 190, 255);
    std::string title = "OPEN - TYPE PATH, ENTER = OK, ESC = CANCEL";
    if (app.prompt == PromptKind::SaveAs)
    {
        title = "SAVE AS - TYPE PATH, ENTER = OK, ESC = CANCEL";
    }
    else if (app.prompt == PromptKind::NewCustom)
    {
        title = "CANVAS SIZE - TYPE WIDTHxHEIGHT, ENTER = OK, ESC = CANCEL";
    }
    appfont::drawText(app.renderer, title, bar.x + 8, bar.y + 6);
    setColor(app.renderer, 235, 238, 242);
    appfont::drawText(app.renderer, app.promptText + "_", bar.x + 8, bar.y + 26);
}

void runDRC(App& app)
{
    const auto components = constAll(app);
    const auto nets = app.wires.buildNets(components);
    std::vector<CircuitConnection> connections;
    for (const auto& net : nets)
    {
        for (const auto& pinRef : net.pins)
        {
            const Component* component = app.manager.getComponent(pinRef.first);
            if (component != nullptr && static_cast<std::size_t>(pinRef.second) < component->getPins().size())
            {
                connections.push_back({pinRef.first, component->getPins()[static_cast<std::size_t>(pinRef.second)].getID(), net.id});
            }
        }
    }
    app.wires.syncPinConnectionFlags(app.manager.getAll());
    const auto findings = app.drc.check(components, connections, &app.log);
    if (findings.empty())
    {
        app.log.info("DRC: no errors, design is clean");
    }
    app.showLog = true;
    showToast(app, "DRC finished: " + std::to_string(findings.size()) + " finding(s)");
}

void saveTo(App& app, const std::string& path)
{
    std::string error;
    if (CircuitSerializer::saveToFile(path, snapshot(app), error))
    {
        app.currentPath = path;
        addRecent(app, path);
        app.log.info("Saved project to " + path);
        showToast(app, "Saved: " + path);
    }
    else
    {
        app.log.error(error);
        showToast(app, error);
    }
}

bool openFrom(App& app, const std::string& path)
{
    std::string json, error;
    if (!CircuitSerializer::loadFromFile(path, json, error) ||
        !CircuitSerializer::fromJSON(json, app.manager, app.wires, app.library, app.canvas, error))
    {
        app.log.error(error);
        showToast(app, error);
        return false;
    }
    app.currentPath = path;
    addRecent(app, path);
    app.undoStack.clear();
    app.redoStack.clear();
    app.sim.stop(app.manager.getAll());
    app.log.info("Opened project " + path);
    showToast(app, "Opened: " + path);
    return true;
}

void exportImage(App& app)
{
    const int width = static_cast<int>(app.canvas.widthUnits);
    const int height = static_cast<int>(app.canvas.heightUnits);
    SDL_Texture* target = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);
    if (target == nullptr)
    {
        showToast(app, "Export failed: cannot create texture");
        return;
    }
    const Camera saved = app.camera;
    app.camera = {0.0f, 0.0f, 1.0f};
    SDL_SetRenderTarget(app.renderer, target);
    setColor(app.renderer, 255, 255, 255);
    SDL_RenderClear(app.renderer);
    SDL_Rect full = {0, 0, width, height};
    drawGridAndPage(app, full);
    drawWires(app);
    for (const Component* component : constAll(app))
    {
        drawComponent(app, component);
    }
    SDL_RenderSetClipRect(app.renderer, nullptr);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    SDL_RenderReadPixels(app.renderer, &full, SDL_PIXELFORMAT_RGBA32, pixels.data(), width * 4);
    SDL_SetRenderTarget(app.renderer, nullptr);
    app.camera = saved;
    SDL_DestroyTexture(target);
    const std::string path = app.currentPath.empty() ? "circuit_export.png" : app.currentPath + ".png";
    if (stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) != 0)
    {
        app.log.info("Exported circuit image to " + path);
        showToast(app, "Exported: " + path);
    }
    else
    {
        showToast(app, "PNG export failed");
    }
}

void newProject(App& app, const std::string& pageSize)
{
    app.manager.clear();
    app.wires.clearAll();
    app.undoStack.clear();
    app.redoStack.clear();
    app.currentPath.clear();
    app.canvas.pageSize = pageSize;
    if (pageSize == "A3")
    {
        app.canvas.widthUnits = 1587.0f;
        app.canvas.heightUnits = 1122.0f;
    }
    else if (pageSize == "A5")
    {
        app.canvas.widthUnits = 793.0f;
        app.canvas.heightUnits = 559.0f;
    }
    else if (pageSize.find('x') != std::string::npos)
    {
        const std::size_t split = pageSize.find('x');
        const float customWidth = std::strtof(pageSize.substr(0, split).c_str(), nullptr);
        const float customHeight = std::strtof(pageSize.substr(split + 1).c_str(), nullptr);
        app.canvas.widthUnits = customWidth >= 100.0f ? customWidth : 1122.0f;
        app.canvas.heightUnits = customHeight >= 100.0f ? customHeight : 793.0f;
        app.canvas.pageSize = "CUSTOM";
    }
    else
    {
        app.canvas.widthUnits = 1122.0f;
        app.canvas.heightUnits = 793.0f;
    }
    app.camera = {40.0f, 60.0f, 1.0f};
    app.sim.stop(app.manager.getAll());
    app.log.clear();
    app.log.info("New " + pageSize + " project created");
    app.screen = Screen::Editor;
}

void handleToolbarClick(App& app, const std::string& id)
{
    if (id == "select") { app.tool = Tool::Select; app.wireDraft = false; }
    else if (id == "wire") { app.tool = Tool::Wire; app.pendingPlacement.clear(); }
    else if (id == "run")
    {
        app.sim.run();
        app.log.info("Simulation running");
    }
    else if (id == "pause") { app.sim.pause(); app.log.info("Simulation paused"); }
    else if (id == "stop") { app.sim.stop(app.manager.getAll()); app.log.info("Simulation stopped, time reset"); }
    else if (id == "step")
    {
        app.sim.step(app.manager.getAll(), app.wires, &app.log);
    }
    else if (id == "drc") { runDRC(app); }
    else if (id == "undo") { doUndo(app); }
    else if (id == "redo") { doRedo(app); }
    else if (id == "save")
    {
        if (app.currentPath.empty())
        {
            app.prompt = PromptKind::SaveAs;
            app.promptText = "circuit.json";
            SDL_StartTextInput();
        }
        else
        {
            saveTo(app, app.currentPath);
        }
    }
    else if (id == "saveas")
    {
        app.prompt = PromptKind::SaveAs;
        app.promptText = app.currentPath.empty() ? "circuit.json" : app.currentPath;
        SDL_StartTextInput();
    }
    else if (id == "open")
    {
        app.prompt = PromptKind::Open;
        app.promptText = app.recents.empty() ? "circuit.json" : app.recents.front();
        SDL_StartTextInput();
    }
    else if (id == "export") { exportImage(app); }
    else if (id == "log") { app.showLog = !app.showLog; }
    else if (id == "menu") { app.screen = Screen::Startup; app.sim.stop(app.manager.getAll()); }
}

void canvasMouseDown(App& app, const SDL_MouseButtonEvent& event)
{
    const float wx = app.camera.worldX(event.x);
    const float wy = app.camera.worldY(event.y);

    if (event.button == SDL_BUTTON_MIDDLE)
    {
        app.panning = true;
        app.panStartX = event.x;
        app.panStartY = event.y;
        app.panOrigX = app.camera.offsetX;
        app.panOrigY = app.camera.offsetY;
        return;
    }
    if (event.button != SDL_BUTTON_LEFT && event.button != SDL_BUTTON_RIGHT)
    {
        return;
    }

    if (event.button == SDL_BUTTON_RIGHT)
    {
        if (app.manager.selectAt(wx, wy))
        {
            pushUndo(app);
            app.manager.deleteSelected();
            showToast(app, "Component deleted");
            return;
        }
        WPoint snapped;
        const int wireID = app.wires.findWireNear(wx, wy, 6.0f / app.camera.zoom, &snapped);
        if (wireID >= 0)
        {
            pushUndo(app);
            app.wires.deleteWire(wireID);
            showToast(app, "Wire deleted");
        }
        return;
    }

    if (!app.pendingPlacement.empty())
    {
        pushUndo(app);
        Component* placed = app.manager.placeComponent(app.library, app.pendingPlacement, wx, wy);
        if (placed != nullptr)
        {
            app.log.info("Placed " + placed->getType() + " #" + std::to_string(placed->getID()));
        }
        return;
    }

    if (app.tool == Tool::Wire)
    {
        int compID = -1, pinIdx = -1;
        if (app.wires.findPinNear(wx, wy, 10.0f / app.camera.zoom, compID, pinIdx, constAll(app)))
        {
            if (!app.wireDraft)
            {
                app.wireDraft = true;
                app.wireFromComp = compID;
                app.wireFromPin = pinIdx;
            }
            else
            {
                pushUndo(app);
                app.wires.addWirePinToPin(app.wireFromComp, app.wireFromPin, compID, pinIdx);
                app.wires.syncPinConnectionFlags(app.manager.getAll());
                app.wireDraft = false;
            }
            return;
        }
        if (app.wireDraft)
        {
            WPoint snapped;
            const int wireID = app.wires.findWireNear(wx, wy, 8.0f / app.camera.zoom, &snapped);
            if (wireID >= 0)
            {
                pushUndo(app);
                const int junctionID = app.wires.createJunctionOnWire(wireID, snapped.x, snapped.y);
                app.wires.addWirePinToJunction(app.wireFromComp, app.wireFromPin, junctionID);
                app.wires.syncPinConnectionFlags(app.manager.getAll());
                app.wireDraft = false;
            }
        }
        return;
    }

    if (app.sim.getState() == SimState::Running)
    {
        for (Component* component : app.manager.getAll())
        {
            if (component->contains(wx, wy))
            {
                if (auto* toggle = dynamic_cast<Switch*>(component))
                {
                    toggle->toggle();
                    return;
                }
                if (auto* button = dynamic_cast<PushButton*>(component))
                {
                    button->press();
                    app.heldButtonComponent = component->getID();
                    return;
                }
            }
        }
    }

    if (app.manager.selectAt(wx, wy))
    {
        app.draggingComponents = true;
        app.dragStartWX = wx;
        app.dragStartWY = wy;
        app.manager.beginDrag();
        return;
    }
    app.wires.selectWireAt(wx, wy, 6.0f / app.camera.zoom, false);
    bool anyWire = false;
    for (const auto& wire : app.wires.getWires())
    {
        anyWire = anyWire || wire.selected;
    }
    if (!anyWire)
    {
        app.rectSelecting = true;
        app.dragStartWX = wx;
        app.dragStartWY = wy;
        app.rectWX = wx;
        app.rectWY = wy;
    }
}

void canvasMouseUp(App& app, const SDL_MouseButtonEvent& event)
{
    (void)event;
    if (app.heldButtonComponent >= 0)
    {
        if (auto* button = dynamic_cast<PushButton*>(app.manager.getComponent(app.heldButtonComponent)))
        {
            button->release();
        }
        app.heldButtonComponent = -1;
    }
    if (app.draggingComponents)
    {
        app.manager.endDrag();
        pushUndo(app);
        app.draggingComponents = false;
    }
    if (app.rectSelecting)
    {
        Rect rect = {
            std::min(app.dragStartWX, app.rectWX), std::min(app.dragStartWY, app.rectWY),
            std::fabs(app.rectWX - app.dragStartWX), std::fabs(app.rectWY - app.dragStartWY)};
        if (rect.width > 2 && rect.height > 2)
        {
            app.manager.selectInRectangle(rect);
        }
        app.rectSelecting = false;
    }
    app.panning = false;
}

void handleTextInput(App& app, const char* text)
{
    if (app.prompt != PromptKind::None)
    {
        app.promptText += text;
    }
    else if (app.propsOpen && app.propsRowEditing >= 0)
    {
        app.propsEditText += text;
    }
    else if (app.searchFocused)
    {
        app.searchText += text;
    }
}

void handleKeyDown(App& app, const SDL_KeyboardEvent& event)
{
    const SDL_Keycode key = event.keysym.sym;

    if (app.prompt != PromptKind::None)
    {
        if (key == SDLK_RETURN)
        {
            const PromptKind kind = app.prompt;
            app.prompt = PromptKind::None;
            SDL_StopTextInput();
            if (kind == PromptKind::SaveAs)
            {
                saveTo(app, app.promptText);
            }
            else
            {
                openFrom(app, app.promptText);
            }
        }
        else if (key == SDLK_ESCAPE)
        {
            app.prompt = PromptKind::None;
            SDL_StopTextInput();
        }
        else if (key == SDLK_BACKSPACE && !app.promptText.empty())
        {
            app.promptText.pop_back();
        }
        return;
    }

    if (app.propsOpen)
    {
        if (key == SDLK_ESCAPE)
        {
            app.propsOpen = false;
            app.propsRowEditing = -1;
            app.propertiesPanel.cancel();
            SDL_StopTextInput();
        }
        else if (key == SDLK_BACKSPACE && app.propsRowEditing >= 0 && !app.propsEditText.empty())
        {
            app.propsEditText.pop_back();
        }
        else if (key == SDLK_RETURN && app.propsRowEditing >= 0)
        {
            const auto schema = app.propertiesPanel.getSchema();
            if (static_cast<std::size_t>(app.propsRowEditing) < schema.size())
            {
                std::string error;
                pushUndo(app);
                if (!app.propertiesPanel.setPendingValue(schema[static_cast<std::size_t>(app.propsRowEditing)].key, app.propsEditText, error) ||
                    !app.propertiesPanel.apply(error))
                {
                    app.propsError = error;
                    app.undoStack.pop_back();
                }
                else
                {
                    app.propsError.clear();
                }
            }
            app.propsRowEditing = -1;
        }
        return;
    }

    if (app.searchFocused)
    {
        if (key == SDLK_BACKSPACE && !app.searchText.empty())
        {
            app.searchText.pop_back();
        }
        else if (key == SDLK_ESCAPE || key == SDLK_RETURN)
        {
            app.searchFocused = false;
            SDL_StopTextInput();
        }
        return;
    }

    switch (key)
    {
    case SDLK_ESCAPE:
        app.wireDraft = false;
        app.pendingPlacement.clear();
        app.manager.clearSelection();
        app.wires.clearSelection();
        break;
    case SDLK_r:
        if (!app.manager.getSelectedIDs().empty())
        {
            pushUndo(app);
            app.manager.rotateSelected();
        }
        break;
    case SDLK_h:
        if (!app.manager.getSelectedIDs().empty())
        {
            pushUndo(app);
            app.manager.mirrorSelectedHorizontal();
        }
        break;
    case SDLK_v:
        if (!app.manager.getSelectedIDs().empty())
        {
            pushUndo(app);
            app.manager.mirrorSelectedVertical();
        }
        break;
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
    {
        bool wireSelected = false;
        for (const auto& wire : app.wires.getWires())
        {
            wireSelected = wireSelected || wire.selected;
        }
        if (!app.manager.getSelectedIDs().empty() || wireSelected)
        {
            pushUndo(app);
            app.manager.deleteSelected();
            app.wires.deleteSelectedWires();
            app.wires.syncPinConnectionFlags(app.manager.getAll());
        }
        break;
    }
    case SDLK_w:
        app.tool = app.tool == Tool::Wire ? Tool::Select : Tool::Wire;
        app.wireDraft = false;
        break;
    case SDLK_SPACE:
        if (app.sim.getState() == SimState::Running)
        {
            app.sim.pause();
        }
        else
        {
            app.sim.run();
        }
        break;
    case SDLK_s:
        app.sim.step(app.manager.getAll(), app.wires, &app.log);
        break;
    case SDLK_p:
    {
        WPoint snapped;
        const int wireID = app.wires.findWireNear(app.mouseWX, app.mouseWY, 8.0f / app.camera.zoom, &snapped);
        if (wireID >= 0)
        {
            app.probedWireID = app.probedWireID == wireID ? -1 : wireID;
            app.scopeSamples.clear();
            showToast(app, app.probedWireID >= 0 ? "Probe attached to wire" : "Probe removed");
        }
        break;
    }
    case SDLK_z:
        doUndo(app);
        break;
    case SDLK_y:
        doRedo(app);
        break;
    default:
        break;
    }
}

void editorEvent(App& app, const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_MOUSEMOTION:
    {
        app.mouseWX = app.camera.worldX(event.motion.x);
        app.mouseWY = app.camera.worldY(event.motion.y);
        if (app.panning)
        {
            app.camera.offsetX = app.panOrigX + static_cast<float>(event.motion.x - app.panStartX);
            app.camera.offsetY = app.panOrigY + static_cast<float>(event.motion.y - app.panStartY);
        }
        else if (app.draggingComponents)
        {
            app.manager.dragSelected(app.mouseWX - app.dragStartWX, app.mouseWY - app.dragStartWY);
        }
        else if (app.rectSelecting)
        {
            app.rectWX = app.mouseWX;
            app.rectWY = app.mouseWY;
        }
        app.hoverPinComp = -1;
        app.hoverPinIdx = -1;
        if (app.tool == Tool::Wire)
        {
            app.wires.findPinNear(app.mouseWX, app.mouseWY, 10.0f / app.camera.zoom,
                                  app.hoverPinComp, app.hoverPinIdx, constAll(app));
        }
        break;
    }
    case SDL_MOUSEWHEEL:
    {
        int mouseX = 0, mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        const float before = app.camera.zoom;
        app.camera.zoom = std::min(4.0f, std::max(0.25f, app.camera.zoom * (event.wheel.y > 0 ? 1.1f : 0.9f)));
        const float scale = app.camera.zoom / before;
        app.camera.offsetX = static_cast<float>(mouseX) - (static_cast<float>(mouseX) - app.camera.offsetX) * scale;
        app.camera.offsetY = static_cast<float>(mouseY) - (static_cast<float>(mouseY) - app.camera.offsetY) * scale;
        break;
    }
    case SDL_MOUSEBUTTONDOWN:
    {
        const int mx = event.button.x;
        const int my = event.button.y;
        if (app.propsOpen)
        {
            SDL_Rect modal = {app.windowWidth / 2 - 220, 120, 440, 300};
            if (mx >= modal.x && mx < modal.x + modal.w && my >= modal.y && my < modal.y + modal.h)
            {
                const int row = (my - modal.y - 30) / 22;
                const auto schema = app.propertiesPanel.getSchema();
                if (row >= 0 && static_cast<std::size_t>(row) < schema.size() && schema[static_cast<std::size_t>(row)].editable)
                {
                    app.propsRowEditing = row;
                    app.propsEditText = schema[static_cast<std::size_t>(row)].value;
                    SDL_StartTextInput();
                }
            }
            else
            {
                app.propsOpen = false;
                app.propertiesPanel.cancel();
                SDL_StopTextInput();
            }
            return;
        }
        if (my < kToolbarHeight)
        {
            for (const auto& button : toolbarButtons(app))
            {
                if (mx >= button.rect.x && mx < button.rect.x + button.rect.w)
                {
                    handleToolbarClick(app, button.id);
                    return;
                }
            }
            return;
        }
        if (mx < kLibraryWidth)
        {
            SDL_Rect search = {6, kToolbarHeight + 6, kLibraryWidth - 12, 22};
            if (my >= search.y && my < search.y + search.h)
            {
                app.searchFocused = true;
                SDL_StartTextInput();
                return;
            }
            app.searchFocused = false;
            if (my >= app.windowHeight - kStatusHeight - 118)
            {
                return;
            }
            for (const auto& row : libraryRows(app))
            {
                if (my < row.rect.y || my >= row.rect.y + row.rect.h)
                {
                    continue;
                }
                if (row.kind == LibraryRowKind::Header)
                {
                    const int group = std::stoi(row.itemName);
                    app.groupOpen[group] = !app.groupOpen[group];
                }
                else if (row.kind == LibraryRowKind::ActiveItem)
                {
                    if (mx >= row.rect.x + row.rect.w - 16)
                    {
                        app.activeList.erase(
                            std::remove(app.activeList.begin(), app.activeList.end(), row.itemName),
                            app.activeList.end());
                        if (app.pendingPlacement == row.itemName)
                        {
                            app.pendingPlacement.clear();
                        }
                    }
                    else
                    {
                        app.pendingPlacement = row.itemName;
                        app.tool = Tool::Select;
                    }
                }
                else if (row.kind == LibraryRowKind::Item)
                {
                    if (mx >= row.rect.x + row.rect.w - 16)
                    {
                        if (std::find(app.activeList.begin(), app.activeList.end(), row.itemName) == app.activeList.end())
                        {
                            app.activeList.push_back(row.itemName);
                        }
                    }
                    else
                    {
                        app.pendingPlacement = row.itemName;
                        app.tool = Tool::Select;
                    }
                }
                return;
            }
            return;
        }
        if (event.button.button == SDL_BUTTON_LEFT && event.button.clicks >= 2)
        {
            const float wx = app.camera.worldX(mx);
            const float wy = app.camera.worldY(my);
            if (app.manager.selectAt(wx, wy))
            {
                const auto ids = app.manager.getSelectedIDs();
                if (!ids.empty())
                {
                    app.propertiesPanel.open(app.manager.getComponent(ids.front()));
                    app.propsOpen = true;
                    app.propsError.clear();
                    app.propsRowEditing = -1;
                }
                return;
            }
        }
        canvasMouseDown(app, event.button);
        break;
    }
    case SDL_MOUSEBUTTONUP:
        canvasMouseUp(app, event.button);
        break;
    case SDL_TEXTINPUT:
        handleTextInput(app, event.text.text);
        break;
    case SDL_KEYDOWN:
        handleKeyDown(app, event.key);
        break;
    default:
        break;
    }
}

std::vector<UIButton> startupButtons(App& app)
{
    std::vector<UIButton> buttons;
    const int cx = app.windowWidth / 2 - 140;
    int y = 210;
    auto add = [&](const std::string& id, const std::string& label)
    {
        buttons.push_back({{cx, y, 280, 34}, label, id});
        y += 44;
    };
    add("new_a4", "NEW PROJECT (A4)");
    add("new_a3", "NEW PROJECT (A3)");
    add("new_a5", "NEW PROJECT (A5)");
    add("new_custom", "NEW PROJECT (CUSTOM SIZE...)");
    add("open", "OPEN PROJECT...");
    y += 8;
    int shown = 0;
    for (const auto& recent : app.recents)
    {
        if (shown >= 5)
        {
            break;
        }
        buttons.push_back({{cx, y, 280, 26}, recent, "recent:" + recent});
        y += 32;
        ++shown;
    }
    return buttons;
}

void drawStartup(App& app)
{
    setColor(app.renderer, 22, 25, 32);
    SDL_RenderClear(app.renderer);
    setColor(app.renderer, 140, 190, 255);
    appfont::drawText(app.renderer, "PROTEUS SIMULATOR", app.windowWidth / 2 - appfont::textWidth("PROTEUS SIMULATOR", 3) / 2, 90, 3);
    setColor(app.renderer, 150, 156, 168);
    appfont::drawText(app.renderer, "OOP PROJECT - GROUP 39: KIARASH, AIDA, NILOUFAR",
                      app.windowWidth / 2 - appfont::textWidth("OOP PROJECT - GROUP 39: KIARASH, AIDA, NILOUFAR") / 2, 140);
    bool first = true;
    for (const auto& button : startupButtons(app))
    {
        const bool recent = button.id.rfind("recent:", 0) == 0;
        setColor(app.renderer, recent ? 38 : 52, recent ? 42 : 58, recent ? 52 : 70);
        SDL_Rect rect = button.rect;
        SDL_RenderFillRect(app.renderer, &rect);
        setColor(app.renderer, 210, 214, 222);
        std::string label = button.label;
        if (static_cast<int>(label.size()) > 44)
        {
            label = "..." + label.substr(label.size() - 41);
        }
        appfont::drawText(app.renderer, label, button.rect.x + 10, button.rect.y + (recent ? 9 : 13));
        if (first && recent)
        {
            first = false;
        }
    }
    if (!app.recents.empty())
    {
        setColor(app.renderer, 120, 126, 138);
        appfont::drawText(app.renderer, "RECENT PROJECTS:", app.windowWidth / 2 - 140, 340);
    }
    drawPrompt(app);
}

void startupEvent(App& app, const SDL_Event& event)
{
    if (event.type == SDL_KEYDOWN && app.prompt != PromptKind::None)
    {
        const SDL_Keycode key = event.key.keysym.sym;
        if (key == SDLK_RETURN)
        {
            const PromptKind kind = app.prompt;
            app.prompt = PromptKind::None;
            SDL_StopTextInput();
            if (kind == PromptKind::NewCustom)
            {
                newProject(app, app.promptText);
            }
            else if (openFrom(app, app.promptText))
            {
                app.screen = Screen::Editor;
            }
        }
        else if (key == SDLK_ESCAPE)
        {
            app.prompt = PromptKind::None;
            SDL_StopTextInput();
        }
        else if (key == SDLK_BACKSPACE && !app.promptText.empty())
        {
            app.promptText.pop_back();
        }
        return;
    }
    if (event.type == SDL_TEXTINPUT && app.prompt != PromptKind::None)
    {
        app.promptText += event.text.text;
        return;
    }
    if (event.type != SDL_MOUSEBUTTONDOWN)
    {
        return;
    }
    for (const auto& button : startupButtons(app))
    {
        const int mx = event.button.x, my = event.button.y;
        if (mx >= button.rect.x && mx < button.rect.x + button.rect.w &&
            my >= button.rect.y && my < button.rect.y + button.rect.h)
        {
            if (button.id == "new_a4")
            {
                newProject(app, "A4");
            }
            else if (button.id == "new_a3")
            {
                newProject(app, "A3");
            }
            else if (button.id == "new_a5")
            {
                newProject(app, "A5");
            }
            else if (button.id == "new_custom")
            {
                app.prompt = PromptKind::NewCustom;
                app.promptText = "1600x1200";
                SDL_StartTextInput();
            }
            else if (button.id == "open")
            {
                app.prompt = PromptKind::Open;
                app.promptText = app.recents.empty() ? "circuit.json" : app.recents.front();
                SDL_StartTextInput();
            }
            else if (button.id.rfind("recent:", 0) == 0)
            {
                if (openFrom(app, button.id.substr(7)))
                {
                    app.screen = Screen::Editor;
                }
            }
            return;
        }
    }
}

void drawEditor(App& app)
{
    setColor(app.renderer, 44, 48, 58);
    SDL_RenderClear(app.renderer);

    SDL_Rect view = {kLibraryWidth, kToolbarHeight, app.windowWidth - kLibraryWidth,
                     app.windowHeight - kToolbarHeight - kStatusHeight};
    drawGridAndPage(app, view);
    drawWires(app);
    for (const Component* component : constAll(app))
    {
        drawComponent(app, component);
    }
    if (app.rectSelecting)
    {
        setColor(app.renderer, 90, 140, 240, 90);
        SDL_Rect rect = {
            std::min(app.camera.screenX(app.dragStartWX), app.camera.screenX(app.rectWX)),
            std::min(app.camera.screenY(app.dragStartWY), app.camera.screenY(app.rectWY)),
            std::abs(app.camera.screenX(app.rectWX) - app.camera.screenX(app.dragStartWX)),
            std::abs(app.camera.screenY(app.rectWY) - app.camera.screenY(app.dragStartWY))};
        SDL_RenderDrawRect(app.renderer, &rect);
    }
    if (app.sim.getState() != SimState::Stopped)
    {
        WPoint snapped;
        const int wireID = app.wires.findWireNear(app.mouseWX, app.mouseWY, 6.0f / app.camera.zoom, &snapped);
        if (wireID >= 0)
        {
            NetValue value;
            if (app.sim.valueForWire(wireID, app.wires, constAll(app), value))
            {
                std::ostringstream stream;
                stream.precision(2);
                stream << std::fixed << value.voltage << " V";
                const int sx = app.camera.screenX(snapped.x) + 10;
                const int sy = app.camera.screenY(snapped.y) - 18;
                setColor(app.renderer, 16, 18, 24);
                SDL_Rect tip = {sx - 3, sy - 3, appfont::textWidth(stream.str()) + 6, 14};
                SDL_RenderFillRect(app.renderer, &tip);
                setColor(app.renderer, 120, 230, 160);
                appfont::drawText(app.renderer, stream.str(), sx, sy);
            }
        }
    }
    SDL_RenderSetClipRect(app.renderer, nullptr);

    drawToolbar(app);
    drawLibraryPanel(app);
    drawLogPanel(app);
    drawScope(app);
    drawStatusBar(app);
    drawPropertiesModal(app);
    drawPrompt(app);
    if (SDL_GetTicks() < app.toastUntil && !app.toast.empty())
    {
        setColor(app.renderer, 16, 18, 24);
        SDL_Rect toast = {app.windowWidth / 2 - appfont::textWidth(app.toast) / 2 - 8, kToolbarHeight + 6,
                          appfont::textWidth(app.toast) + 16, 20};
        SDL_RenderFillRect(app.renderer, &toast);
        setColor(app.renderer, 235, 238, 242);
        appfont::drawText(app.renderer, app.toast, toast.x + 8, toast.y + 6);
    }
}

}

int main(int argc, char** argv)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    App app;
    app.window = SDL_CreateWindow(
        "Proteus Simulator - Group 39", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        app.windowWidth, app.windowHeight, SDL_WINDOW_RESIZABLE);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (app.renderer == nullptr)
    {
        app.renderer = SDL_CreateRenderer(app.window, -1, 0);
    }
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

    loadRecents(app);
    app.wires.setComponentLookup([&app](int id) { return static_cast<const ComponentManager&>(app.manager).getComponent(id); });
    app.manager.setComponentDeletedCallback([&app](int id) { app.wires.removeWiresForComponent(id); });

    bool smokeTest = false;
    bool galleryTest = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::string arg = argv[index];
        if (arg == "--smoke-test")
        {
            smokeTest = true;
        }
        else if (arg == "--gallery-test")
        {
            smokeTest = true;
            galleryTest = true;
        }
        else
        {
            if (openFrom(app, arg))
            {
                app.screen = Screen::Editor;
            }
        }
    }

    Uint32 lastTicks = SDL_GetTicks();
    int smokeFrames = 0;
    while (app.running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                app.running = false;
            }
            else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                app.windowWidth = event.window.data1;
                app.windowHeight = event.window.data2;
            }
            else if (app.screen == Screen::Startup)
            {
                startupEvent(app, event);
            }
            else
            {
                editorEvent(app, event);
            }
        }

        const Uint32 now = SDL_GetTicks();
        const double dt = std::min(0.05, static_cast<double>(now - lastTicks) / 1000.0);
        lastTicks = now;
        if (app.screen == Screen::Editor)
        {
            app.sim.tick(dt, app.manager.getAll(), app.wires, &app.log);
            app.wires.syncPinConnectionFlags(app.manager.getAll());
            if (app.probedWireID >= 0 && app.sim.getState() != SimState::Stopped)
            {
                NetValue value;
                if (app.sim.valueForWire(app.probedWireID, app.wires, constAll(app), value))
                {
                    app.scopeSamples.push_back({static_cast<float>(app.sim.getTime()), value.voltage});
                    if (app.scopeSamples.size() > 400)
                    {
                        app.scopeSamples.pop_front();
                    }
                }
            }
        }

        if (app.screen == Screen::Startup)
        {
            drawStartup(app);
        }
        else
        {
            drawEditor(app);
        }
        SDL_RenderPresent(app.renderer);

        if (smokeTest)
        {
            ++smokeFrames;
            if (smokeFrames == 2 && app.screen == Screen::Startup)
            {
                newProject(app, "A4");
                if (galleryTest)
                {
                    const char* names[] = {
                        "GND", "DCVoltageSource", "Battery", "PulseSource",
                        "Resistor", "Capacitor", "Inductor",
                        "Switch", "PushButton", "LED", "SevenSegmentDisplay",
                        "GateAND", "GateOR", "GateNOT", "GateXOR", "GateNAND",
                        "DFlipFlop", "ADC", "DAC", "Voltmeter", "Ammeter"};
                    int column = 0, rowIndex = 0;
                    for (const char* name : names)
                    {
                        Component* placed = app.manager.placeComponent(
                            app.library, name, 260.0f + column * 160.0f, 80.0f + rowIndex * 120.0f);
                        if (placed != nullptr)
                        {
                            if (column == 1) { placed->rotateClockwise(); }
                            if (column == 2) { placed->mirrorHorizontal(); }
                        }
                        if (++column == 5) { column = 0; ++rowIndex; }
                    }
                    app.wires.addWirePinToPin(2, 0, 5, 0);
                    const int clockWire = app.wires.addWirePinToPin(4, 0, 6, 0);
                    app.probedWireID = clockWire;
                    app.pendingPlacement = "GateNAND";
                    app.activeList = {"Resistor", "LED", "GateAND"};
                    app.sim.run();
                    for (int sample = 0; sample < 40; ++sample)
                    {
                        app.sim.step(app.manager.getAll(), app.wires, &app.log);
                        NetValue probed;
                        if (app.sim.valueForWire(clockWire, app.wires, constAll(app), probed))
                        {
                            app.scopeSamples.push_back({static_cast<float>(app.sim.getTime()), probed.voltage});
                        }
                    }
                }
                else
                {
                    app.manager.placeComponent(app.library, "DCVoltageSource", 200, 200);
                    app.manager.placeComponent(app.library, "Resistor", 340, 200);
                    app.manager.placeComponent(app.library, "LED", 480, 200);
                    app.manager.placeComponent(app.library, "GND", 620, 200);
                    app.wires.addWirePinToPin(1, 0, 2, 0);
                    app.wires.addWirePinToPin(2, 1, 3, 0);
                    app.wires.addWirePinToPin(3, 1, 4, 0);
                    app.sim.run();
                }
            }
            if (galleryTest && smokeFrames == 10)
            {
                std::vector<unsigned char> pixels(static_cast<std::size_t>(app.windowWidth) * app.windowHeight * 4);
                SDL_Rect full = {0, 0, app.windowWidth, app.windowHeight};
                if (SDL_RenderReadPixels(app.renderer, &full, SDL_PIXELFORMAT_RGBA32,
                                         pixels.data(), app.windowWidth * 4) == 0)
                {
                    stbi_write_png("/tmp/gallery.png", app.windowWidth, app.windowHeight, 4,
                                   pixels.data(), app.windowWidth * 4);
                    std::cout << "GALLERY WRITTEN /tmp/gallery.png\n";
                }
            }
            if (smokeFrames >= 12)
            {
                runDRC(app);
                exportImage(app);
                std::string error;
                CircuitSerializer::saveToFile("/tmp/smoke_circuit.json", snapshot(app), error);
                std::cout << "SMOKE TEST OK, components=" << app.manager.componentCount()
                          << " wires=" << app.wires.getWires().size()
                          << " state=" << (app.sim.getState() == SimState::Running ? "running" : "other")
                          << "\n";
                app.running = false;
            }
        }
    }

    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
