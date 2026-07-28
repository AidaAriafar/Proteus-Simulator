/*
    Mahmoudi - Project 7: Ideal ADC and DAC with SDL2 graphics

    Linux build:
      g++ Mahmoudi_Project7_ADC_DAC_SDL2.cpp -std=c++14 -O2 \
          $(sdl2-config --cflags --libs) -o Mahmoudi_ADC_DAC

    Windows / MinGW build (SDL2 development package must be configured):
      g++ Mahmoudi_Project7_ADC_DAC_SDL2.cpp -std=c++14 -O2 \
          -I<SDL2_INCLUDE> -L<SDL2_LIB> -lmingw32 -lSDL2main -lSDL2 \
          -o Mahmoudi_ADC_DAC.exe

    Controls:
      RUN / PAUSE / STEP / STOP : simulation control
      FLOAT                      : floating/valid ADC input
      LINK                       : connect ADC bus to DAC bus
      Drag VIN slider            : change analog input (-1V to 6V)
      Click DAC bit circles      : toggle bits when LINK is OFF
      Arrow Left / Right         : change VIN
      Space                      : run/pause
      F                          : floating input
      L                          : link/unlink
      R                          : reset
*/

#define SDL_MAIN_HANDLED

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


// Compatibility helpers for older MinGW versions that do not provide
// C++17 <optional> and std::clamp. The project therefore builds with C++14.
struct NullOptType {};
static const NullOptType nullopt = NullOptType{};

template <typename T>
class Optional {
public:
    Optional() : hasValue_(false), value_() {}
    Optional(NullOptType) : hasValue_(false), value_() {}
    Optional(const T& value) : hasValue_(true), value_(value) {}
    Optional(T&& value) : hasValue_(true), value_(std::move(value)) {}

    Optional& operator=(NullOptType) {
        reset();
        return *this;
    }

    Optional& operator=(const T& value) {
        value_ = value;
        hasValue_ = true;
        return *this;
    }

    Optional& operator=(T&& value) {
        value_ = std::move(value);
        hasValue_ = true;
        return *this;
    }

    bool has_value() const noexcept {
        return hasValue_;
    }

    void reset() noexcept {
        hasValue_ = false;
        value_ = T{};
    }

    T& operator*() {
        return value_;
    }

    const T& operator*() const {
        return value_;
    }

private:
    bool hasValue_;
    T value_;
};

template <typename T>
T clampValue(const T& value, const T& minimum, const T& maximum) {
    return std::max(minimum, std::min(value, maximum));
}

namespace circuit {

enum class LogicState {
    Low,
    High,
    Undefined
};

enum class SimulationMode {
    Running,
    Paused,
    Stopped
};

struct Color {
    Uint8 r{};
    Uint8 g{};
    Uint8 b{};
    Uint8 a{255};
};

static constexpr Color BACKGROUND{22, 27, 36, 255};
static constexpr Color PANEL{34, 41, 52, 255};
static constexpr Color PANEL_LIGHT{45, 54, 68, 255};
static constexpr Color GRID{44, 52, 65, 255};
static constexpr Color WHITE{235, 239, 245, 255};
static constexpr Color MUTED{160, 171, 187, 255};
static constexpr Color CYAN{76, 201, 240, 255};
static constexpr Color BLUE{67, 97, 238, 255};
static constexpr Color GREEN{57, 211, 83, 255};
static constexpr Color YELLOW{255, 196, 61, 255};
static constexpr Color RED{239, 71, 111, 255};
static constexpr Color ORANGE{255, 145, 77, 255};
static constexpr Color BLACK{10, 12, 17, 255};

void setColor(SDL_Renderer* renderer, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fillRect(SDL_Renderer* renderer, const SDL_Rect& rectangle, Color color) {
    setColor(renderer, color);
    SDL_RenderFillRect(renderer, &rectangle);
}

void drawRect(SDL_Renderer* renderer, const SDL_Rect& rectangle, Color color) {
    setColor(renderer, color);
    SDL_RenderDrawRect(renderer, &rectangle);
}

void drawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, Color color) {
    setColor(renderer, color);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void drawFilledCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius, Color color) {
    setColor(renderer, color);
    for (int y = -radius; y <= radius; ++y) {
        const int horizontal = static_cast<int>(std::sqrt(radius * radius - y * y));
        SDL_RenderDrawLine(renderer,
                           centerX - horizontal,
                           centerY + y,
                           centerX + horizontal,
                           centerY + y);
    }
}

bool contains(const SDL_Rect& rectangle, int x, int y) {
    return x >= rectangle.x && x < rectangle.x + rectangle.w &&
           y >= rectangle.y && y < rectangle.y + rectangle.h;
}

using Glyph = std::array<std::uint8_t, 7>;

Glyph glyphFor(char character) {
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    switch (c) {
        case 'A': return {14, 17, 17, 31, 17, 17, 17};
        case 'B': return {30, 17, 17, 30, 17, 17, 30};
        case 'C': return {14, 17, 16, 16, 16, 17, 14};
        case 'D': return {30, 17, 17, 17, 17, 17, 30};
        case 'E': return {31, 16, 16, 30, 16, 16, 31};
        case 'F': return {31, 16, 16, 30, 16, 16, 16};
        case 'G': return {14, 17, 16, 23, 17, 17, 15};
        case 'H': return {17, 17, 17, 31, 17, 17, 17};
        case 'I': return {14, 4, 4, 4, 4, 4, 14};
        case 'J': return {7, 2, 2, 2, 18, 18, 12};
        case 'K': return {17, 18, 20, 24, 20, 18, 17};
        case 'L': return {16, 16, 16, 16, 16, 16, 31};
        case 'M': return {17, 27, 21, 21, 17, 17, 17};
        case 'N': return {17, 25, 21, 19, 17, 17, 17};
        case 'O': return {14, 17, 17, 17, 17, 17, 14};
        case 'P': return {30, 17, 17, 30, 16, 16, 16};
        case 'Q': return {14, 17, 17, 17, 21, 18, 13};
        case 'R': return {30, 17, 17, 30, 20, 18, 17};
        case 'S': return {15, 16, 16, 14, 1, 1, 30};
        case 'T': return {31, 4, 4, 4, 4, 4, 4};
        case 'U': return {17, 17, 17, 17, 17, 17, 14};
        case 'V': return {17, 17, 17, 17, 17, 10, 4};
        case 'W': return {17, 17, 17, 21, 21, 21, 10};
        case 'X': return {17, 17, 10, 4, 10, 17, 17};
        case 'Y': return {17, 17, 10, 4, 4, 4, 4};
        case 'Z': return {31, 1, 2, 4, 8, 16, 31};
        case '0': return {14, 17, 19, 21, 25, 17, 14};
        case '1': return {4, 12, 4, 4, 4, 4, 14};
        case '2': return {14, 17, 1, 2, 4, 8, 31};
        case '3': return {30, 1, 1, 14, 1, 1, 30};
        case '4': return {2, 6, 10, 18, 31, 2, 2};
        case '5': return {31, 16, 16, 30, 1, 1, 30};
        case '6': return {14, 16, 16, 30, 17, 17, 14};
        case '7': return {31, 1, 2, 4, 8, 8, 8};
        case '8': return {14, 17, 17, 14, 17, 17, 14};
        case '9': return {14, 17, 17, 15, 1, 1, 14};
        case '+': return {0, 4, 4, 31, 4, 4, 0};
        case '-': return {0, 0, 0, 31, 0, 0, 0};
        case '.': return {0, 0, 0, 0, 0, 12, 12};
        case ':': return {0, 12, 12, 0, 12, 12, 0};
        case '/': return {1, 2, 2, 4, 8, 8, 16};
        case '=': return {0, 31, 0, 31, 0, 0, 0};
        case '(': return {2, 4, 8, 8, 8, 4, 2};
        case ')': return {8, 4, 2, 2, 2, 4, 8};
        case '[': return {14, 8, 8, 8, 8, 8, 14};
        case ']': return {14, 2, 2, 2, 2, 2, 14};
        case '?': return {14, 17, 1, 2, 4, 0, 4};
        case '_': return {0, 0, 0, 0, 0, 0, 31};
        case ' ': return {0, 0, 0, 0, 0, 0, 0};
        default:  return {31, 17, 1, 6, 4, 0, 4};
    }
}

int textWidth(const std::string& text, int scale) {
    return static_cast<int>(text.size()) * 6 * scale;
}

void drawText(SDL_Renderer* renderer,
              int x,
              int y,
              const std::string& text,
              int scale,
              Color color) {
    setColor(renderer, color);
    int cursorX = x;
    for (char c : text) {
        const Glyph glyph = glyphFor(c);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                const bool enabled = (glyph[row] & (1U << (4 - column))) != 0;
                if (enabled) {
                    SDL_Rect pixel{
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

std::string fixedNumber(double value, int precision = 2) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

class EventScheduler {
public:
    using Callback = std::function<void()>;

    double now() const noexcept {
        return currentTime_;
    }

    std::size_t pendingEventCount() const noexcept {
        return events_.size();
    }

    void scheduleAfter(double delaySeconds, Callback callback) {
        if (!std::isfinite(delaySeconds) || delaySeconds < 0.0 || !callback) {
            throw std::invalid_argument("Invalid scheduled event.");
        }
        events_.push(Event{currentTime_ + delaySeconds,
                           nextSequence_++,
                           std::move(callback)});
    }

    void runUntil(double targetTimeSeconds) {
        if (!std::isfinite(targetTimeSeconds) || targetTimeSeconds < currentTime_) {
            throw std::invalid_argument("Target time cannot be earlier than current time.");
        }

        while (!events_.empty() && events_.top().time <= targetTimeSeconds) {
            Event event = events_.top();
            events_.pop();
            currentTime_ = event.time;
            event.callback();
        }
        currentTime_ = targetTimeSeconds;
    }

    bool stepToNextEvent() {
        if (events_.empty()) {
            return false;
        }
        runUntil(events_.top().time);
        return true;
    }

    void reset() {
        events_ = {};
        currentTime_ = 0.0;
        nextSequence_ = 0;
    }

private:
    struct Event {
        double time{};
        std::uint64_t sequence{};
        Callback callback;
    };

    struct EarlierEvent {
        bool operator()(const Event& left, const Event& right) const noexcept {
            if (left.time != right.time) {
                return left.time > right.time;
            }
            return left.sequence > right.sequence;
        }
    };

    double currentTime_{0.0};
    std::uint64_t nextSequence_{0};
    std::priority_queue<Event, std::vector<Event>, EarlierEvent> events_;
};

class Component {
public:
    Component(std::string id, std::string label, int x, int y, int width, int height)
        : id_(std::move(id)),
          label_(std::move(label)),
          bounds_{x, y, width, height} {}

    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    const std::string& id() const noexcept {
        return id_;
    }

    const SDL_Rect& bounds() const noexcept {
        return bounds_;
    }

    virtual void reset() = 0;
    virtual void draw(SDL_Renderer* renderer) const = 0;

protected:
    const std::string& label() const noexcept {
        return label_;
    }

private:
    std::string id_;
    std::string label_;
    SDL_Rect bounds_{};
};

class ADCComponent final : public Component {
public:
    ADCComponent(std::string id,
                 unsigned int bitCount,
                 double conversionDelaySeconds,
                 int x,
                 int y)
        : Component(std::move(id), "IDEAL ADC", x, y, 310, 350),
          bitCount_(validatedBitCount(bitCount)),
          maximumCode_((std::uint64_t{1} << bitCount_) - 1U),
          conversionDelaySeconds_(validatedDelay(conversionDelaySeconds)),
          outputBits_(bitCount_, LogicState::Undefined) {}

    void setBitCount(unsigned int newBitCount) {
        bitCount_ = validatedBitCount(newBitCount);
        maximumCode_ = (std::uint64_t{1} << bitCount_) - 1U;
        outputBits_.assign(bitCount_, LogicState::Undefined);
        outputCode_.reset();
        ++conversionGeneration_;
        ++outputVersion_;
    }

    unsigned int bitCount() const noexcept {
        return bitCount_;
    }

    std::uint64_t maximumCode() const noexcept {
        return maximumCode_;
    }

    double conversionDelay() const noexcept {
        return conversionDelaySeconds_;
    }

    void setConversionDelay(double seconds) {
        conversionDelaySeconds_ = validatedDelay(seconds);
    }

    void setReferences(double negativeReference,
                       double positiveReference,
                       EventScheduler& scheduler) {
        if (!std::isfinite(negativeReference) ||
            !std::isfinite(positiveReference) ||
            positiveReference <= negativeReference) {
            lastWarning_ = "ADC REFERENCE RANGE IS INVALID";
            return;
        }
        negativeReference_ = negativeReference;
        positiveReference_ = positiveReference;
        requestConversion(scheduler);
    }

    void setAnalogInput(Optional<double> voltage,
                        EventScheduler& scheduler) {
        if (voltage.has_value() && !std::isfinite(*voltage)) {
            return;
        }
        analogInput_ = voltage;
        requestConversion(scheduler);
    }

    Optional<double> analogInput() const noexcept {
        return analogInput_;
    }

    double negativeReference() const noexcept {
        return negativeReference_;
    }

    double positiveReference() const noexcept {
        return positiveReference_;
    }

    Optional<std::uint64_t> outputCode() const noexcept {
        return outputCode_;
    }

    LogicState outputBit(unsigned int bitIndex) const {
        if (bitIndex >= outputBits_.size()) {
            return LogicState::Undefined;
        }
        return outputBits_[bitIndex];
    }

    std::uint64_t outputVersion() const noexcept {
        return outputVersion_;
    }

    bool conversionPending() const noexcept {
        return conversionPending_;
    }

    const std::string& lastWarning() const noexcept {
        return lastWarning_;
    }

    SDL_Point outputPinPosition(unsigned int bitIndex) const {
        const SDL_Rect& body = bounds();
        const int top = body.y + 72;
        const int bottom = body.y + body.h - 56;
        const int spacing = bitCount_ > 1
            ? (bottom - top) / static_cast<int>(bitCount_ - 1)
            : 0;
        return SDL_Point{body.x + body.w, top + spacing * static_cast<int>(bitIndex)};
    }

    void reset() override {
        ++conversionGeneration_;
        conversionPending_ = false;
        analogInput_.reset();
        outputCode_.reset();
        std::fill(outputBits_.begin(), outputBits_.end(), LogicState::Undefined);
        lastWarning_.clear();
        ++outputVersion_;
    }

    void draw(SDL_Renderer* renderer) const override {
        const SDL_Rect body = bounds();
        fillRect(renderer, body, PANEL);
        drawRect(renderer, body, CYAN);
        SDL_Rect titleBar{body.x, body.y, body.w, 45};
        fillRect(renderer, titleBar, PANEL_LIGHT);
        drawText(renderer, body.x + 18, body.y + 14, label(), 2, WHITE);

        drawAnalogPins(renderer);
        drawDigitalPins(renderer);

        const std::string vinText = analogInput_.has_value()
            ? "VIN=" + fixedNumber(*analogInput_) + "V"
            : "VIN=FLOAT";
        drawText(renderer, body.x + 24, body.y + 225, vinText, 2,
                 analogInput_.has_value() ? WHITE : ORANGE);

        drawText(renderer,
                 body.x + 24,
                 body.y + 252,
                 "RANGE=" + fixedNumber(negativeReference_, 1) + ".." +
                     fixedNumber(positiveReference_, 1) + "V",
                 1,
                 MUTED);

        drawText(renderer,
                 body.x + 24,
                 body.y + 272,
                 "BITS=" + std::to_string(bitCount_) +
                     "  DELAY=" + std::to_string(static_cast<int>(conversionDelaySeconds_ * 1000.0)) + "MS",
                 1,
                 MUTED);

        std::string outputText = "CODE=UNDEFINED";
        if (outputCode_.has_value()) {
            outputText = "CODE=" + std::to_string(*outputCode_);
        }
        drawText(renderer, body.x + 24, body.y + 297, outputText, 2,
                 outputCode_.has_value() ? GREEN : RED);

        if (conversionPending_) {
            drawFilledCircle(renderer, body.x + body.w - 24, body.y + 23, 7, YELLOW);
            drawText(renderer, body.x + 24, body.y + 325, "CONVERTING", 1, YELLOW);
        } else {
            drawFilledCircle(renderer, body.x + body.w - 24, body.y + 23, 7, GREEN);
            drawText(renderer, body.x + 24, body.y + 325, saturationText(), 1,
                     saturationColor());
        }
    }

private:
    struct SampleResult {
        bool valid{false};
        std::uint64_t code{0};
        std::string warning;
    };

    static unsigned int validatedBitCount(unsigned int value) {
        if (value < 1 || value > 16) {
            throw std::invalid_argument("ADC bit count must be between 1 and 16.");
        }
        return value;
    }

    static double validatedDelay(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0.0) {
            throw std::invalid_argument("ADC conversion delay is invalid.");
        }
        return seconds;
    }

    SampleResult sampleAndQuantize() const {
        if (!analogInput_.has_value()) {
            return {false, 0, "ADC INPUT IS FLOATING"};
        }
        if (positiveReference_ <= negativeReference_) {
            return {false, 0, "ADC REFERENCE RANGE IS INVALID"};
        }

        const double input = *analogInput_;
        if (input <= negativeReference_) {
            return {true, 0, {}};
        }
        if (input >= positiveReference_) {
            return {true, maximumCode_, {}};
        }

        const double normalized =
            (input - negativeReference_) / (positiveReference_ - negativeReference_);
        const auto code = static_cast<std::uint64_t>(
            std::llround(normalized * static_cast<double>(maximumCode_)));
        return {true, std::min(code, maximumCode_), {}};
    }

    void requestConversion(EventScheduler& scheduler) {
        const SampleResult sample = sampleAndQuantize();
        const std::uint64_t generation = ++conversionGeneration_;
        conversionPending_ = true;

        scheduler.scheduleAfter(conversionDelaySeconds_, [this, generation, sample]() {
            if (generation != conversionGeneration_) {
                return;
            }
            conversionPending_ = false;
            if (!sample.valid) {
                outputCode_.reset();
                std::fill(outputBits_.begin(), outputBits_.end(), LogicState::Undefined);
                lastWarning_ = sample.warning;
            } else {
                outputCode_ = sample.code;
                for (unsigned int bit = 0; bit < bitCount_; ++bit) {
                    const bool high = ((sample.code >> bit) & std::uint64_t{1}) != 0;
                    outputBits_[bit] = high ? LogicState::High : LogicState::Low;
                }
                lastWarning_.clear();
            }
            ++outputVersion_;
        });
    }

    Color stateColor(LogicState state) const {
        if (state == LogicState::High) {
            return GREEN;
        }
        if (state == LogicState::Low) {
            return BLUE;
        }
        return ORANGE;
    }

    void drawAnalogPins(SDL_Renderer* renderer) const {
        const SDL_Rect& body = bounds();
        const std::array<std::pair<std::string, int>, 3> pins{{
            {"VREF+", body.y + 85},
            {"VIN", body.y + 137},
            {"VREF-", body.y + 189}
        }};
        for (const auto& pin : pins) {
            drawLine(renderer, body.x - 24, pin.second, body.x, pin.second, WHITE);
            drawFilledCircle(renderer, body.x - 27, pin.second, 5, CYAN);
            drawText(renderer, body.x + 12, pin.second - 6, pin.first, 1, MUTED);
        }
    }

    void drawDigitalPins(SDL_Renderer* renderer) const {
        const SDL_Rect& body = bounds();
        for (unsigned int bit = 0; bit < bitCount_; ++bit) {
            const SDL_Point pin = outputPinPosition(bit);
            drawLine(renderer, body.x + body.w, pin.y, body.x + body.w + 24, pin.y,
                     stateColor(outputBit(bit)));
            drawFilledCircle(renderer, body.x + body.w + 27, pin.y, 5,
                             stateColor(outputBit(bit)));
            drawText(renderer,
                     body.x + body.w - 42,
                     pin.y - 5,
                     "D" + std::to_string(bit),
                     1,
                     stateColor(outputBit(bit)));
        }
    }

    std::string saturationText() const {
        if (!analogInput_.has_value()) {
            return "FLOATING INPUT";
        }
        if (*analogInput_ <= negativeReference_) {
            return "SATURATED LOW";
        }
        if (*analogInput_ >= positiveReference_) {
            return "SATURATED HIGH";
        }
        return "LINEAR RANGE";
    }

    Color saturationColor() const {
        if (!analogInput_.has_value()) {
            return ORANGE;
        }
        if (*analogInput_ <= negativeReference_ ||
            *analogInput_ >= positiveReference_) {
            return YELLOW;
        }
        return GREEN;
    }

    unsigned int bitCount_{8};
    std::uint64_t maximumCode_{255};
    double conversionDelaySeconds_{0.25};
    Optional<double> analogInput_;
    double negativeReference_{0.0};
    double positiveReference_{5.0};
    std::vector<LogicState> outputBits_;
    Optional<std::uint64_t> outputCode_;
    bool conversionPending_{false};
    std::uint64_t conversionGeneration_{0};
    std::uint64_t outputVersion_{0};
    std::string lastWarning_;
};

class DACComponent final : public Component {
public:
    DACComponent(std::string id,
                 unsigned int bitCount,
                 double conversionDelaySeconds,
                 int x,
                 int y)
        : Component(std::move(id), "IDEAL DAC", x, y, 310, 350),
          bitCount_(validatedBitCount(bitCount)),
          maximumCode_((std::uint64_t{1} << bitCount_) - 1U),
          conversionDelaySeconds_(validatedDelay(conversionDelaySeconds)),
          inputBits_(bitCount_, LogicState::Undefined) {}

    void setBitCount(unsigned int newBitCount) {
        bitCount_ = validatedBitCount(newBitCount);
        maximumCode_ = (std::uint64_t{1} << bitCount_) - 1U;
        inputBits_.assign(bitCount_, LogicState::Undefined);
        analogOutput_.reset();
        ++conversionGeneration_;
    }

    unsigned int bitCount() const noexcept {
        return bitCount_;
    }

    double conversionDelay() const noexcept {
        return conversionDelaySeconds_;
    }

    void setConversionDelay(double seconds) {
        conversionDelaySeconds_ = validatedDelay(seconds);
    }

    void setReferences(double negativeReference,
                       double positiveReference,
                       EventScheduler& scheduler) {
        if (!std::isfinite(negativeReference) ||
            !std::isfinite(positiveReference) ||
            positiveReference <= negativeReference) {
            lastWarning_ = "DAC REFERENCE RANGE IS INVALID";
            return;
        }
        negativeReference_ = negativeReference;
        positiveReference_ = positiveReference;
        requestConversion(scheduler);
    }

    void setInputCode(std::uint64_t code, EventScheduler& scheduler) {
        code = std::min(code, maximumCode_);
        for (unsigned int bit = 0; bit < bitCount_; ++bit) {
            const bool high = ((code >> bit) & std::uint64_t{1}) != 0;
            inputBits_[bit] = high ? LogicState::High : LogicState::Low;
        }
        requestConversion(scheduler);
    }

    void setInputUndefined(EventScheduler& scheduler) {
        std::fill(inputBits_.begin(), inputBits_.end(), LogicState::Undefined);
        requestConversion(scheduler);
    }

    void toggleInputBit(unsigned int bitIndex, EventScheduler& scheduler) {
        if (bitIndex >= bitCount_) {
            return;
        }
        inputBits_[bitIndex] = inputBits_[bitIndex] == LogicState::High
            ? LogicState::Low
            : LogicState::High;
        requestConversion(scheduler);
    }

    LogicState inputBit(unsigned int bitIndex) const {
        if (bitIndex >= inputBits_.size()) {
            return LogicState::Undefined;
        }
        return inputBits_[bitIndex];
    }

    Optional<std::uint64_t> inputCode() const {
        std::uint64_t code = 0;
        for (unsigned int bit = 0; bit < bitCount_; ++bit) {
            if (inputBits_[bit] == LogicState::Undefined) {
                return nullopt;
            }
            if (inputBits_[bit] == LogicState::High) {
                code |= (std::uint64_t{1} << bit);
            }
        }
        return code;
    }

    Optional<double> outputVoltage() const noexcept {
        return analogOutput_;
    }

    bool conversionPending() const noexcept {
        return conversionPending_;
    }

    const std::string& lastWarning() const noexcept {
        return lastWarning_;
    }

    SDL_Point inputPinPosition(unsigned int bitIndex) const {
        const SDL_Rect& body = bounds();
        const int top = body.y + 72;
        const int bottom = body.y + body.h - 56;
        const int spacing = bitCount_ > 1
            ? (bottom - top) / static_cast<int>(bitCount_ - 1)
            : 0;
        return SDL_Point{body.x, bottom - spacing * static_cast<int>(bitIndex)};
    }

    SDL_Rect inputBitHitArea(unsigned int bitIndex) const {
        const SDL_Point point = inputPinPosition(bitIndex);
        return SDL_Rect{point.x - 34, point.y - 11, 22, 22};
    }

    void reset() override {
        ++conversionGeneration_;
        conversionPending_ = false;
        std::fill(inputBits_.begin(), inputBits_.end(), LogicState::Undefined);
        analogOutput_.reset();
        lastWarning_.clear();
    }

    void draw(SDL_Renderer* renderer) const override {
        const SDL_Rect body = bounds();
        fillRect(renderer, body, PANEL);
        drawRect(renderer, body, CYAN);
        SDL_Rect titleBar{body.x, body.y, body.w, 45};
        fillRect(renderer, titleBar, PANEL_LIGHT);
        drawText(renderer, body.x + 18, body.y + 14, label(), 2, WHITE);

        drawDigitalPins(renderer);
        drawAnalogPins(renderer);

        const Optional<std::uint64_t> code = inputCode();
        const std::string codeText = code.has_value()
            ? "CODE=" + std::to_string(*code)
            : "CODE=UNDEFINED";
        drawText(renderer, body.x + 24, body.y + 225, codeText, 2,
                 code.has_value() ? WHITE : ORANGE);

        drawText(renderer,
                 body.x + 24,
                 body.y + 252,
                 "RANGE=" + fixedNumber(negativeReference_, 1) + ".." +
                     fixedNumber(positiveReference_, 1) + "V",
                 1,
                 MUTED);

        drawText(renderer,
                 body.x + 24,
                 body.y + 272,
                 "BITS=" + std::to_string(bitCount_) +
                     "  DELAY=" + std::to_string(static_cast<int>(conversionDelaySeconds_ * 1000.0)) + "MS",
                 1,
                 MUTED);

        const std::string voltageText = analogOutput_.has_value()
            ? "VOUT=" + fixedNumber(*analogOutput_) + "V"
            : "VOUT=UNDEFINED";
        drawText(renderer, body.x + 24, body.y + 297, voltageText, 2,
                 analogOutput_.has_value() ? GREEN : RED);

        drawOutputBar(renderer);

        if (conversionPending_) {
            drawFilledCircle(renderer, body.x + body.w - 24, body.y + 23, 7, YELLOW);
            drawText(renderer, body.x + 24, body.y + 325, "CONVERTING", 1, YELLOW);
        } else {
            drawFilledCircle(renderer, body.x + body.w - 24, body.y + 23, 7,
                             analogOutput_.has_value() ? GREEN : ORANGE);
        }
    }

private:
    struct ConversionResult {
        bool valid{false};
        double voltage{0.0};
        std::string warning;
    };

    static unsigned int validatedBitCount(unsigned int value) {
        if (value < 1 || value > 16) {
            throw std::invalid_argument("DAC bit count must be between 1 and 16.");
        }
        return value;
    }

    static double validatedDelay(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0.0) {
            throw std::invalid_argument("DAC conversion delay is invalid.");
        }
        return seconds;
    }

    ConversionResult calculateVoltage() const {
        const auto code = inputCode();
        if (!code.has_value()) {
            return {false, 0.0, "DAC INPUT HAS AN UNDEFINED BIT"};
        }
        if (positiveReference_ <= negativeReference_) {
            return {false, 0.0, "DAC REFERENCE RANGE IS INVALID"};
        }
        const double normalized = static_cast<double>(*code) /
                                  static_cast<double>(maximumCode_);
        const double voltage = negativeReference_ +
            normalized * (positiveReference_ - negativeReference_);
        return {true, voltage, {}};
    }

    void requestConversion(EventScheduler& scheduler) {
        const ConversionResult conversion = calculateVoltage();
        const std::uint64_t generation = ++conversionGeneration_;
        conversionPending_ = true;

        scheduler.scheduleAfter(conversionDelaySeconds_, [this, generation, conversion]() {
            if (generation != conversionGeneration_) {
                return;
            }
            conversionPending_ = false;
            if (!conversion.valid) {
                analogOutput_.reset();
                lastWarning_ = conversion.warning;
            } else {
                analogOutput_ = conversion.voltage;
                lastWarning_.clear();
            }
        });
    }

    Color stateColor(LogicState state) const {
        if (state == LogicState::High) {
            return GREEN;
        }
        if (state == LogicState::Low) {
            return BLUE;
        }
        return ORANGE;
    }

    void drawDigitalPins(SDL_Renderer* renderer) const {
        const SDL_Rect& body = bounds();
        for (unsigned int bit = 0; bit < bitCount_; ++bit) {
            const SDL_Point pin = inputPinPosition(bit);
            drawLine(renderer, body.x - 24, pin.y, body.x, pin.y,
                     stateColor(inputBit(bit)));
            drawFilledCircle(renderer, body.x - 27, pin.y, 7,
                             stateColor(inputBit(bit)));
            drawText(renderer, body.x + 12, pin.y - 5,
                     "D" + std::to_string(bit), 1, stateColor(inputBit(bit)));
        }
    }

    void drawAnalogPins(SDL_Renderer* renderer) const {
        const SDL_Rect& body = bounds();
        const std::array<std::pair<std::string, int>, 3> pins{{
            {"VREF+", body.y + 85},
            {"VOUT", body.y + 137},
            {"VREF-", body.y + 189}
        }};
        for (const auto& pin : pins) {
            drawLine(renderer, body.x + body.w, pin.second,
                     body.x + body.w + 24, pin.second, WHITE);
            drawFilledCircle(renderer, body.x + body.w + 27, pin.second, 5, CYAN);
            drawText(renderer, body.x + body.w - 60, pin.second - 6, pin.first, 1, MUTED);
        }
    }

    void drawOutputBar(SDL_Renderer* renderer) const {
        const SDL_Rect& body = bounds();
        SDL_Rect backgroundBar{body.x + 175, body.y + 324, 105, 10};
        fillRect(renderer, backgroundBar, BLACK);
        drawRect(renderer, backgroundBar, MUTED);
        if (!analogOutput_.has_value()) {
            return;
        }
        const double normalized = clampValue(
            (*analogOutput_ - negativeReference_) /
                (positiveReference_ - negativeReference_),
            0.0,
            1.0);
        SDL_Rect valueBar{backgroundBar.x + 1,
                          backgroundBar.y + 1,
                          static_cast<int>((backgroundBar.w - 2) * normalized),
                          backgroundBar.h - 2};
        fillRect(renderer, valueBar, GREEN);
    }

    unsigned int bitCount_{8};
    std::uint64_t maximumCode_{255};
    double conversionDelaySeconds_{0.18};
    std::vector<LogicState> inputBits_;
    double negativeReference_{0.0};
    double positiveReference_{5.0};
    Optional<double> analogOutput_;
    bool conversionPending_{false};
    std::uint64_t conversionGeneration_{0};
    std::string lastWarning_;
};

class Application {
public:
    Application()
        : adc_("ADC1", 8, 0.25, 110, 145),
          dac_("DAC1", 8, 0.18, 860, 145) {}

    int run() {
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
            return 1;
        }

        window_ = SDL_CreateWindow("Mahmoudi Project 7 - ADC DAC SDL2",
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   WINDOW_WIDTH,
                                   WINDOW_HEIGHT,
                                   SDL_WINDOW_SHOWN);
        if (window_ == nullptr) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
            SDL_Quit();
            return 1;
        }

        renderer_ = SDL_CreateRenderer(window_,
                                       -1,
                                       SDL_RENDERER_ACCELERATED |
                                           SDL_RENDERER_PRESENTVSYNC);
        if (renderer_ == nullptr) {
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        }
        if (renderer_ == nullptr) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return 1;
        }

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        resetSimulation();

        Uint32 previousTicks = SDL_GetTicks();
        bool quit = false;
        while (!quit) {
            SDL_Event event{};
            while (SDL_PollEvent(&event) != 0) {
                if (event.type == SDL_QUIT) {
                    quit = true;
                } else {
                    processEvent(event);
                }
            }

            const Uint32 currentTicks = SDL_GetTicks();
            const double realDelta = std::min(
                static_cast<double>(currentTicks - previousTicks) / 1000.0,
                0.1);
            previousTicks = currentTicks;

            if (simulationMode_ == SimulationMode::Running) {
                scheduler_.runUntil(scheduler_.now() + realDelta);
            }

            synchronizeAdcAndDac();
            render();
        }

        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return 0;
    }

private:
    static constexpr int WINDOW_WIDTH = 1280;
    static constexpr int WINDOW_HEIGHT = 720;

    struct Button {
        SDL_Rect rectangle{};
        std::string text;
    };

    const Button runButton_{{20, 18, 90, 38}, "RUN"};
    const Button pauseButton_{{120, 18, 90, 38}, "PAUSE"};
    const Button stepButton_{{220, 18, 90, 38}, "STEP"};
    const Button stopButton_{{320, 18, 90, 38}, "STOP"};
    const Button floatButton_{{430, 18, 100, 38}, "FLOAT"};
    const Button linkButton_{{540, 18, 120, 38}, "LINK"};
    const Button bitMinusButton_{{680, 18, 90, 38}, "BIT-"};
    const Button bitPlusButton_{{780, 18, 90, 38}, "BIT+"};
    const Button delayMinusButton_{{890, 18, 100, 38}, "DLY-"};
    const Button delayPlusButton_{{1000, 18, 100, 38}, "DLY+"};
    const Button refMinusButton_{{1110, 18, 70, 38}, "REF-"};
    const Button refPlusButton_{{1190, 18, 70, 38}, "REF+"};

    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    EventScheduler scheduler_;
    ADCComponent adc_;
    DACComponent dac_;
    SimulationMode simulationMode_{SimulationMode::Stopped};
    bool busLinked_{true};
    bool inputFloating_{false};
    bool draggingSlider_{false};
    double analogInputVoltage_{2.50};
    std::uint64_t lastAdcOutputVersion_{std::numeric_limits<std::uint64_t>::max()};
    std::string statusMessage_{"READY"};

    SDL_Rect sliderTrack() const {
        return SDL_Rect{235, 602, 810, 12};
    }

    SDL_Rect sliderHitArea() const {
        return SDL_Rect{210, 580, 860, 55};
    }

    void processEvent(const SDL_Event& event) {
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            processKey(event.key.keysym.sym);
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            processMouseDown(event.button.x, event.button.y);
        }

        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            draggingSlider_ = false;
        }

        if (event.type == SDL_MOUSEMOTION && draggingSlider_) {
            updateSliderFromMouse(event.motion.x);
        }
    }

    void processKey(SDL_Keycode key) {
        switch (key) {
            case SDLK_SPACE:
                if (simulationMode_ == SimulationMode::Running) {
                    simulationMode_ = SimulationMode::Paused;
                    statusMessage_ = "SIMULATION PAUSED";
                } else {
                    simulationMode_ = SimulationMode::Running;
                    statusMessage_ = "SIMULATION RUNNING";
                }
                break;
            case SDLK_r:
                resetSimulation();
                break;
            case SDLK_f:
                toggleFloatingInput();
                break;
            case SDLK_l:
                busLinked_ = !busLinked_;
                lastAdcOutputVersion_ = std::numeric_limits<std::uint64_t>::max();
                statusMessage_ = busLinked_ ? "ADC BUS LINKED TO DAC" : "DAC MANUAL MODE";
                break;
            case SDLK_LEFT:
                setAnalogVoltage(analogInputVoltage_ - 0.05);
                break;
            case SDLK_RIGHT:
                setAnalogVoltage(analogInputVoltage_ + 0.05);
                break;
            case SDLK_n:
                simulationMode_ = SimulationMode::Paused;
                if (!scheduler_.stepToNextEvent()) {
                    statusMessage_ = "NO PENDING EVENT";
                } else {
                    statusMessage_ = "EXECUTED NEXT EVENT";
                }
                break;
            default:
                break;
        }
    }

    void processMouseDown(int mouseX, int mouseY) {
        if (contains(runButton_.rectangle, mouseX, mouseY)) {
            simulationMode_ = SimulationMode::Running;
            statusMessage_ = "SIMULATION RUNNING";
            return;
        }
        if (contains(pauseButton_.rectangle, mouseX, mouseY)) {
            simulationMode_ = SimulationMode::Paused;
            statusMessage_ = "SIMULATION PAUSED";
            return;
        }
        if (contains(stepButton_.rectangle, mouseX, mouseY)) {
            simulationMode_ = SimulationMode::Paused;
            statusMessage_ = scheduler_.stepToNextEvent()
                ? "EXECUTED NEXT EVENT"
                : "NO PENDING EVENT";
            synchronizeAdcAndDac();
            return;
        }
        if (contains(stopButton_.rectangle, mouseX, mouseY)) {
            resetSimulation();
            return;
        }
        if (contains(floatButton_.rectangle, mouseX, mouseY)) {
            toggleFloatingInput();
            return;
        }
        if (contains(linkButton_.rectangle, mouseX, mouseY)) {
            busLinked_ = !busLinked_;
            lastAdcOutputVersion_ = std::numeric_limits<std::uint64_t>::max();
            statusMessage_ = busLinked_ ? "ADC BUS LINKED TO DAC" : "DAC MANUAL MODE";
            return;
        }
        if (contains(bitMinusButton_.rectangle, mouseX, mouseY)) {
            changeBitCount(-1);
            return;
        }
        if (contains(bitPlusButton_.rectangle, mouseX, mouseY)) {
            changeBitCount(1);
            return;
        }
        if (contains(delayMinusButton_.rectangle, mouseX, mouseY)) {
            changeDelay(-0.05);
            return;
        }
        if (contains(delayPlusButton_.rectangle, mouseX, mouseY)) {
            changeDelay(0.05);
            return;
        }
        if (contains(refMinusButton_.rectangle, mouseX, mouseY)) {
            changePositiveReference(-0.5);
            return;
        }
        if (contains(refPlusButton_.rectangle, mouseX, mouseY)) {
            changePositiveReference(0.5);
            return;
        }
        if (contains(sliderHitArea(), mouseX, mouseY)) {
            draggingSlider_ = true;
            updateSliderFromMouse(mouseX);
            return;
        }

        if (!busLinked_) {
            for (unsigned int bit = 0; bit < dac_.bitCount(); ++bit) {
                if (contains(dac_.inputBitHitArea(bit), mouseX, mouseY)) {
                    dac_.toggleInputBit(bit, scheduler_);
                    statusMessage_ = "DAC BIT D" + std::to_string(bit) + " TOGGLED";
                    return;
                }
            }
        }
    }

    void resetSimulation() {
        scheduler_.reset();
        adc_.reset();
        dac_.reset();
        simulationMode_ = SimulationMode::Stopped;
        busLinked_ = true;
        inputFloating_ = false;
        analogInputVoltage_ = 2.50;
        lastAdcOutputVersion_ = std::numeric_limits<std::uint64_t>::max();
        adc_.setAnalogInput(analogInputVoltage_, scheduler_);
        statusMessage_ = "STOPPED AND RESET - PRESS RUN";
    }

    void toggleFloatingInput() {
        inputFloating_ = !inputFloating_;
        if (inputFloating_) {
            adc_.setAnalogInput(nullopt, scheduler_);
            statusMessage_ = "ADC INPUT IS FLOATING";
        } else {
            adc_.setAnalogInput(analogInputVoltage_, scheduler_);
            statusMessage_ = "ADC INPUT RESTORED";
        }
    }

    void setAnalogVoltage(double voltage) {
        analogInputVoltage_ = clampValue(voltage, -1.0, 6.0);
        if (!inputFloating_) {
            adc_.setAnalogInput(analogInputVoltage_, scheduler_);
        }
        statusMessage_ = "VIN CHANGED TO " + fixedNumber(analogInputVoltage_) + "V";
    }

    void updateSliderFromMouse(int mouseX) {
        const SDL_Rect sliderRectangle = sliderTrack();

        const double sliderWidth =
            (sliderRectangle.w > 0)
                ? static_cast<double>(sliderRectangle.w)
                : 1.0;

        const double normalized = clampValue(
            static_cast<double>(mouseX - sliderRectangle.x) / sliderWidth,
            0.0,
            1.0
        );

        setAnalogVoltage(-1.0 + normalized * 7.0);
    }

    void changeBitCount(int direction) {
        const int newCount = clampValue(
            static_cast<int>(adc_.bitCount()) + direction,
            2,
            12);
        if (newCount == static_cast<int>(adc_.bitCount())) {
            return;
        }
        adc_.setBitCount(static_cast<unsigned int>(newCount));
        dac_.setBitCount(static_cast<unsigned int>(newCount));
        lastAdcOutputVersion_ = std::numeric_limits<std::uint64_t>::max();
        if (!inputFloating_) {
            adc_.setAnalogInput(analogInputVoltage_, scheduler_);
        } else {
            adc_.setAnalogInput(nullopt, scheduler_);
        }
        statusMessage_ = "BUS WIDTH CHANGED TO " + std::to_string(newCount) + " BITS";
    }

    void changeDelay(double deltaSeconds) {
        const double newAdcDelay = clampValue(adc_.conversionDelay() + deltaSeconds,
                                              0.0,
                                              1.0);
        const double newDacDelay = clampValue(dac_.conversionDelay() + deltaSeconds,
                                              0.0,
                                              1.0);
        adc_.setConversionDelay(newAdcDelay);
        dac_.setConversionDelay(newDacDelay);
        statusMessage_ = "CONVERSION DELAY UPDATED";
    }

    void changePositiveReference(double deltaVoltage) {
        const double newReference = clampValue(adc_.positiveReference() + deltaVoltage,
                                               1.0,
                                               10.0);
        adc_.setReferences(0.0, newReference, scheduler_);
        dac_.setReferences(0.0, newReference, scheduler_);
        statusMessage_ = "VREF+ CHANGED TO " + fixedNumber(newReference, 1) + "V";
    }

    void synchronizeAdcAndDac() {
        if (!busLinked_) {
            return;
        }
        if (adc_.outputVersion() == lastAdcOutputVersion_) {
            return;
        }
        lastAdcOutputVersion_ = adc_.outputVersion();
        if (adc_.outputCode().has_value()) {
            dac_.setInputCode(*adc_.outputCode(), scheduler_);
        } else {
            dac_.setInputUndefined(scheduler_);
        }
    }

    void drawGrid() const {
        for (int x = 0; x < WINDOW_WIDTH; x += 20) {
            drawLine(renderer_, x, 78, x, WINDOW_HEIGHT, GRID);
        }
        for (int y = 78; y < WINDOW_HEIGHT; y += 20) {
            drawLine(renderer_, 0, y, WINDOW_WIDTH, y, GRID);
        }
    }

    void drawButton(const Button& button, bool active = false) const {
        int mouseX = 0;
        int mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        const bool hovered = contains(button.rectangle, mouseX, mouseY);
        Color background = active ? BLUE : PANEL_LIGHT;
        if (hovered) {
            background = active ? CYAN : Color{63, 74, 91, 255};
        }
        fillRect(renderer_, button.rectangle, background);
        drawRect(renderer_, button.rectangle, active ? CYAN : MUTED);
        const int labelX = button.rectangle.x +
            (button.rectangle.w - textWidth(button.text, 1)) / 2;
        const int labelY = button.rectangle.y + 15;
        drawText(renderer_, labelX, labelY, button.text, 1, WHITE);
    }

    void drawToolbar() const {
        SDL_Rect toolbar{0, 0, WINDOW_WIDTH, 76};
        fillRect(renderer_, toolbar, Color{18, 22, 30, 255});
        drawLine(renderer_, 0, 75, WINDOW_WIDTH, 75, CYAN);

        drawButton(runButton_, simulationMode_ == SimulationMode::Running);
        drawButton(pauseButton_, simulationMode_ == SimulationMode::Paused);
        drawButton(stepButton_);
        drawButton(stopButton_, simulationMode_ == SimulationMode::Stopped);
        drawButton(floatButton_, inputFloating_);
        drawButton(linkButton_, busLinked_);
        drawButton(bitMinusButton_);
        drawButton(bitPlusButton_);
        drawButton(delayMinusButton_);
        drawButton(delayPlusButton_);
        drawButton(refMinusButton_);
        drawButton(refPlusButton_);
    }

    Color wireColor(LogicState state) const {
        if (!busLinked_) {
            return MUTED;
        }
        if (state == LogicState::High) {
            return GREEN;
        }
        if (state == LogicState::Low) {
            return BLUE;
        }
        return ORANGE;
    }

    void drawBusWires() const {
        const unsigned int count = std::min(adc_.bitCount(), dac_.bitCount());
        for (unsigned int bit = 0; bit < count; ++bit) {
            const SDL_Point from = adc_.outputPinPosition(bit);
            const SDL_Point to = dac_.inputPinPosition(bit);
            const int midX = 565 + static_cast<int>(bit) * 21;
            const Color color = wireColor(adc_.outputBit(bit));

            drawLine(renderer_, from.x + 24, from.y, midX, from.y, color);
            drawLine(renderer_, midX, from.y, midX, to.y, color);
            drawLine(renderer_, midX, to.y, to.x - 24, to.y, color);
            drawFilledCircle(renderer_, midX, from.y, 3, color);
            drawFilledCircle(renderer_, midX, to.y, 3, color);
        }

        drawText(renderer_, 575, 112,
                 busLinked_ ? "DIGITAL BUS - LINKED" : "DIGITAL BUS - MANUAL DAC",
                 1,
                 busLinked_ ? CYAN : MUTED);
    }

    void drawSlider() const {
        const SDL_Rect track = sliderTrack();
        fillRect(renderer_, track, BLACK);
        drawRect(renderer_, track, MUTED);

        const double normalized = (analogInputVoltage_ + 1.0) / 7.0;
        const int knobX = track.x + static_cast<int>(normalized * track.w);
        SDL_Rect valueRect{track.x + 1,
                           track.y + 1,
                           std::max(0, knobX - track.x - 1),
                           track.h - 2};
        fillRect(renderer_, valueRect, CYAN);
        drawFilledCircle(renderer_, knobX, track.y + track.h / 2, 11,
                         inputFloating_ ? ORANGE : WHITE);
        drawFilledCircle(renderer_, knobX, track.y + track.h / 2, 6,
                         inputFloating_ ? RED : BLUE);

        drawText(renderer_, 110, 594, "VIN", 2, WHITE);
        drawText(renderer_, track.x - 15, 624, "-1V", 1, MUTED);
        drawText(renderer_, track.x + track.w - 15, 624, "6V", 1, MUTED);
        drawText(renderer_, 1080, 594,
                 inputFloating_ ? "FLOAT" : fixedNumber(analogInputVoltage_) + "V",
                 2,
                 inputFloating_ ? ORANGE : GREEN);

        const double refNormalized = (adc_.positiveReference() + 1.0) / 7.0;
        const int refX = track.x + static_cast<int>(clampValue(refNormalized, 0.0, 1.0) * track.w);
        drawLine(renderer_, refX, track.y - 12, refX, track.y + 24, YELLOW);
        drawText(renderer_, refX - 24, track.y - 28, "VREF+", 1, YELLOW);
    }

    void drawStatusPanel() const {
        SDL_Rect panel{20, 651, WINDOW_WIDTH - 40, 52};
        fillRect(renderer_, panel, Color{17, 21, 28, 245});
        drawRect(renderer_, panel, PANEL_LIGHT);

        const std::string timeText = "TIME=" + fixedNumber(scheduler_.now(), 3) +
            "S  EVENTS=" + std::to_string(scheduler_.pendingEventCount());
        drawText(renderer_, 35, 664, timeText, 1, CYAN);
        drawText(renderer_, 35, 684, statusMessage_, 1, WHITE);

        std::string warning;
        if (!adc_.lastWarning().empty()) {
            warning = adc_.lastWarning();
        } else if (!dac_.lastWarning().empty()) {
            warning = dac_.lastWarning();
        }
        if (!warning.empty()) {
            drawText(renderer_, 720, 684, "WARNING: " + warning, 1, ORANGE);
        }
    }

    void render() const {
        setColor(renderer_, BACKGROUND);
        SDL_RenderClear(renderer_);
        drawGrid();
        drawToolbar();
        drawBusWires();
        adc_.draw(renderer_);
        dac_.draw(renderer_);
        drawSlider();
        drawStatusPanel();
        SDL_RenderPresent(renderer_);
    }
};

} // namespace circuit

int main() {
    try {
        circuit::Application application;
        return application.run();
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
