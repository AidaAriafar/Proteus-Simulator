/*
    Project 8 - Proteus-style Logic Gate Simulation Control
    C++14 + SDL2

    Gates included:
      BUFFER, NOT, AND, NAND, OR, NOR, XOR, XNOR

    Features:
      - Run / Pause / Stop
      - Fixed step and next-event step
      - Event-driven propagation delay for every gate
      - Live HIGH / LOW / FLOAT wire colors
      - Interactive inputs A and B
      - Proteus-like ANSI gate symbols and output LEDs
      - Click a gate to show its truth table

    Controls:
      Mouse left on A/B : toggle LOW/HIGH
      Mouse right on A/B: cycle LOW/HIGH/FLOAT
      A / B             : toggle corresponding input
      1 / 2             : cycle corresponding input including FLOAT
      Space             : Run/Pause
      S                 : fixed step 0.10 s
      N                 : advance to next event
      R                 : Stop/reset
*/

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace project8 {

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

template <typename T>
T clampValue(const T& value, const T& minimum, const T& maximum) {
    return std::max(minimum, std::min(value, maximum));
}

struct Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
};

static const Color BACKGROUND       = {21, 25, 32, 255};
static const Color PANEL            = {32, 38, 48, 255};
static const Color PANEL_LIGHT      = {44, 52, 65, 255};
static const Color GRID             = {38, 44, 55, 255};
static const Color WHITE            = {237, 241, 247, 255};
static const Color MUTED            = {151, 162, 178, 255};
static const Color CYAN             = {72, 202, 228, 255};
static const Color GREEN            = {70, 214, 118, 255};
static const Color RED              = {242, 72, 88, 255};
static const Color BLUE             = {64, 119, 255, 255};
static const Color YELLOW           = {255, 197, 61, 255};
static const Color ORANGE           = {255, 145, 77, 255};
static const Color PURPLE           = {178, 124, 255, 255};
static const Color BLACK            = {9, 12, 17, 255};
static const Color DEFAULT_WIRE     = {107, 118, 134, 255};
static const Color GATE_FILL        = {229, 233, 238, 255};
static const Color GATE_OUTLINE     = {25, 29, 36, 255};
static const Color SELECTED         = {255, 214, 92, 255};

void setColor(SDL_Renderer* renderer, const Color& color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fillRect(SDL_Renderer* renderer, const SDL_Rect& rect, const Color& color) {
    setColor(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

void drawRect(SDL_Renderer* renderer, const SDL_Rect& rect, const Color& color) {
    setColor(renderer, color);
    SDL_RenderDrawRect(renderer, &rect);
}

void drawLine(SDL_Renderer* renderer,
              int x1, int y1, int x2, int y2,
              const Color& color) {
    setColor(renderer, color);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void drawThickLine(SDL_Renderer* renderer,
                   int x1, int y1, int x2, int y2,
                   const Color& color,
                   int thickness = 4) {
    setColor(renderer, color);
    if (std::abs(x2 - x1) >= std::abs(y2 - y1)) {
        for (int offset = -thickness / 2; offset <= thickness / 2; ++offset) {
            SDL_RenderDrawLine(renderer, x1, y1 + offset, x2, y2 + offset);
        }
    } else {
        for (int offset = -thickness / 2; offset <= thickness / 2; ++offset) {
            SDL_RenderDrawLine(renderer, x1 + offset, y1, x2 + offset, y2);
        }
    }
}

void drawFilledCircle(SDL_Renderer* renderer,
                      int centerX,
                      int centerY,
                      int radius,
                      const Color& color) {
    setColor(renderer, color);
    for (int y = -radius; y <= radius; ++y) {
        const int width = static_cast<int>(std::sqrt(
            static_cast<double>(radius * radius - y * y)));
        SDL_RenderDrawLine(renderer,
                           centerX - width, centerY + y,
                           centerX + width, centerY + y);
    }
}

void drawCircle(SDL_Renderer* renderer,
                int centerX,
                int centerY,
                int radius,
                const Color& color) {
    setColor(renderer, color);
    int x = radius;
    int y = 0;
    int error = 0;
    while (x >= y) {
        SDL_RenderDrawPoint(renderer, centerX + x, centerY + y);
        SDL_RenderDrawPoint(renderer, centerX + y, centerY + x);
        SDL_RenderDrawPoint(renderer, centerX - y, centerY + x);
        SDL_RenderDrawPoint(renderer, centerX - x, centerY + y);
        SDL_RenderDrawPoint(renderer, centerX - x, centerY - y);
        SDL_RenderDrawPoint(renderer, centerX - y, centerY - x);
        SDL_RenderDrawPoint(renderer, centerX + y, centerY - x);
        SDL_RenderDrawPoint(renderer, centerX + x, centerY - y);
        if (error <= 0) {
            ++y;
            error += 2 * y + 1;
        }
        if (error > 0) {
            --x;
            error -= 2 * x + 1;
        }
    }
}

bool contains(const SDL_Rect& rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

void drawPolyline(SDL_Renderer* renderer,
                  const std::vector<SDL_Point>& points,
                  const Color& color,
                  int thickness = 2) {
    if (points.size() < 2u) return;
    for (std::size_t i = 1; i < points.size(); ++i) {
        drawThickLine(renderer,
                      points[i - 1].x, points[i - 1].y,
                      points[i].x, points[i].y,
                      color, thickness);
    }
}

SDL_Point quadraticPoint(const SDL_Point& p0,
                         const SDL_Point& p1,
                         const SDL_Point& p2,
                         double t) {
    const double u = 1.0 - t;
    SDL_Point point;
    point.x = static_cast<int>(u * u * p0.x + 2.0 * u * t * p1.x + t * t * p2.x);
    point.y = static_cast<int>(u * u * p0.y + 2.0 * u * t * p1.y + t * t * p2.y);
    return point;
}

void drawQuadratic(SDL_Renderer* renderer,
                   const SDL_Point& p0,
                   const SDL_Point& p1,
                   const SDL_Point& p2,
                   const Color& color,
                   int thickness = 2) {
    SDL_Point previous = p0;
    for (int i = 1; i <= 28; ++i) {
        const double t = static_cast<double>(i) / 28.0;
        const SDL_Point current = quadraticPoint(p0, p1, p2, t);
        drawThickLine(renderer,
                      previous.x, previous.y,
                      current.x, current.y,
                      color, thickness);
        previous = current;
    }
}

// -----------------------------------------------------------------------------
// Tiny 5x7 font, no SDL_ttf dependency
// -----------------------------------------------------------------------------

using Glyph = std::array<std::uint8_t, 7>;

Glyph glyphFor(char character) {
    const char c = static_cast<char>(std::toupper(
        static_cast<unsigned char>(character)));
    switch (c) {
        case 'A': return {{14,17,17,31,17,17,17}};
        case 'B': return {{30,17,17,30,17,17,30}};
        case 'C': return {{14,17,16,16,16,17,14}};
        case 'D': return {{30,17,17,17,17,17,30}};
        case 'E': return {{31,16,16,30,16,16,31}};
        case 'F': return {{31,16,16,30,16,16,16}};
        case 'G': return {{14,17,16,23,17,17,15}};
        case 'H': return {{17,17,17,31,17,17,17}};
        case 'I': return {{14,4,4,4,4,4,14}};
        case 'J': return {{7,2,2,2,18,18,12}};
        case 'K': return {{17,18,20,24,20,18,17}};
        case 'L': return {{16,16,16,16,16,16,31}};
        case 'M': return {{17,27,21,21,17,17,17}};
        case 'N': return {{17,25,21,19,17,17,17}};
        case 'O': return {{14,17,17,17,17,17,14}};
        case 'P': return {{30,17,17,30,16,16,16}};
        case 'Q': return {{14,17,17,17,21,18,13}};
        case 'R': return {{30,17,17,30,20,18,17}};
        case 'S': return {{15,16,16,14,1,1,30}};
        case 'T': return {{31,4,4,4,4,4,4}};
        case 'U': return {{17,17,17,17,17,17,14}};
        case 'V': return {{17,17,17,17,17,10,4}};
        case 'W': return {{17,17,17,21,21,21,10}};
        case 'X': return {{17,17,10,4,10,17,17}};
        case 'Y': return {{17,17,10,4,4,4,4}};
        case 'Z': return {{31,1,2,4,8,16,31}};
        case '0': return {{14,17,19,21,25,17,14}};
        case '1': return {{4,12,4,4,4,4,14}};
        case '2': return {{14,17,1,2,4,8,31}};
        case '3': return {{30,1,1,14,1,1,30}};
        case '4': return {{2,6,10,18,31,2,2}};
        case '5': return {{31,16,16,30,1,1,30}};
        case '6': return {{14,16,16,30,17,17,14}};
        case '7': return {{31,1,2,4,8,8,8}};
        case '8': return {{14,17,17,14,17,17,14}};
        case '9': return {{14,17,17,15,1,1,14}};
        case '-': return {{0,0,0,31,0,0,0}};
        case '.': return {{0,0,0,0,0,12,12}};
        case ':': return {{0,12,12,0,12,12,0}};
        case '/': return {{1,2,2,4,8,8,16}};
        case '=': return {{0,31,0,31,0,0,0}};
        case '(': return {{2,4,8,8,8,4,2}};
        case ')': return {{8,4,2,2,2,4,8}};
        case '[': return {{14,8,8,8,8,8,14}};
        case ']': return {{14,2,2,2,2,2,14}};
        case '?': return {{14,17,1,2,4,0,4}};
        case '_': return {{0,0,0,0,0,0,31}};
        case '>': return {{16,8,4,2,4,8,16}};
        case '<': return {{1,2,4,8,4,2,1}};
        case ' ': return {{0,0,0,0,0,0,0}};
        default:  return {{31,17,1,6,4,0,4}};
    }
}

int textWidth(const std::string& text, int scale) {
    return static_cast<int>(text.size()) * 6 * scale;
}

void drawText(SDL_Renderer* renderer,
              const std::string& text,
              int x, int y,
              int scale,
              const Color& color) {
    setColor(renderer, color);
    int cursorX = x;
    for (char c : text) {
        const Glyph glyph = glyphFor(c);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph[row] & (1u << (4 - column))) != 0u) {
                    SDL_Rect pixel = {
                        cursorX + column * scale,
                        y + row * scale,
                        scale,
                        scale
                    };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        cursorX += 6 * scale;
    }
}

void drawCenteredText(SDL_Renderer* renderer,
                      const std::string& text,
                      const SDL_Rect& rect,
                      int scale,
                      const Color& color) {
    drawText(renderer,
             text,
             rect.x + (rect.w - textWidth(text, scale)) / 2,
             rect.y + (rect.h - 7 * scale) / 2,
             scale,
             color);
}

std::string formatDouble(double value, int precision = 2) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

// -----------------------------------------------------------------------------
// Digital logic model
// -----------------------------------------------------------------------------

enum class LogicState {
    Low,
    High,
    Undefined
};

std::string logicName(LogicState state) {
    switch (state) {
        case LogicState::Low: return "LOW";
        case LogicState::High: return "HIGH";
        default: return "FLOAT";
    }
}

std::string logicShort(LogicState state) {
    switch (state) {
        case LogicState::Low: return "0";
        case LogicState::High: return "1";
        default: return "X";
    }
}

Color logicColor(LogicState state, bool stopped) {
    if (stopped) return DEFAULT_WIRE;
    switch (state) {
        case LogicState::Low: return BLUE;
        case LogicState::High: return RED;
        default: return YELLOW;
    }
}

LogicState invertLogic(LogicState value) {
    if (value == LogicState::Undefined) return LogicState::Undefined;
    return value == LogicState::High ? LogicState::Low : LogicState::High;
}

LogicState logicAnd(LogicState a, LogicState b) {
    if (a == LogicState::Low || b == LogicState::Low) return LogicState::Low;
    if (a == LogicState::High && b == LogicState::High) return LogicState::High;
    return LogicState::Undefined;
}

LogicState logicOr(LogicState a, LogicState b) {
    if (a == LogicState::High || b == LogicState::High) return LogicState::High;
    if (a == LogicState::Low && b == LogicState::Low) return LogicState::Low;
    return LogicState::Undefined;
}

LogicState logicXor(LogicState a, LogicState b) {
    if (a == LogicState::Undefined || b == LogicState::Undefined) {
        return LogicState::Undefined;
    }
    return a == b ? LogicState::Low : LogicState::High;
}

enum class SimulationMode {
    Stopped,
    Paused,
    Running
};

// -----------------------------------------------------------------------------
// Event-driven scheduler
// -----------------------------------------------------------------------------

class EventScheduler {
public:
    using Callback = std::function<void()>;

    EventScheduler() : currentTime_(0.0), nextSequence_(0) {}

    double now() const { return currentTime_; }
    bool empty() const { return events_.empty(); }
    std::size_t size() const { return events_.size(); }

    double nextEventTime() const {
        return events_.empty() ? currentTime_ : events_.top().time;
    }

    void clear() {
        while (!events_.empty()) events_.pop();
        currentTime_ = 0.0;
        nextSequence_ = 0;
    }

    void scheduleAfter(double delay, const std::string& label, Callback callback) {
        Event event;
        event.time = currentTime_ + std::max(0.0, delay);
        event.sequence = nextSequence_++;
        event.label = label;
        event.callback = std::move(callback);
        events_.push(std::move(event));
    }

    void runUntil(double targetTime) {
        targetTime = std::max(targetTime, currentTime_);
        while (!events_.empty() && events_.top().time <= targetTime + 1e-9) {
            Event event = events_.top();
            events_.pop();
            currentTime_ = event.time;
            if (event.callback) event.callback();
        }
        currentTime_ = targetTime;
    }

    void runNext() {
        if (events_.empty()) return;
        runUntil(events_.top().time);
    }

private:
    struct Event {
        double time;
        std::uint64_t sequence;
        std::string label;
        Callback callback;
    };

    struct Compare {
        bool operator()(const Event& left, const Event& right) const {
            if (left.time != right.time) return left.time > right.time;
            return left.sequence > right.sequence;
        }
    };

    double currentTime_;
    std::uint64_t nextSequence_;
    std::priority_queue<Event, std::vector<Event>, Compare> events_;
};

class SimulationController {
public:
    SimulationController()
        : mode_(SimulationMode::Stopped), fixedStep_(0.10) {}

    SimulationMode mode() const { return mode_; }
    EventScheduler& scheduler() { return scheduler_; }
    const EventScheduler& scheduler() const { return scheduler_; }
    double time() const { return scheduler_.now(); }

    void startPaused() { mode_ = SimulationMode::Paused; }
    void run() { mode_ = SimulationMode::Running; }
    void pause() {
        if (mode_ != SimulationMode::Stopped) mode_ = SimulationMode::Paused;
    }
    void stop() {
        mode_ = SimulationMode::Stopped;
        scheduler_.clear();
    }
    void update(double realDelta) {
        if (mode_ == SimulationMode::Running) {
            scheduler_.runUntil(scheduler_.now() + clampValue(realDelta, 0.0, 0.05));
        }
    }
    void fixedStep() {
        if (mode_ == SimulationMode::Stopped) return;
        mode_ = SimulationMode::Paused;
        scheduler_.runUntil(scheduler_.now() + fixedStep_);
    }
    void nextEvent() {
        if (mode_ == SimulationMode::Stopped) return;
        mode_ = SimulationMode::Paused;
        scheduler_.runNext();
    }

private:
    SimulationMode mode_;
    double fixedStep_;
    EventScheduler scheduler_;
};

class SimulationLog {
public:
    void clear() { lines_.clear(); }

    void add(double time, const std::string& message) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2)
               << "T=" << time << " " << message;
        lines_.push_back(stream.str());
        if (lines_.size() > 9u) lines_.erase(lines_.begin());
    }

    const std::vector<std::string>& lines() const { return lines_; }

private:
    std::vector<std::string> lines_;
};

// -----------------------------------------------------------------------------
// Input switch
// -----------------------------------------------------------------------------

class InputSwitch {
public:
    InputSwitch(const std::string& label, const SDL_Rect& bounds)
        : label_(label), bounds_(bounds), state_(LogicState::Low) {}

    const SDL_Rect& bounds() const { return bounds_; }
    LogicState state() const { return state_; }

    void reset() { state_ = LogicState::Low; }

    void toggleBinary() {
        state_ = (state_ == LogicState::High) ? LogicState::Low : LogicState::High;
    }

    void cycle() {
        if (state_ == LogicState::Low) state_ = LogicState::High;
        else if (state_ == LogicState::High) state_ = LogicState::Undefined;
        else state_ = LogicState::Low;
    }

    void draw(SDL_Renderer* renderer, bool stopped) const {
        fillRect(renderer, bounds_, PANEL_LIGHT);
        drawRect(renderer, bounds_, CYAN);
        drawText(renderer, "INPUT " + label_, bounds_.x + 14, bounds_.y + 11, 2, WHITE);

        SDL_Rect switchBody = {bounds_.x + 18, bounds_.y + 45, bounds_.w - 36, 34};
        fillRect(renderer, switchBody, logicColor(state_, stopped));
        drawRect(renderer, switchBody, WHITE);

        const int knobX = state_ == LogicState::Low
            ? switchBody.x + 16
            : (state_ == LogicState::High
                ? switchBody.x + switchBody.w - 16
                : switchBody.x + switchBody.w / 2);
        drawFilledCircle(renderer, knobX, switchBody.y + switchBody.h / 2, 12, WHITE);

        drawText(renderer,
                 stopped ? "EDIT" : logicName(state_),
                 bounds_.x + 26,
                 bounds_.y + 91,
                 2,
                 stopped ? MUTED : logicColor(state_, false));
        drawFilledCircle(renderer,
                         bounds_.x + bounds_.w,
                         bounds_.y + bounds_.h / 2,
                         5,
                         WHITE);
    }

private:
    std::string label_;
    SDL_Rect bounds_;
    LogicState state_;
};

// -----------------------------------------------------------------------------
// Logic gates
// -----------------------------------------------------------------------------

enum class GateType {
    Buffer,
    Not,
    And,
    Nand,
    Or,
    Nor,
    Xor,
    Xnor
};

std::string gateName(GateType type) {
    switch (type) {
        case GateType::Buffer: return "BUFFER";
        case GateType::Not: return "NOT";
        case GateType::And: return "AND";
        case GateType::Nand: return "NAND";
        case GateType::Or: return "OR";
        case GateType::Nor: return "NOR";
        case GateType::Xor: return "XOR";
        default: return "XNOR";
    }
}

bool gateIsUnary(GateType type) {
    return type == GateType::Buffer || type == GateType::Not;
}

bool gateHasBubble(GateType type) {
    return type == GateType::Not || type == GateType::Nand ||
           type == GateType::Nor || type == GateType::Xnor;
}

bool gateIsOrFamily(GateType type) {
    return type == GateType::Or || type == GateType::Nor ||
           type == GateType::Xor || type == GateType::Xnor;
}

bool gateIsXorFamily(GateType type) {
    return type == GateType::Xor || type == GateType::Xnor;
}

LogicState evaluateGate(GateType type, LogicState a, LogicState b) {
    switch (type) {
        case GateType::Buffer: return a;
        case GateType::Not: return invertLogic(a);
        case GateType::And: return logicAnd(a, b);
        case GateType::Nand: return invertLogic(logicAnd(a, b));
        case GateType::Or: return logicOr(a, b);
        case GateType::Nor: return invertLogic(logicOr(a, b));
        case GateType::Xor: return logicXor(a, b);
        case GateType::Xnor: return invertLogic(logicXor(a, b));
    }
    return LogicState::Undefined;
}

class LogicGate {
public:
    LogicGate(GateType type,
              const SDL_Rect& bounds,
              double delay,
              SimulationLog& log)
        : type_(type),
          bounds_(bounds),
          output_(LogicState::Undefined),
          delay_(delay),
          generation_(0),
          selected_(false),
          log_(log) {}

    GateType type() const { return type_; }
    LogicState output() const { return output_; }
    const SDL_Rect& bounds() const { return bounds_; }
    double delay() const { return delay_; }
    bool selected() const { return selected_; }
    void setSelected(bool value) { selected_ = value; }

    void reset() {
        output_ = LogicState::Undefined;
        ++generation_;
        selected_ = false;
    }

    SDL_Point inputA() const {
        return SDL_Point{bounds_.x, gateIsUnary(type_)
            ? bounds_.y + bounds_.h / 2
            : bounds_.y + bounds_.h / 2 - 16};
    }

    SDL_Point inputB() const {
        return SDL_Point{bounds_.x, bounds_.y + bounds_.h / 2 + 16};
    }

    SDL_Point outputPin() const {
        const int bubbleExtension = gateHasBubble(type_) ? 12 : 0;
        return SDL_Point{bounds_.x + bounds_.w + bubbleExtension,
                         bounds_.y + bounds_.h / 2};
    }

    void request(LogicState a,
                 LogicState b,
                 SimulationController& controller) {
        const LogicState target = evaluateGate(type_, a, b);
        const std::uint64_t token = ++generation_;
        if (target == output_) return;

        log_.add(controller.time(), gateName(type_) + " REQUEST " + logicShort(target));
        controller.scheduler().scheduleAfter(
            delay_,
            gateName(type_) + " propagation",
            [this, target, token, &controller]() {
                if (token != generation_) return;
                output_ = target;
                log_.add(controller.time(),
                         gateName(type_) + " OUTPUT " + logicShort(output_));
            });
    }

    void draw(SDL_Renderer* renderer, bool stopped) const {
        if (selected_) {
            SDL_Rect highlight = {bounds_.x - 10, bounds_.y - 10,
                                  bounds_.w + 32, bounds_.h + 20};
            drawRect(renderer, highlight, SELECTED);
        }

        if (gateIsUnary(type_)) drawTriangleGate(renderer);
        else if (gateIsOrFamily(type_)) drawOrGate(renderer);
        else drawAndGate(renderer);

        const std::string label = gateName(type_);
        drawText(renderer,
                 label,
                 bounds_.x + (bounds_.w - textWidth(label, 2)) / 2,
                 bounds_.y - 25,
                 2,
                 selected_ ? SELECTED : WHITE);

        drawText(renderer,
                 "D=" + formatDouble(delay_, 2) + "S",
                 bounds_.x + (bounds_.w - textWidth("D=0.20S", 1)) / 2,
                 bounds_.y + bounds_.h + 8,
                 1,
                 MUTED);

        const SDL_Point a = inputA();
        drawFilledCircle(renderer, a.x, a.y, 4, WHITE);
        if (!gateIsUnary(type_)) {
            const SDL_Point b = inputB();
            drawFilledCircle(renderer, b.x, b.y, 4, WHITE);
        }
        const SDL_Point out = outputPin();
        drawFilledCircle(renderer, out.x, out.y, 4, WHITE);

        const Color outColor = logicColor(output_, stopped);
        drawText(renderer,
                 stopped ? "OUT -" : "OUT " + logicShort(output_),
                 bounds_.x + bounds_.w - 58,
                 bounds_.y + bounds_.h + 23,
                 1,
                 stopped ? MUTED : outColor);
    }

private:
    void drawTriangleGate(SDL_Renderer* renderer) const {
        const int x0 = bounds_.x + 10;
        const int y0 = bounds_.y + 8;
        const int x1 = bounds_.x + 10;
        const int y1 = bounds_.y + bounds_.h - 8;
        const int x2 = bounds_.x + bounds_.w - 12;
        const int y2 = bounds_.y + bounds_.h / 2;

        // Fill by horizontal interpolation.
        for (int y = y0; y <= y1; ++y) {
            const double normalized = std::abs(
                static_cast<double>(y - y2)) /
                static_cast<double>((y1 - y0) / 2);
            const int right = static_cast<int>(x2 - normalized * (x2 - x0));
            drawLine(renderer, x0, y, std::max(x0, right), y, GATE_FILL);
        }
        drawThickLine(renderer, x0, y0, x0, y1, GATE_OUTLINE, 2);
        drawThickLine(renderer, x0, y0, x2, y2, GATE_OUTLINE, 2);
        drawThickLine(renderer, x0, y1, x2, y2, GATE_OUTLINE, 2);

        if (gateHasBubble(type_)) {
            drawFilledCircle(renderer, x2 + 7, y2, 7, GATE_FILL);
            drawCircle(renderer, x2 + 7, y2, 7, GATE_OUTLINE);
        }
    }

    void drawAndGate(SDL_Renderer* renderer) const {
        const int left = bounds_.x + 12;
        const int top = bounds_.y + 7;
        const int bottom = bounds_.y + bounds_.h - 7;
        const int centerY = bounds_.y + bounds_.h / 2;
        const int radius = (bottom - top) / 2;
        const int arcCenterX = bounds_.x + bounds_.w - radius - 8;

        SDL_Rect leftFill = {left, top, arcCenterX - left + 1, bottom - top + 1};
        fillRect(renderer, leftFill, GATE_FILL);
        for (int y = -radius; y <= radius; ++y) {
            const int width = static_cast<int>(std::sqrt(
                static_cast<double>(radius * radius - y * y)));
            drawLine(renderer, arcCenterX, centerY + y,
                     arcCenterX + width, centerY + y, GATE_FILL);
        }

        drawThickLine(renderer, left, top, left, bottom, GATE_OUTLINE, 2);
        drawThickLine(renderer, left, top, arcCenterX, top, GATE_OUTLINE, 2);
        drawThickLine(renderer, left, bottom, arcCenterX, bottom, GATE_OUTLINE, 2);
        for (int angle = -90; angle < 90; ++angle) {
            const double radians = angle * 3.14159265358979323846 / 180.0;
            const int x = arcCenterX + static_cast<int>(radius * std::cos(radians));
            const int y = centerY + static_cast<int>(radius * std::sin(radians));
            drawFilledCircle(renderer, x, y, 1, GATE_OUTLINE);
        }

        if (gateHasBubble(type_)) {
            const int bubbleX = arcCenterX + radius + 7;
            drawFilledCircle(renderer, bubbleX, centerY, 7, GATE_FILL);
            drawCircle(renderer, bubbleX, centerY, 7, GATE_OUTLINE);
        }
    }

    void drawOrGate(SDL_Renderer* renderer) const {
        const int left = bounds_.x + 10;
        const int top = bounds_.y + 7;
        const int bottom = bounds_.y + bounds_.h - 7;
        const int right = bounds_.x + bounds_.w - 12;
        const int centerY = bounds_.y + bounds_.h / 2;

        // Approximate filled OR shape with scanlines between two curves.
        for (int y = top; y <= bottom; ++y) {
            const double t = static_cast<double>(y - top) /
                             static_cast<double>(bottom - top);
            const double symmetric = std::abs(2.0 * t - 1.0);
            const int leftX = left + static_cast<int>((1.0 - symmetric) * 23.0);
            const int rightX = right - static_cast<int>(symmetric * 38.0);
            drawLine(renderer, leftX, y, std::max(leftX, rightX), y, GATE_FILL);
        }

        drawQuadratic(renderer,
                      SDL_Point{left, top},
                      SDL_Point{right - 28, top - 1},
                      SDL_Point{right, centerY},
                      GATE_OUTLINE, 2);
        drawQuadratic(renderer,
                      SDL_Point{right, centerY},
                      SDL_Point{right - 28, bottom + 1},
                      SDL_Point{left, bottom},
                      GATE_OUTLINE, 2);
        drawQuadratic(renderer,
                      SDL_Point{left, top},
                      SDL_Point{left + 48, centerY},
                      SDL_Point{left, bottom},
                      GATE_OUTLINE, 2);

        if (gateIsXorFamily(type_)) {
            drawQuadratic(renderer,
                          SDL_Point{left - 9, top},
                          SDL_Point{left + 39, centerY},
                          SDL_Point{left - 9, bottom},
                          GATE_OUTLINE, 2);
        }

        if (gateHasBubble(type_)) {
            drawFilledCircle(renderer, right + 7, centerY, 7, GATE_FILL);
            drawCircle(renderer, right + 7, centerY, 7, GATE_OUTLINE);
        }
    }

    GateType type_;
    SDL_Rect bounds_;
    LogicState output_;
    double delay_;
    std::uint64_t generation_;
    bool selected_;
    SimulationLog& log_;
};

// -----------------------------------------------------------------------------
// UI button
// -----------------------------------------------------------------------------

class UiButton {
public:
    UiButton(const std::string& label,
             const SDL_Rect& bounds,
             const Color& accent)
        : label_(label), bounds_(bounds), accent_(accent) {}

    bool hit(int x, int y) const { return contains(bounds_, x, y); }

    void draw(SDL_Renderer* renderer, bool active, bool enabled) const {
        Color fill = enabled ? PANEL_LIGHT : PANEL;
        Color border = enabled ? accent_ : DEFAULT_WIRE;
        if (active && enabled) fill = accent_;
        fillRect(renderer, bounds_, fill);
        drawRect(renderer, bounds_, border);
        drawCenteredText(renderer,
                         label_, bounds_, 2,
                         active && enabled ? BLACK : (enabled ? WHITE : MUTED));
    }

private:
    std::string label_;
    SDL_Rect bounds_;
    Color accent_;
};

// -----------------------------------------------------------------------------
// Circuit canvas with all gates
// -----------------------------------------------------------------------------

class LogicGateCircuit {
public:
    explicit LogicGateCircuit(SimulationLog& log)
        : log_(log),
          inputA_("A", SDL_Rect{35, 155, 145, 125}),
          inputB_("B", SDL_Rect{35, 320, 145, 125}),
          gates_(),
          selectedIndex_(2),
          started_(false) {
        gates_.push_back(LogicGate(GateType::Buffer, SDL_Rect{315, 145, 150, 76}, 0.10, log_));
        gates_.push_back(LogicGate(GateType::Not,    SDL_Rect{735, 145, 150, 76}, 0.10, log_));
        gates_.push_back(LogicGate(GateType::And,    SDL_Rect{315, 310, 150, 76}, 0.20, log_));
        gates_.push_back(LogicGate(GateType::Nand,   SDL_Rect{735, 310, 150, 76}, 0.20, log_));
        gates_.push_back(LogicGate(GateType::Or,     SDL_Rect{315, 475, 150, 76}, 0.18, log_));
        gates_.push_back(LogicGate(GateType::Nor,    SDL_Rect{735, 475, 150, 76}, 0.18, log_));
        gates_.push_back(LogicGate(GateType::Xor,    SDL_Rect{315, 640, 150, 76}, 0.25, log_));
        gates_.push_back(LogicGate(GateType::Xnor,   SDL_Rect{735, 640, 150, 76}, 0.25, log_));
        gates_[selectedIndex_].setSelected(true);
    }

    const InputSwitch& inputA() const { return inputA_; }
    const InputSwitch& inputB() const { return inputB_; }
    const LogicGate& selectedGate() const { return gates_[selectedIndex_]; }
    bool started() const { return started_; }

    void start(SimulationController& controller) {
        reset(false);
        started_ = true;
        log_.add(controller.time(), "SIMULATION STARTED");
        requestAll(controller);
    }

    void reset(bool resetSelection = false) {
        started_ = false;
        inputA_.reset();
        inputB_.reset();
        for (std::size_t i = 0; i < gates_.size(); ++i) gates_[i].reset();
        if (resetSelection) selectedIndex_ = 2;
        for (std::size_t i = 0; i < gates_.size(); ++i) {
            gates_[i].setSelected(i == selectedIndex_);
        }
    }

    bool hitInputA(int x, int y) const { return contains(inputA_.bounds(), x, y); }
    bool hitInputB(int x, int y) const { return contains(inputB_.bounds(), x, y); }

    void toggleA(SimulationController& controller) {
        if (!started_) return;
        inputA_.toggleBinary();
        log_.add(controller.time(), "INPUT A = " + logicShort(inputA_.state()));
        requestAll(controller);
    }

    void toggleB(SimulationController& controller) {
        if (!started_) return;
        inputB_.toggleBinary();
        log_.add(controller.time(), "INPUT B = " + logicShort(inputB_.state()));
        requestAll(controller);
    }

    void cycleA(SimulationController& controller) {
        if (!started_) return;
        inputA_.cycle();
        log_.add(controller.time(), "INPUT A = " + logicShort(inputA_.state()));
        requestAll(controller);
    }

    void cycleB(SimulationController& controller) {
        if (!started_) return;
        inputB_.cycle();
        log_.add(controller.time(), "INPUT B = " + logicShort(inputB_.state()));
        requestAll(controller);
    }

    void selectGateAt(int x, int y) {
        for (std::size_t i = 0; i < gates_.size(); ++i) {
            SDL_Rect hit = gates_[i].bounds();
            hit.x -= 15;
            hit.y -= 30;
            hit.w += 45;
            hit.h += 65;
            if (contains(hit, x, y)) {
                selectedIndex_ = i;
                for (std::size_t j = 0; j < gates_.size(); ++j) {
                    gates_[j].setSelected(j == selectedIndex_);
                }
                return;
            }
        }
    }

    void draw(SDL_Renderer* renderer,
              SimulationMode mode,
              double time) const {
        const bool stopped = mode == SimulationMode::Stopped;
        drawGrid(renderer);
        drawInputBuses(renderer, stopped, mode, time);
        drawGateOutputWires(renderer, stopped, mode, time);

        inputA_.draw(renderer, stopped);
        inputB_.draw(renderer, stopped);
        for (std::size_t i = 0; i < gates_.size(); ++i) gates_[i].draw(renderer, stopped);
        drawOutputLeds(renderer, stopped);
    }

private:
    void requestAll(SimulationController& controller) {
        for (std::size_t i = 0; i < gates_.size(); ++i) {
            gates_[i].request(inputA_.state(), inputB_.state(), controller);
        }
    }

    void drawGrid(SDL_Renderer* renderer) const {
        for (int x = 10; x < 1125; x += 20) {
            drawLine(renderer, x, 105, x, 825, GRID);
        }
        for (int y = 105; y < 825; y += 20) {
            drawLine(renderer, 10, y, 1125, y, GRID);
        }
    }

    void drawOrthogonalWire(SDL_Renderer* renderer,
                            const std::vector<SDL_Point>& points,
                            LogicState state,
                            bool stopped,
                            bool animate,
                            double timeOffset) const {
        const Color color = logicColor(state, stopped);
        drawPolyline(renderer, points, color, 5);
        if (animate && !stopped && points.size() >= 2u) {
            const double phase = std::fmod(timeOffset * 2.0, 1.0);
            const SDL_Point p0 = points[0];
            const SDL_Point p1 = points[1];
            const int x = p0.x + static_cast<int>((p1.x - p0.x) * phase);
            const int y = p0.y + static_cast<int>((p1.y - p0.y) * phase);
            drawFilledCircle(renderer, x, y, 4, WHITE);
        }
    }

    void drawInputBuses(SDL_Renderer* renderer,
                        bool stopped,
                        SimulationMode mode,
                        double time) const {
        const bool animate = mode == SimulationMode::Running;
        const int busAX = 220;
        const int busBX = 250;
        const int topY = 175;
        const int bottomY = 695;

        drawOrthogonalWire(renderer,
                           std::vector<SDL_Point>{{180,217},{busAX,217},{busAX,topY}},
                           inputA_.state(), stopped, animate, time);
        drawOrthogonalWire(renderer,
                           std::vector<SDL_Point>{{180,382},{busBX,382},{busBX,topY}},
                           inputB_.state(), stopped, animate, time + 0.23);
        drawThickLine(renderer, busAX, topY, busAX, bottomY,
                      logicColor(inputA_.state(), stopped), 5);
        drawThickLine(renderer, busBX, topY, busBX, bottomY,
                      logicColor(inputB_.state(), stopped), 5);

        drawText(renderer, "A BUS", 195, 118, 1, logicColor(inputA_.state(), stopped));
        drawText(renderer, "B BUS", 238, 118, 1, logicColor(inputB_.state(), stopped));

        for (std::size_t i = 0; i < gates_.size(); ++i) {
            const LogicGate& gate = gates_[i];
            const SDL_Point pinA = gate.inputA();
            drawOrthogonalWire(renderer,
                               std::vector<SDL_Point>{{busAX, pinA.y}, {pinA.x, pinA.y}},
                               inputA_.state(), stopped, animate, time + 0.10 * i);
            if (!gateIsUnary(gate.type())) {
                const SDL_Point pinB = gate.inputB();
                drawOrthogonalWire(renderer,
                                   std::vector<SDL_Point>{{busBX, pinB.y}, {pinB.x, pinB.y}},
                                   inputB_.state(), stopped, animate, time + 0.10 * i + 0.31);
            }
        }
    }

    void drawGateOutputWires(SDL_Renderer* renderer,
                             bool stopped,
                             SimulationMode mode,
                             double time) const {
        const bool animate = mode == SimulationMode::Running;
        for (std::size_t i = 0; i < gates_.size(); ++i) {
            const SDL_Point output = gates_[i].outputPin();
            const int ledX = output.x + 90;
            drawOrthogonalWire(renderer,
                               std::vector<SDL_Point>{{output.x, output.y}, {ledX - 24, output.y}},
                               gates_[i].output(), stopped, animate, time + 0.17 * i);
        }
    }

    void drawOutputLeds(SDL_Renderer* renderer, bool stopped) const {
        for (std::size_t i = 0; i < gates_.size(); ++i) {
            const SDL_Point output = gates_[i].outputPin();
            const int ledX = output.x + 90;
            const int ledY = output.y;
            const Color color = logicColor(gates_[i].output(), stopped);
            drawFilledCircle(renderer, ledX, ledY, 15, color);
            drawCircle(renderer, ledX, ledY, 17, WHITE);
            drawText(renderer,
                     logicShort(gates_[i].output()),
                     ledX - 3,
                     ledY - 7,
                     2,
                     BLACK);
        }
    }

    SimulationLog& log_;
    InputSwitch inputA_;
    InputSwitch inputB_;
    std::vector<LogicGate> gates_;
    std::size_t selectedIndex_;
    bool started_;
};

// -----------------------------------------------------------------------------
// Main application
// -----------------------------------------------------------------------------

class Application {
public:
    Application()
        : window_(NULL),
          renderer_(NULL),
          keepRunning_(true),
          controller_(),
          log_(),
          circuit_(log_),
          runButton_("RUN", SDL_Rect{20, 23, 100, 44}, GREEN),
          pauseButton_("PAUSE", SDL_Rect{132, 23, 100, 44}, YELLOW),
          stepButton_("STEP", SDL_Rect{244, 23, 100, 44}, CYAN),
          eventButton_("NEXT EVENT", SDL_Rect{356, 23, 155, 44}, PURPLE),
          stopButton_("STOP", SDL_Rect{523, 23, 100, 44}, RED) {}

    ~Application() {
        if (renderer_ != NULL) SDL_DestroyRenderer(renderer_);
        if (window_ != NULL) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    bool initialize() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
            return false;
        }

        window_ = SDL_CreateWindow(
            "Project 8 - All Logic Gates - Proteus Style",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            1500,
            880,
            SDL_WINDOW_SHOWN);
        if (window_ == NULL) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
            return false;
        }

        renderer_ = SDL_CreateRenderer(
            window_, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer_ == NULL) {
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        }
        if (renderer_ == NULL) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
            return false;
        }

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        return true;
    }

    int run() {
        const Uint64 frequency = SDL_GetPerformanceFrequency();
        Uint64 previous = SDL_GetPerformanceCounter();

        while (keepRunning_) {
            const Uint64 current = SDL_GetPerformanceCounter();
            const double delta = static_cast<double>(current - previous) /
                                 static_cast<double>(frequency);
            previous = current;

            processEvents();
            controller_.update(delta);
            render();
        }
        return 0;
    }

private:
    void ensureStarted() {
        if (controller_.mode() == SimulationMode::Stopped) {
            controller_.startPaused();
            log_.clear();
            circuit_.start(controller_);
        }
    }

    void runSimulation() {
        ensureStarted();
        controller_.run();
        log_.add(controller_.time(), "MODE RUN");
    }

    void pauseSimulation() {
        if (controller_.mode() == SimulationMode::Stopped) return;
        controller_.pause();
        log_.add(controller_.time(), "MODE PAUSE");
    }

    void stopSimulation() {
        controller_.stop();
        circuit_.reset();
        log_.clear();
        log_.add(0.0, "MODE STOP - RESET");
    }

    void stepSimulation() {
        ensureStarted();
        controller_.fixedStep();
        log_.add(controller_.time(), "FIXED STEP");
    }

    void nextEventSimulation() {
        ensureStarted();
        controller_.nextEvent();
        log_.add(controller_.time(), "NEXT EVENT");
    }

    bool interactionAllowed() const {
        return controller_.mode() != SimulationMode::Stopped;
    }

    void processEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                keepRunning_ = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                handleMouseDown(event.button.x,
                                event.button.y,
                                event.button.button);
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                handleKeyDown(event.key.keysym.sym);
            }
        }
    }

    void handleMouseDown(int x, int y, Uint8 button) {
        if (runButton_.hit(x, y)) { runSimulation(); return; }
        if (pauseButton_.hit(x, y)) { pauseSimulation(); return; }
        if (stepButton_.hit(x, y)) { stepSimulation(); return; }
        if (eventButton_.hit(x, y)) { nextEventSimulation(); return; }
        if (stopButton_.hit(x, y)) { stopSimulation(); return; }

        if (interactionAllowed()) {
            if (circuit_.hitInputA(x, y)) {
                if (button == SDL_BUTTON_RIGHT) circuit_.cycleA(controller_);
                else circuit_.toggleA(controller_);
                return;
            }
            if (circuit_.hitInputB(x, y)) {
                if (button == SDL_BUTTON_RIGHT) circuit_.cycleB(controller_);
                else circuit_.toggleB(controller_);
                return;
            }
        }
        circuit_.selectGateAt(x, y);
    }

    void handleKeyDown(SDL_Keycode key) {
        switch (key) {
            case SDLK_SPACE:
                if (controller_.mode() == SimulationMode::Running) pauseSimulation();
                else runSimulation();
                break;
            case SDLK_r: stopSimulation(); break;
            case SDLK_s: stepSimulation(); break;
            case SDLK_n: nextEventSimulation(); break;
            case SDLK_a:
                if (interactionAllowed()) circuit_.toggleA(controller_);
                break;
            case SDLK_b:
                if (interactionAllowed()) circuit_.toggleB(controller_);
                break;
            case SDLK_1:
                if (interactionAllowed()) circuit_.cycleA(controller_);
                break;
            case SDLK_2:
                if (interactionAllowed()) circuit_.cycleB(controller_);
                break;
            default: break;
        }
    }

    std::string modeName() const {
        switch (controller_.mode()) {
            case SimulationMode::Running: return "RUNNING";
            case SimulationMode::Paused: return "PAUSED";
            default: return "STOPPED";
        }
    }

    Color modeColor() const {
        switch (controller_.mode()) {
            case SimulationMode::Running: return GREEN;
            case SimulationMode::Paused: return YELLOW;
            default: return MUTED;
        }
    }

    void render() {
        setColor(renderer_, BACKGROUND);
        SDL_RenderClear(renderer_);

        drawToolbar();
        circuit_.draw(renderer_, controller_.mode(), controller_.time());
        drawRightPanel();
        drawBottomHelp();

        SDL_RenderPresent(renderer_);
    }

    void drawToolbar() {
        SDL_Rect toolbar = {0, 0, 1500, 90};
        fillRect(renderer_, toolbar, PANEL);
        drawLine(renderer_, 0, 89, 1500, 89, CYAN);

        const bool stopped = controller_.mode() == SimulationMode::Stopped;
        runButton_.draw(renderer_, controller_.mode() == SimulationMode::Running, true);
        pauseButton_.draw(renderer_, controller_.mode() == SimulationMode::Paused, !stopped);
        stepButton_.draw(renderer_, false, true);
        eventButton_.draw(renderer_, false, true);
        stopButton_.draw(renderer_, false, !stopped);

        drawText(renderer_, "PROJECT 8 - ALL LOGIC GATES", 680, 18, 3, WHITE);
        drawText(renderer_, "PROTEUS STYLE - SDL2 - C++14", 730, 54, 2, MUTED);
    }

    void drawRightPanel() {
        SDL_Rect panel = {1140, 105, 345, 720};
        fillRect(renderer_, panel, PANEL);
        drawRect(renderer_, panel, CYAN);

        drawText(renderer_, "SIMULATION", 1160, 122, 2, WHITE);
        drawText(renderer_, modeName(), 1160, 155, 2, modeColor());
        drawText(renderer_, "TIME " + formatDouble(controller_.time(), 3) + " S",
                 1160, 188, 2, CYAN);
        drawText(renderer_, "EVENTS " + std::to_string(controller_.scheduler().size()),
                 1160, 220, 2, WHITE);
        if (!controller_.scheduler().empty()) {
            drawText(renderer_, "NEXT " + formatDouble(controller_.scheduler().nextEventTime(), 3),
                     1160, 250, 2, PURPLE);
        } else {
            drawText(renderer_, "NEXT NONE", 1160, 250, 2, MUTED);
        }

        const LogicGate& gate = circuit_.selectedGate();
        drawText(renderer_, "SELECTED GATE", 1160, 300, 2, WHITE);
        drawText(renderer_, gateName(gate.type()), 1160, 333, 3, SELECTED);
        drawText(renderer_, "DELAY " + formatDouble(gate.delay(), 2) + " S",
                 1160, 372, 2, MUTED);
        drawText(renderer_, "OUTPUT " + logicShort(gate.output()),
                 1160, 404, 2,
                 logicColor(gate.output(), controller_.mode() == SimulationMode::Stopped));

        drawTruthTable(gate.type(), 1160, 450);

        drawText(renderer_, "WIRE COLORS", 1160, 628, 2, WHITE);
        drawThickLine(renderer_, 1160, 662, 1210, 662, RED, 6);
        drawText(renderer_, "HIGH", 1230, 655, 2, RED);
        drawThickLine(renderer_, 1160, 695, 1210, 695, BLUE, 6);
        drawText(renderer_, "LOW", 1230, 688, 2, BLUE);
        drawThickLine(renderer_, 1160, 728, 1210, 728, YELLOW, 6);
        drawText(renderer_, "FLOAT", 1230, 721, 2, YELLOW);
        drawThickLine(renderer_, 1160, 761, 1210, 761, DEFAULT_WIRE, 6);
        drawText(renderer_, "STOP", 1230, 754, 2, MUTED);

        drawText(renderer_, "EVENT LOG", 1160, 793, 2, WHITE);
        int y = 818;
        const std::vector<std::string>& lines = log_.lines();
        const std::size_t start = lines.size() > 2u ? lines.size() - 2u : 0u;
        for (std::size_t i = start; i < lines.size(); ++i) {
            drawText(renderer_, lines[i], 1160, y, 1, MUTED);
            y += 18;
        }
    }

    void drawTruthTable(GateType type, int x, int y) {
        drawText(renderer_, "TRUTH TABLE", x, y, 2, WHITE);
        if (gateIsUnary(type)) {
            drawText(renderer_, "A  Y", x, y + 30, 2, MUTED);
            for (int a = 0; a <= 1; ++a) {
                const LogicState aState = a ? LogicState::High : LogicState::Low;
                const LogicState result = evaluateGate(type, aState, LogicState::Low);
                drawText(renderer_,
                         std::to_string(a) + "  " + logicShort(result),
                         x, y + 60 + a * 28, 2, WHITE);
            }
        } else {
            drawText(renderer_, "A B  Y", x, y + 30, 2, MUTED);
            int row = 0;
            for (int a = 0; a <= 1; ++a) {
                for (int b = 0; b <= 1; ++b) {
                    const LogicState result = evaluateGate(
                        type,
                        a ? LogicState::High : LogicState::Low,
                        b ? LogicState::High : LogicState::Low);
                    drawText(renderer_,
                             std::to_string(a) + " " + std::to_string(b) + "  " + logicShort(result),
                             x, y + 60 + row * 25, 2, WHITE);
                    ++row;
                }
            }
        }
    }

    void drawBottomHelp() {
        SDL_Rect footer = {0, 835, 1500, 45};
        fillRect(renderer_, footer, PANEL);
        drawLine(renderer_, 0, 835, 1500, 835, CYAN);
        drawText(renderer_,
                 "LEFT CLICK A/B: TOGGLE  RIGHT CLICK: FLOAT  A/B KEYS  1/2 CYCLE  SPACE RUN  S STEP  N NEXT  R STOP",
                 20, 850, 2, MUTED);
    }

    SDL_Window* window_;
    SDL_Renderer* renderer_;
    bool keepRunning_;

    SimulationController controller_;
    SimulationLog log_;
    LogicGateCircuit circuit_;

    UiButton runButton_;
    UiButton pauseButton_;
    UiButton stepButton_;
    UiButton eventButton_;
    UiButton stopButton_;
};

} // namespace project8

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    project8::Application application;
    if (!application.initialize()) return 1;
    return application.run();
}