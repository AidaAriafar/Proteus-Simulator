#include "DRCChecker.h"

#include "../components/Sources.h"

#include <algorithm>
#include <map>
#include <set>

SimulationLog::SimulationLog()
:
nextSequence(0)
{
}

void SimulationLog::add(LogSeverity severity, const std::string& message)
{
    entries.push_back({nextSequence++, severity, message});
}

void SimulationLog::info(const std::string& message)
{
    add(LogSeverity::Info, message);
}

void SimulationLog::warning(const std::string& message)
{
    add(LogSeverity::Warning, message);
}

void SimulationLog::error(const std::string& message)
{
    add(LogSeverity::Error, message);
}

void SimulationLog::clear()
{
    entries.clear();
    nextSequence = 0;
}

const std::vector<LogEntry>& SimulationLog::getEntries() const
{
    return entries;
}

namespace
{
bool isVoltageSource(const Component& component)
{
    return dynamic_cast<const DCVoltageSource*>(&component) != nullptr ||
           dynamic_cast<const Battery*>(&component) != nullptr ||
           dynamic_cast<const PulseSource*>(&component) != nullptr;
}

bool isDrivenSourcePin(const Component& component, int pinID)
{
    return isVoltageSource(component) && pinID == 1;
}

bool isGround(const Component& component)
{
    return dynamic_cast<const GND*>(&component) != nullptr;
}

LogSeverity toLogSeverity(DRCSeverity severity)
{
    if(severity == DRCSeverity::Error) return LogSeverity::Error;
    if(severity == DRCSeverity::Warning) return LogSeverity::Warning;
    return LogSeverity::Info;
}
}

std::vector<DRCFinding> DRCChecker::check(
    const std::vector<const Component*>& components,
    const std::vector<CircuitConnection>& connections,
    SimulationLog* log
) const
{
    std::vector<DRCFinding> findings;
    std::map<int, std::vector<CircuitConnection>> byNet;
    std::set<std::pair<int, int>> connectedPins;

    for(const auto& connection : connections)
    {
        byNet[connection.netID].push_back(connection);
        connectedPins.insert({connection.componentID, connection.pinID});
    }

    for(const auto& entry : byNet)
    {
        std::vector<int> voltageSources;
        std::vector<int> grounds;

        for(const auto& connection : entry.second)
        {
            const auto componentIt = std::find_if(
                components.begin(),
                components.end(),
                [&connection](const Component* component)
                {
                    return component != nullptr && component->getID() == connection.componentID;
                });

            if(componentIt == components.end())
            {
                continue;
            }

            if(isDrivenSourcePin(**componentIt, connection.pinID))
            {
                voltageSources.push_back((*componentIt)->getID());
            }
            if(isGround(**componentIt))
            {
                grounds.push_back((*componentIt)->getID());
            }
        }

        if(!voltageSources.empty() && !grounds.empty())
        {
            DRCFinding finding;
            finding.severity = DRCSeverity::Error;
            finding.ruleCode = "DRC_SHORT_SOURCE_GND";
            finding.message = "Direct voltage source to GND connection detected.";
            finding.componentIDs = voltageSources;
            finding.componentIDs.insert(finding.componentIDs.end(), grounds.begin(), grounds.end());
            finding.netIDs = {entry.first};
            finding.pinID = -1;
            findings.push_back(finding);
        }
    }

    for(const Component* component : components)
    {
        if(component == nullptr)
        {
            continue;
        }

        for(const Pin& pin : component->getPins())
        {
            const bool connected = pin.isConnected() ||
                connectedPins.count({component->getID(), pin.getID()}) != 0;

            if(pin.isRequired() && !connected)
            {
                DRCFinding finding;
                finding.severity = DRCSeverity::Warning;
                finding.ruleCode = "DRC_FLOATING_PIN";
                finding.message = component->getType() + " " + component->getLabel() + " pin " + pin.getName() + " is floating.";
                finding.componentIDs = {component->getID()};
                finding.pinID = pin.getID();
                findings.push_back(finding);
            }
        }
    }

    if(log != nullptr)
    {
        if(findings.empty())
        {
            log->info("DRC completed with no findings.");
        }
        for(const auto& finding : findings)
        {
            log->add(toLogSeverity(finding.severity), finding.ruleCode + ": " + finding.message);
        }
    }

    return findings;
}
