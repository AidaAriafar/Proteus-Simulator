/*
    Project 9 - Measurement Instruments + D Flip-Flop
    C++14 + SDL2

    Included instruments:
      1) Voltage probe
      2) Digital voltmeter
      3) Digital ammeter
      4) Simple 4-channel oscilloscope

    Digital component added:
      Rising-edge triggered D flip-flop with asynchronous RESET
      and propagation delay.

    Keyboard:
      Space : Run/Pause
      R     : Stop/Reset simulation
      S     : Fixed step (0.05 s)
      N     : Advance to next event
      D     : Toggle D input
      C     : Manual clock pulse
      A     : Toggle automatic clock
      X     : Toggle RESET
      P     : Cycle voltage-probe node
      V     : Cycle voltmeter positive terminal
      M     : Cycle voltmeter negative terminal
      +/-   : Change clock frequency
*/

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace project9 {

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

static const Color BACKGROUND   = {18, 22, 29, 255};
static const Color PANEL        = {30, 37, 48, 255};
static const Color PANEL_LIGHT  = {42, 51, 65, 255};
static const Color GRID         = {52, 62, 77, 255};
static const Color WHITE        = {238, 242, 248, 255};
static const Color MUTED        = {151, 162, 178, 255};
static const Color GREEN        = {72, 215, 121, 255};
static const Color RED          = {243, 76, 91, 255};
static const Color BLUE         = {70, 126, 255, 255};
static const Color YELLOW       = {255, 202, 72, 255};
static const Color CYAN         = {72, 204, 229, 255};
static const Color ORANGE       = {255, 146, 77, 255};
static const Color PURPLE       = {180, 126, 255, 255};
static const Color BLACK        = {8, 11, 16, 255};
static const Color WIRE_OFF     = {105, 117, 133, 255};
static const Color DISPLAY      = {13, 28, 22, 255};
static const Color DISPLAY_TEXT = {114, 255, 152, 255};

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

void drawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2,
              const Color& color) {
    setColor(renderer, color);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void drawThickLine(SDL_Renderer* renderer,
                   int x1, int y1, int x2, int y2,
                   const Color& color, int thickness = 3) {
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
                      int centerX, int centerY, int radius,
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
                int centerX, int centerY, int radius,
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
        case '+': return {{0,4,4,31,4,4,0}};
        case '%': return {{17,2,4,8,17,0,0}};
        case '(': return {{2,4,8,8,8,4,2}};
        case ')': return {{8,4,2,2,2,4,8}};
        case '[': return {{14,8,8,8,8,8,14}};
        case ']': return {{14,2,2,2,2,2,14}};
        case '_': return {{0,0,0,0,0,0,31}};
        case '>': return {{16,8,4,2,4,8,16}};
        case '<': return {{1,2,4,8,4,2,1}};
        case '?': return {{14,17,1,2,4,0,4}};
        case ' ': return {{0,0,0,0,0,0,0}};
        default:  return {{0,0,0,0,0,0,0}};
    }
}

void drawCharacter(SDL_Renderer* renderer,
                   char character,
                   int x, int y,
                   int scale,
                   const Color& color) {
    const Glyph glyph = glyphFor(character);
    setColor(renderer, color);
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            if ((glyph[static_cast<std::size_t>(row)] >> (4 - column)) & 1u) {
                SDL_Rect pixel = {x + column * scale, y + row * scale,
                                  scale, scale};
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
    }
}

int textWidth(const std::string& text, int scale) {
    if (text.empty()) return 0;
    return static_cast<int>(text.size()) * 6 * scale - scale;
}

void drawText(SDL_Renderer* renderer,
              const std::string& text,
              int x, int y,
              int scale,
              const Color& color) {
    int cursor = x;
    for (std::size_t i = 0; i < text.size(); ++i) {
        drawCharacter(renderer, text[i], cursor, y, scale, color);
        cursor += 6 * scale;
    }
}

void drawCenteredText(SDL_Renderer* renderer,
                      const std::string& text,
                      const SDL_Rect& rect,
                      int scale,
                      const Color& color) {
    const int x = rect.x + (rect.w - textWidth(text, scale)) / 2;
    const int y = rect.y + (rect.h - 7 * scale) / 2;
    drawText(renderer, text, x, y, scale, color);
}

std::string fixedNumber(double value, int precision) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

enum class LogicState {
    Low,
    High,
    Floating
};

std::string logicName(LogicState state) {
    if (state == LogicState::Low) return "LOW";
    if (state == LogicState::High) return "HIGH";
    return "FLOAT";
}

Color logicColor(LogicState state, bool stopped = false) {
    if (stopped) return WIRE_OFF;
    if (state == LogicState::Low) return BLUE;
    if (state == LogicState::High) return RED;
    return YELLOW;
}

double logicVoltage(LogicState state) {
    if (state == LogicState::Low) return 0.0;
    if (state == LogicState::High) return 5.0;
    return std::numeric_limits<double>::quiet_NaN();
}

LogicState inverted(LogicState state) {
    if (state == LogicState::Low) return LogicState::High;
    if (state == LogicState::High) return LogicState::Low;
    return LogicState::Floating;
}

class Scheduler {
public:
    struct Event {
        double time;
        std::uint64_t serial;
        std::function<void()> callback;
    };

    struct Compare {
        bool operator()(const Event& left, const Event& right) const {
            if (left.time != right.time) return left.time > right.time;
            return left.serial > right.serial;
        }
    };

    void schedule(double time, const std::function<void()>& callback) {
        Event event;
        event.time = time;
        event.serial = serial_++;
        event.callback = callback;
        events_.push(event);
    }

    void runUntil(double targetTime) {
        while (!events_.empty() && events_.top().time <= targetTime + 1e-9) {
            const Event event = events_.top();
            events_.pop();
            if (event.callback) event.callback();
        }
    }

    bool empty() const { return events_.empty(); }

    double nextTime() const {
        if (events_.empty()) return std::numeric_limits<double>::infinity();
        return events_.top().time;
    }

    std::size_t size() const { return events_.size(); }

    void clear() {
        while (!events_.empty()) events_.pop();
        serial_ = 0;
    }

private:
    std::priority_queue<Event, std::vector<Event>, Compare> events_;
    std::uint64_t serial_ = 0;
};

class DFlipFlop {
public:
    typedef std::function<void(LogicState, LogicState)> OutputCallback;

    explicit DFlipFlop(Scheduler& scheduler)
        : scheduler_(scheduler) {}

    void setCallback(const OutputCallback& callback) {
        callback_ = callback;
    }

    void setPropagationDelay(double seconds) {
        delay_ = clampValue(seconds, 0.0, 1.0);
    }

    double propagationDelay() const { return delay_; }

    void setD(LogicState state) {
        d_ = state;
    }

    void setClock(LogicState state, double time) {
        const bool rising = (clock_ != LogicState::High &&
                             state == LogicState::High);
        clock_ = state;
        if (reset_ == LogicState::High) return;
        if (rising) scheduleCapture(d_, time + delay_);
    }

    void setReset(LogicState state, double time) {
        reset_ = state;
        ++generation_;
        if (reset_ == LogicState::High) {
            const std::uint64_t generation = generation_;
            scheduler_.schedule(time + 0.03, [this, generation]() {
                if (generation != generation_) return;
                applyOutputs(LogicState::Low, LogicState::High);
            });
        }
    }

    LogicState q() const { return q_; }
    LogicState qBar() const { return qBar_; }

    void resetAll() {
        ++generation_;
        d_ = LogicState::Low;
        clock_ = LogicState::Low;
        reset_ = LogicState::Low;
        applyOutputs(LogicState::Low, LogicState::High);
    }

private:
    void scheduleCapture(LogicState capturedD, double eventTime) {
        const std::uint64_t generation = ++generation_;
        scheduler_.schedule(eventTime, [this, capturedD, generation]() {
            if (generation != generation_) return;
            if (reset_ == LogicState::High) return;
            applyOutputs(capturedD, inverted(capturedD));
        });
    }

    void applyOutputs(LogicState newQ, LogicState newQBar) {
        q_ = newQ;
        qBar_ = newQBar;
        if (callback_) callback_(q_, qBar_);
    }

    Scheduler& scheduler_;
    OutputCallback callback_;
    LogicState d_ = LogicState::Low;
    LogicState clock_ = LogicState::Low;
    LogicState reset_ = LogicState::Low;
    LogicState q_ = LogicState::Low;
    LogicState qBar_ = LogicState::High;
    double delay_ = 0.12;
    std::uint64_t generation_ = 0;
};

enum class NodeId {
    Ground,
    D,
    Clock,
    Reset,
    Q,
    QBar,
    Load
};

std::string nodeName(NodeId node) {
    switch (node) {
        case NodeId::Ground: return "GND";
        case NodeId::D:      return "D";
        case NodeId::Clock:  return "CLK";
        case NodeId::Reset:  return "RESET";
        case NodeId::Q:      return "Q";
        case NodeId::QBar:   return "QBAR";
        case NodeId::Load:   return "LOAD";
    }
    return "?";
}

NodeId nextNode(NodeId node) {
    const int value = (static_cast<int>(node) + 1) % 7;
    return static_cast<NodeId>(value);
}

struct Sample {
    double time;
    LogicState d;
    LogicState clock;
    LogicState q;
    LogicState qBar;
};

enum class SimulationMode {
    Stopped,
    Running,
    Paused
};

class Application {
public:
    Application()
        : flipFlop_(scheduler_) {
        flipFlop_.setCallback([this](LogicState q, LogicState qBar) {
            q_ = q;
            qBar_ = qBar;
            appendLog("DFF OUTPUT Q=" + logicName(q));
        });
    }

    ~Application() {
        shutdown();
    }

    bool initialize() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
            return false;
        }

        window_ = SDL_CreateWindow(
            "Project 9 - Measurement Instruments and D Flip-Flop",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            SDL_WINDOW_SHOWN);

        if (window_ == NULL) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
            SDL_Quit();
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
            SDL_DestroyWindow(window_);
            window_ = NULL;
            SDL_Quit();
            return false;
        }

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        resetSimulation();
        lastTicks_ = SDL_GetTicks();
        return true;
    }

    int run() {
        bool quit = false;
        while (!quit) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    quit = true;
                } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    handleKey(event.key.keysym.sym);
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    handleMouse(event.button.x, event.button.y, event.button.button);
                }
            }

            const Uint32 now = SDL_GetTicks();
            double realDelta = static_cast<double>(now - lastTicks_) / 1000.0;
            lastTicks_ = now;
            realDelta = clampValue(realDelta, 0.0, 0.05);

            if (mode_ == SimulationMode::Running) {
                accumulator_ += realDelta * simulationSpeed_;
                while (accumulator_ >= 0.01) {
                    advanceTo(simulationTime_ + 0.01);
                    accumulator_ -= 0.01;
                }
            }

            render();
        }
        return 0;
    }

private:
    static const int WINDOW_WIDTH = 1440;
    static const int WINDOW_HEIGHT = 900;

    SDL_Window* window_ = NULL;
    SDL_Renderer* renderer_ = NULL;
    Scheduler scheduler_;
    DFlipFlop flipFlop_;

    SimulationMode mode_ = SimulationMode::Stopped;
    double simulationTime_ = 0.0;
    double accumulator_ = 0.0;
    double simulationSpeed_ = 1.0;
    Uint32 lastTicks_ = 0;

    LogicState d_ = LogicState::Low;
    LogicState clock_ = LogicState::Low;
    LogicState reset_ = LogicState::Low;
    LogicState q_ = LogicState::Low;
    LogicState qBar_ = LogicState::High;

    bool automaticClock_ = true;
    double clockFrequency_ = 1.0;
    double nextClockToggle_ = 0.5;

    NodeId probeNode_ = NodeId::Q;
    NodeId voltPositive_ = NodeId::Q;
    NodeId voltNegative_ = NodeId::Ground;
    double loadResistanceOhms_ = 330.0;
    double ledForwardVoltage_ = 2.0;

    std::deque<Sample> samples_;
    double nextSampleTime_ = 0.0;
    std::deque<std::string> logLines_;

    const SDL_Rect runButton_       = {24, 20, 108, 42};
    const SDL_Rect pauseButton_     = {142, 20, 108, 42};
    const SDL_Rect stopButton_      = {260, 20, 108, 42};
    const SDL_Rect stepButton_      = {378, 20, 108, 42};
    const SDL_Rect nextButton_      = {496, 20, 138, 42};

    const SDL_Rect dButton_         = {55, 135, 130, 58};
    const SDL_Rect autoClockButton_ = {55, 230, 130, 44};
    const SDL_Rect pulseButton_     = {55, 285, 130, 44};
    const SDL_Rect resetButton_     = {55, 370, 130, 52};
    const SDL_Rect frequencyMinus_  = {55, 465, 45, 36};
    const SDL_Rect frequencyPlus_   = {140, 465, 45, 36};

    const SDL_Rect probePanel_      = {940, 90, 465, 150};
    const SDL_Rect voltPanel_       = {940, 255, 465, 170};
    const SDL_Rect ampPanel_        = {940, 440, 465, 170};
    const SDL_Rect probeSelect_     = {969, 184, 160, 36};
    const SDL_Rect voltPlusSelect_  = {969, 361, 160, 36};
    const SDL_Rect voltMinusSelect_ = {1165, 361, 160, 36};
    const SDL_Rect resistanceMinus_ = {969, 556, 48, 34};
    const SDL_Rect resistancePlus_  = {1280, 556, 48, 34};

    const SDL_Rect scopePanel_      = {24, 630, 1381, 245};

    void shutdown() {
        if (renderer_ != NULL) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = NULL;
        }
        if (window_ != NULL) {
            SDL_DestroyWindow(window_);
            window_ = NULL;
        }
        SDL_Quit();
    }

    void appendLog(const std::string& text) {
        std::ostringstream stream;
        stream << "T=" << std::fixed << std::setprecision(2)
               << simulationTime_ << " " << text;
        logLines_.push_front(stream.str());
        while (logLines_.size() > 6u) logLines_.pop_back();
    }

    void resetSimulation() {
        scheduler_.clear();
        simulationTime_ = 0.0;
        accumulator_ = 0.0;
        d_ = LogicState::Low;
        clock_ = LogicState::Low;
        reset_ = LogicState::Low;
        q_ = LogicState::Low;
        qBar_ = LogicState::High;
        nextClockToggle_ = 0.5 / clockFrequency_;
        nextSampleTime_ = 0.0;
        samples_.clear();
        logLines_.clear();
        flipFlop_.resetAll();
        flipFlop_.setD(d_);
        flipFlop_.setReset(reset_, simulationTime_);
        flipFlop_.setClock(clock_, simulationTime_);
        recordSample(0.0);
        appendLog("SIMULATION RESET");
    }

    void setD(LogicState state) {
        d_ = state;
        flipFlop_.setD(d_);
        appendLog("D=" + logicName(d_));
    }

    void setClock(LogicState state) {
        clock_ = state;
        flipFlop_.setClock(clock_, simulationTime_);
        appendLog("CLK=" + logicName(clock_));
    }

    void setReset(LogicState state) {
        reset_ = state;
        flipFlop_.setReset(reset_, simulationTime_);
        appendLog("RESET=" + logicName(reset_));
    }

    void pulseClock() {
        automaticClock_ = false;
        setClock(LogicState::High);
        const double pulseEnd = simulationTime_ + 0.18;
        scheduler_.schedule(pulseEnd, [this]() {
            setClock(LogicState::Low);
        });
        if (mode_ == SimulationMode::Stopped) mode_ = SimulationMode::Paused;
    }

    void toggleAutomaticClock() {
        automaticClock_ = !automaticClock_;
        if (automaticClock_) {
            nextClockToggle_ = simulationTime_ + 0.5 / clockFrequency_;
            appendLog("AUTO CLOCK ON");
        } else {
            appendLog("AUTO CLOCK OFF");
        }
    }

    void changeFrequency(double amount) {
        clockFrequency_ = clampValue(clockFrequency_ + amount, 0.25, 5.0);
        if (automaticClock_) {
            nextClockToggle_ = simulationTime_ + 0.5 / clockFrequency_;
        }
        appendLog("FREQ=" + fixedNumber(clockFrequency_, 2) + "HZ");
    }

    void advanceTo(double targetTime) {
        targetTime = std::max(targetTime, simulationTime_);

        while (simulationTime_ < targetTime - 1e-9) {
            double nextBoundary = targetTime;
            if (automaticClock_) {
                nextBoundary = std::min(nextBoundary, nextClockToggle_);
            }
            nextBoundary = std::min(nextBoundary, scheduler_.nextTime());

            if (!std::isfinite(nextBoundary)) nextBoundary = targetTime;
            if (nextBoundary < simulationTime_) nextBoundary = simulationTime_;

            simulationTime_ = nextBoundary;
            scheduler_.runUntil(simulationTime_);

            if (automaticClock_ &&
                simulationTime_ >= nextClockToggle_ - 1e-9) {
                const LogicState nextState =
                    (clock_ == LogicState::High) ? LogicState::Low
                                                 : LogicState::High;
                setClock(nextState);
                nextClockToggle_ += 0.5 / clockFrequency_;
            }

            scheduler_.runUntil(simulationTime_);
            sampleUntil(simulationTime_);

            if (nextBoundary >= targetTime - 1e-9) break;
        }

        scheduler_.runUntil(targetTime);
        simulationTime_ = targetTime;
        sampleUntil(simulationTime_);
    }

    void advanceNextEvent() {
        double target = scheduler_.nextTime();
        if (automaticClock_) target = std::min(target, nextClockToggle_);
        if (!std::isfinite(target) || target <= simulationTime_ + 1e-9) {
            target = simulationTime_ + 0.05;
        }
        if (mode_ == SimulationMode::Stopped) mode_ = SimulationMode::Paused;
        advanceTo(target);
    }

    void recordSample(double time) {
        Sample sample;
        sample.time = time;
        sample.d = d_;
        sample.clock = clock_;
        sample.q = q_;
        sample.qBar = qBar_;
        samples_.push_back(sample);
        const double keepFrom = std::max(0.0, simulationTime_ - 12.0);
        while (!samples_.empty() && samples_.front().time < keepFrom) {
            samples_.pop_front();
        }
    }

    void sampleUntil(double time) {
        const double interval = 0.02;
        while (nextSampleTime_ <= time + 1e-9) {
            recordSample(nextSampleTime_);
            nextSampleTime_ += interval;
        }
    }

    double nodeVoltage(NodeId node) const {
        switch (node) {
            case NodeId::Ground: return 0.0;
            case NodeId::D:      return logicVoltage(d_);
            case NodeId::Clock:  return logicVoltage(clock_);
            case NodeId::Reset:  return logicVoltage(reset_);
            case NodeId::Q:      return logicVoltage(q_);
            case NodeId::QBar:   return logicVoltage(qBar_);
            case NodeId::Load: {
                const double qVoltage = logicVoltage(q_);
                if (!std::isfinite(qVoltage)) {
                    return std::numeric_limits<double>::quiet_NaN();
                }
                if (qVoltage <= ledForwardVoltage_) return 0.0;
                return ledForwardVoltage_;
            }
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    double loadCurrentMilliAmps() const {
        const double qVoltage = logicVoltage(q_);
        if (!std::isfinite(qVoltage)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (qVoltage <= ledForwardVoltage_) return 0.0;
        return (qVoltage - ledForwardVoltage_) /
               loadResistanceOhms_ * 1000.0;
    }

    std::string voltageText(double voltage) const {
        if (!std::isfinite(voltage)) return "--- V";
        return fixedNumber(voltage, 3) + " V";
    }

    std::string currentText(double current) const {
        if (!std::isfinite(current)) return "--- MA";
        return fixedNumber(current, 2) + " MA";
    }

    void handleKey(SDL_Keycode key) {
        switch (key) {
            case SDLK_SPACE:
                if (mode_ == SimulationMode::Running) {
                    mode_ = SimulationMode::Paused;
                    appendLog("PAUSE");
                } else {
                    mode_ = SimulationMode::Running;
                    appendLog("RUN");
                }
                break;
            case SDLK_r:
                mode_ = SimulationMode::Stopped;
                resetSimulation();
                break;
            case SDLK_s:
                if (mode_ == SimulationMode::Stopped) mode_ = SimulationMode::Paused;
                advanceTo(simulationTime_ + 0.05);
                break;
            case SDLK_n:
                advanceNextEvent();
                break;
            case SDLK_d:
                setD(d_ == LogicState::High ? LogicState::Low : LogicState::High);
                break;
            case SDLK_c:
                pulseClock();
                break;
            case SDLK_a:
                toggleAutomaticClock();
                break;
            case SDLK_x:
                setReset(reset_ == LogicState::High ? LogicState::Low
                                                    : LogicState::High);
                break;
            case SDLK_p:
                probeNode_ = nextNode(probeNode_);
                break;
            case SDLK_v:
                voltPositive_ = nextNode(voltPositive_);
                break;
            case SDLK_m:
                voltNegative_ = nextNode(voltNegative_);
                break;
            case SDLK_PLUS:
            case SDLK_EQUALS:
            case SDLK_KP_PLUS:
                changeFrequency(0.25);
                break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
                changeFrequency(-0.25);
                break;
            default:
                break;
        }
    }

    void handleMouse(int x, int y, Uint8 button) {
        if (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT) return;

        if (contains(dButton_, x, y) && button == SDL_BUTTON_RIGHT) {
            LogicState next = LogicState::Low;
            if (d_ == LogicState::Low) next = LogicState::High;
            else if (d_ == LogicState::High) next = LogicState::Floating;
            setD(next);
            return;
        }

        if (button != SDL_BUTTON_LEFT) return;

        if (contains(runButton_, x, y)) {
            mode_ = SimulationMode::Running;
            appendLog("RUN");
        } else if (contains(pauseButton_, x, y)) {
            mode_ = SimulationMode::Paused;
            appendLog("PAUSE");
        } else if (contains(stopButton_, x, y)) {
            mode_ = SimulationMode::Stopped;
            resetSimulation();
        } else if (contains(stepButton_, x, y)) {
            if (mode_ == SimulationMode::Stopped) mode_ = SimulationMode::Paused;
            advanceTo(simulationTime_ + 0.05);
        } else if (contains(nextButton_, x, y)) {
            advanceNextEvent();
        } else if (contains(dButton_, x, y)) {
            setD(d_ == LogicState::High ? LogicState::Low : LogicState::High);
        } else if (contains(autoClockButton_, x, y)) {
            toggleAutomaticClock();
        } else if (contains(pulseButton_, x, y)) {
            pulseClock();
        } else if (contains(resetButton_, x, y)) {
            setReset(reset_ == LogicState::High ? LogicState::Low
                                                : LogicState::High);
        } else if (contains(frequencyMinus_, x, y)) {
            changeFrequency(-0.25);
        } else if (contains(frequencyPlus_, x, y)) {
            changeFrequency(0.25);
        } else if (contains(probeSelect_, x, y)) {
            probeNode_ = nextNode(probeNode_);
        } else if (contains(voltPlusSelect_, x, y)) {
            voltPositive_ = nextNode(voltPositive_);
        } else if (contains(voltMinusSelect_, x, y)) {
            voltNegative_ = nextNode(voltNegative_);
        } else if (contains(resistanceMinus_, x, y)) {
            loadResistanceOhms_ = clampValue(loadResistanceOhms_ - 50.0,
                                              100.0, 2000.0);
        } else if (contains(resistancePlus_, x, y)) {
            loadResistanceOhms_ = clampValue(loadResistanceOhms_ + 50.0,
                                              100.0, 2000.0);
        }
    }

    void drawButton(const SDL_Rect& rect,
                    const std::string& text,
                    bool active,
                    const Color& accent) {
        fillRect(renderer_, rect, active ? accent : PANEL_LIGHT);
        drawRect(renderer_, rect, active ? WHITE : MUTED);
        drawCenteredText(renderer_, text, rect, 2,
                         active ? BLACK : WHITE);
    }

    void renderTopBar() {
        SDL_Rect top = {0, 0, WINDOW_WIDTH, 78};
        fillRect(renderer_, top, PANEL);
        drawButton(runButton_, "RUN", mode_ == SimulationMode::Running, GREEN);
        drawButton(pauseButton_, "PAUSE", mode_ == SimulationMode::Paused, YELLOW);
        drawButton(stopButton_, "STOP", mode_ == SimulationMode::Stopped, RED);
        drawButton(stepButton_, "STEP", false, CYAN);
        drawButton(nextButton_, "NEXT EVENT", false, PURPLE);

        drawText(renderer_, "PROJECT 9: D FLIP-FLOP + MEASUREMENT TOOLS",
                 670, 19, 2, WHITE);
        drawText(renderer_, "TIME " + fixedNumber(simulationTime_, 2) + " S",
                 670, 47, 2, CYAN);
        drawText(renderer_, "EVENTS " + std::to_string(scheduler_.size()),
                 1000, 47, 2, MUTED);
    }

    void renderInputPanel() {
        SDL_Rect panel = {24, 90, 195, 520};
        fillRect(renderer_, panel, PANEL);
        drawRect(renderer_, panel, GRID);
        drawText(renderer_, "INPUTS", 70, 105, 2, WHITE);

        drawText(renderer_, "DATA D", 75, 126, 2, MUTED);
        fillRect(renderer_, dButton_, logicColor(d_));
        drawRect(renderer_, dButton_, WHITE);
        std::string dLabel = "LOW 0V";
        if (d_ == LogicState::High) dLabel = "HIGH 5V";
        else if (d_ == LogicState::Floating) dLabel = "FLOAT";
        drawCenteredText(renderer_, dLabel, dButton_, 2, WHITE);

        drawText(renderer_, "CLOCK", 75, 211, 2, MUTED);
        drawButton(autoClockButton_,
                   automaticClock_ ? "AUTO ON" : "AUTO OFF",
                   automaticClock_, GREEN);
        drawButton(pulseButton_, "PULSE CLK", false, CYAN);

        drawText(renderer_, "ASYNC RESET", 53, 350, 2, MUTED);
        fillRect(renderer_, resetButton_,
                 reset_ == LogicState::High ? ORANGE : PANEL_LIGHT);
        drawRect(renderer_, resetButton_, WHITE);
        drawCenteredText(renderer_,
                         reset_ == LogicState::High ? "RESET ON" : "RESET OFF",
                         resetButton_, 2, WHITE);

        drawText(renderer_, "FREQUENCY", 61, 444, 2, MUTED);
        drawButton(frequencyMinus_, "-", false, BLUE);
        drawButton(frequencyPlus_, "+", false, RED);
        drawText(renderer_, fixedNumber(clockFrequency_, 2) + " HZ",
                 70, 513, 2, CYAN);

        drawText(renderer_, "DELAY " + fixedNumber(flipFlop_.propagationDelay(), 2) + " S",
                 46, 550, 2, MUTED);
        drawText(renderer_, "D/C/A/X KEYS", 48, 582, 1, MUTED);
    }

    void drawWire(int x1, int y1, int x2, int y2, LogicState state) {
        drawThickLine(renderer_, x1, y1, x2, y2,
                      logicColor(state, mode_ == SimulationMode::Stopped), 4);
    }

    void renderCircuit() {
        SDL_Rect panel = {235, 90, 685, 520};
        fillRect(renderer_, panel, PANEL);
        drawRect(renderer_, panel, GRID);
        drawText(renderer_, "RISING EDGE D FLIP-FLOP", 390, 108, 2, WHITE);

        const SDL_Rect body = {475, 205, 230, 230};
        fillRect(renderer_, body, {228, 232, 238, 255});
        drawRect(renderer_, body, WHITE);
        drawText(renderer_, "D", 505, 250, 4, BLACK);
        drawText(renderer_, "Q", 650, 250, 4, BLACK);
        drawText(renderer_, "QBAR", 610, 355, 2, BLACK);
        drawText(renderer_, "DFF", 555, 302, 3, BLACK);

        drawLine(renderer_, body.x, 345, body.x + 20, 355, BLACK);
        drawLine(renderer_, body.x + 20, 355, body.x, 365, BLACK);
        drawText(renderer_, "CLK", 500, 380, 2, BLACK);
        drawText(renderer_, "R", 510, 410, 2, BLACK);

        drawWire(285, 270, body.x, 270, d_);
        drawText(renderer_, "D", 275, 250, 2, WHITE);
        drawFilledCircle(renderer_, 285, 270, 7, logicColor(d_));

        drawWire(285, 355, body.x, 355, clock_);
        drawText(renderer_, "CLK", 258, 335, 2, WHITE);
        drawFilledCircle(renderer_, 285, 355, 7, logicColor(clock_));

        drawWire(285, 420, body.x, 420, reset_);
        drawText(renderer_, "RESET", 245, 400, 2, WHITE);
        drawFilledCircle(renderer_, 285, 420, 7, logicColor(reset_));

        drawWire(body.x + body.w, 270, 870, 270, q_);
        drawWire(body.x + body.w, 380, 870, 380, qBar_);
        drawFilledCircle(renderer_, 870, 270, 9,
                         q_ == LogicState::High ? RED :
                         (q_ == LogicState::Low ? BLUE : YELLOW));
        drawFilledCircle(renderer_, 870, 380, 9,
                         qBar_ == LogicState::High ? RED :
                         (qBar_ == LogicState::Low ? BLUE : YELLOW));
        drawText(renderer_, "Q " + logicName(q_), 745, 245, 2, WHITE);
        drawText(renderer_, "QBAR " + logicName(qBar_), 720, 355, 2, WHITE);

        drawText(renderer_, "LOAD: Q -> R -> LED -> GND", 380, 478, 2, MUTED);
        drawWire(430, 520, 535, 520, q_);
        SDL_Rect resistor = {535, 505, 105, 30};
        fillRect(renderer_, resistor, {211, 185, 125, 255});
        drawRect(renderer_, resistor, WHITE);
        drawCenteredText(renderer_, fixedNumber(loadResistanceOhms_, 0) + " OHM",
                         resistor, 1, BLACK);
        drawWire(640, 520, 720, 520, q_);
        drawCircle(renderer_, 750, 520, 24, WHITE);
        drawText(renderer_, "LED", 732, 512, 1,
                 loadCurrentMilliAmps() > 0.01 ? YELLOW : MUTED);
        drawLine(renderer_, 774, 520, 820, 520, WIRE_OFF);
        drawLine(renderer_, 820, 520, 820, 545, WIRE_OFF);
        drawLine(renderer_, 800, 545, 840, 545, WHITE);
        drawLine(renderer_, 807, 552, 833, 552, WHITE);
        drawLine(renderer_, 814, 559, 826, 559, WHITE);

        drawText(renderer_, "EDGE CAPTURES D AFTER PROPAGATION DELAY",
                 350, 580, 1, MUTED);
    }

    void renderProbe() {
        fillRect(renderer_, probePanel_, PANEL);
        drawRect(renderer_, probePanel_, GRID);
        drawText(renderer_, "VOLTAGE PROBE", 970, 104, 2, WHITE);

        const double voltage = nodeVoltage(probeNode_);
        SDL_Rect display = {1150, 130, 220, 68};
        fillRect(renderer_, display, DISPLAY);
        drawRect(renderer_, display, GREEN);
        drawCenteredText(renderer_, voltageText(voltage), display, 3, DISPLAY_TEXT);

        drawText(renderer_, "NODE", 970, 151, 2, MUTED);
        fillRect(renderer_, probeSelect_, PANEL_LIGHT);
        drawRect(renderer_, probeSelect_, CYAN);
        drawCenteredText(renderer_, nodeName(probeNode_), probeSelect_, 2, WHITE);

        drawLine(renderer_, 1368, 220, 1387, 241, YELLOW);
        drawLine(renderer_, 1387, 241, 1377, 239, YELLOW);
        drawLine(renderer_, 1387, 241, 1385, 231, YELLOW);
    }

    void renderVoltmeter() {
        fillRect(renderer_, voltPanel_, PANEL);
        drawRect(renderer_, voltPanel_, GRID);
        drawText(renderer_, "DIGITAL VOLTMETER", 970, 269, 2, WHITE);

        const double plus = nodeVoltage(voltPositive_);
        const double minus = nodeVoltage(voltNegative_);
        double difference = std::numeric_limits<double>::quiet_NaN();
        if (std::isfinite(plus) && std::isfinite(minus)) difference = plus - minus;

        SDL_Rect display = {1000, 301, 370, 52};
        fillRect(renderer_, display, DISPLAY);
        drawRect(renderer_, display, GREEN);
        drawCenteredText(renderer_, voltageText(difference), display, 3, DISPLAY_TEXT);

        fillRect(renderer_, voltPlusSelect_, PANEL_LIGHT);
        drawRect(renderer_, voltPlusSelect_, RED);
        drawCenteredText(renderer_, "+ " + nodeName(voltPositive_),
                         voltPlusSelect_, 2, WHITE);
        fillRect(renderer_, voltMinusSelect_, PANEL_LIGHT);
        drawRect(renderer_, voltMinusSelect_, BLUE);
        drawCenteredText(renderer_, "- " + nodeName(voltNegative_),
                         voltMinusSelect_, 2, WHITE);
        drawText(renderer_, "CLICK TERMINALS TO CHANGE NODES",
                 1000, 405, 1, MUTED);
    }

    void renderAmmeter() {
        fillRect(renderer_, ampPanel_, PANEL);
        drawRect(renderer_, ampPanel_, GRID);
        drawText(renderer_, "DIGITAL AMMETER", 970, 454, 2, WHITE);

        SDL_Rect display = {1000, 486, 370, 52};
        fillRect(renderer_, display, DISPLAY);
        drawRect(renderer_, display, GREEN);
        drawCenteredText(renderer_, currentText(loadCurrentMilliAmps()),
                         display, 3, DISPLAY_TEXT);

        drawButton(resistanceMinus_, "-", false, BLUE);
        drawButton(resistancePlus_, "+", false, RED);
        drawText(renderer_, "LOAD R = " + fixedNumber(loadResistanceOhms_, 0) + " OHM",
                 1035, 563, 2, WHITE);
        drawText(renderer_, "SERIES CURRENT THROUGH R AND LED",
                 1000, 595, 1, MUTED);
    }

    double logicPlotValue(LogicState state) const {
        if (state == LogicState::High) return 1.0;
        if (state == LogicState::Low) return 0.0;
        return 0.5;
    }

    void drawScopeChannel(const SDL_Rect& graph,
                          int channelIndex,
                          const std::string& label,
                          const Color& color,
                          const std::function<LogicState(const Sample&)>& selector) {
        const int channelHeight = graph.h / 4;
        const int channelTop = graph.y + channelIndex * channelHeight;
        const int highY = channelTop + 10;
        const int lowY = channelTop + channelHeight - 12;
        const int middleY = (highY + lowY) / 2;

        drawText(renderer_, label, graph.x + 8, channelTop + 7, 1, color);
        drawLine(renderer_, graph.x + 55, lowY, graph.x + graph.w - 8, lowY,
                 {58, 66, 78, 255});

        if (samples_.size() < 2u) return;
        const double windowSeconds = 8.0;
        const double rightTime = std::max(windowSeconds, simulationTime_);
        const double leftTime = rightTime - windowSeconds;
        const int leftX = graph.x + 58;
        const int rightX = graph.x + graph.w - 8;

        bool havePrevious = false;
        int previousX = 0;
        int previousY = 0;

        for (std::deque<Sample>::const_iterator it = samples_.begin();
             it != samples_.end(); ++it) {
            if (it->time < leftTime) continue;
            const double normalized = (it->time - leftTime) / windowSeconds;
            const int x = leftX + static_cast<int>(normalized * (rightX - leftX));
            const LogicState state = selector(*it);
            int y = lowY;
            if (state == LogicState::High) y = highY;
            else if (state == LogicState::Floating) y = middleY;

            if (havePrevious) {
                drawLine(renderer_, previousX, previousY, x, previousY, color);
                drawLine(renderer_, x, previousY, x, y, color);
            }
            previousX = x;
            previousY = y;
            havePrevious = true;
        }
    }

    void renderOscilloscope() {
        fillRect(renderer_, scopePanel_, BLACK);
        drawRect(renderer_, scopePanel_, CYAN);
        drawText(renderer_, "SIMPLE OSCILLOSCOPE - 4 DIGITAL CHANNELS - 8 SECOND WINDOW",
                 45, 642, 2, WHITE);

        SDL_Rect graph = {42, 670, 1344, 190};
        fillRect(renderer_, graph, {10, 18, 17, 255});

        for (int i = 0; i <= 8; ++i) {
            const int x = graph.x + 58 + i * (graph.w - 66) / 8;
            drawLine(renderer_, x, graph.y, x, graph.y + graph.h, GRID);
        }
        for (int i = 0; i <= 4; ++i) {
            const int y = graph.y + i * graph.h / 4;
            drawLine(renderer_, graph.x, y, graph.x + graph.w, y, GRID);
        }

        drawScopeChannel(graph, 0, "D", CYAN,
                         [](const Sample& sample) { return sample.d; });
        drawScopeChannel(graph, 1, "CLK", YELLOW,
                         [](const Sample& sample) { return sample.clock; });
        drawScopeChannel(graph, 2, "Q", RED,
                         [](const Sample& sample) { return sample.q; });
        drawScopeChannel(graph, 3, "QB", PURPLE,
                         [](const Sample& sample) { return sample.qBar; });
    }

    void renderLog() {
        int y = 535;
        drawText(renderer_, "EVENT LOG", 250, 510, 1, MUTED);
        for (std::deque<std::string>::const_iterator it = logLines_.begin();
             it != logLines_.end(); ++it) {
            drawText(renderer_, *it, 250, y, 1, MUTED);
            y += 13;
            if (y > 600) break;
        }
    }

    void render() {
        setColor(renderer_, BACKGROUND);
        SDL_RenderClear(renderer_);

        renderTopBar();
        renderInputPanel();
        renderCircuit();
        renderProbe();
        renderVoltmeter();
        renderAmmeter();
        renderLog();
        renderOscilloscope();

        SDL_RenderPresent(renderer_);
    }
};

}  // namespace project9

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    project9::Application application;
    if (!application.initialize()) return 1;
    return application.run();
}
