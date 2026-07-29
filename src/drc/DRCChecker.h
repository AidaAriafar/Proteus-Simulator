#ifndef DRC_CHECKER_H
#define DRC_CHECKER_H

#include "../core/Component.h"

#include <cstddef>
#include <string>
#include <vector>

enum class LogSeverity
{
    Info,
    Warning,
    Error
};

struct LogEntry
{
    std::size_t sequence;
    LogSeverity severity;
    std::string message;
};

class SimulationLog
{
private:
    std::vector<LogEntry> entries;
    std::size_t nextSequence;

public:
    SimulationLog();
    void add(LogSeverity severity, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void clear();
    const std::vector<LogEntry>& getEntries() const;
};

enum class DRCSeverity
{
    Info,
    Warning,
    Error
};

struct DRCFinding
{
    DRCSeverity severity;
    std::string ruleCode;
    std::string message;
    std::vector<int> componentIDs;
    std::vector<int> netIDs;
    int pinID;
};

struct CircuitConnection
{
    int componentID;
    int pinID;
    int netID;
};

class DRCChecker
{
public:
    std::vector<DRCFinding> check(
        const std::vector<const Component*>& components,
        const std::vector<CircuitConnection>& connections,
        SimulationLog* log = nullptr
    ) const;
};

#endif
