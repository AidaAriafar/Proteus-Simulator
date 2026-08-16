#ifndef KIARASH_COMMON_PROJECT_DATA_H
#define KIARASH_COMMON_PROJECT_DATA_H

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kiarash
{

using Timestamp = std::chrono::system_clock::time_point;

struct Point
{
    double x{0.0};
    double y{0.0};
};

struct Size
{
    double width{0.0};
    double height{0.0};
};

struct GridSettingsData
{
    double gridSpacing{20.0};
    bool gridVisible{true};
    bool snapEnabled{true};
    double gridOpacity{0.35};
};

struct CanvasSettingsData
{
    Size size{1600.0, 1000.0};
    std::string presetName;
    double gridSpacing{20.0};
    bool gridVisible{true};
    bool snapEnabled{true};
    double gridOpacity{0.35};
};

struct ViewportStateData
{
    double zoom{1.0};
    Point offset{0.0, 0.0};
};

struct ProjectMetadataData
{
    std::string id;
    std::string name;
    Timestamp createdAt{};
    Timestamp lastModifiedAt{};
    std::optional<std::string> filePath;
    bool dirty{true};
};

struct ComponentRecordData
{
    std::string id;
    std::string typeName;
    std::string displayName;
    Point position;
    int rotationDegrees{0};
    bool mirroredHorizontally{false};
    bool mirroredVertically{false};
    std::map<std::string, std::string> properties;
};

struct ConnectionRecordData
{
    std::string id;
    std::string fromComponentID;
    std::string fromPinID;
    std::string toComponentID;
    std::string toPinID;
    std::map<std::string, std::string> metadata;
};

struct ActiveLibraryItemData
{
    std::string descriptorID;
};

struct SimulationSnapshotData
{
    std::string providerName;
    std::string payload;
};

struct ProjectDocumentData
{
    ProjectMetadataData metadata;
    CanvasSettingsData canvas;
    ViewportStateData viewport;
    std::vector<ComponentRecordData> components;
    std::vector<ConnectionRecordData> connections;
    std::vector<ActiveLibraryItemData> activeLibraryItems;
    std::optional<SimulationSnapshotData> simulationSnapshot;
};

std::string generateID(const std::string& prefix);
Timestamp now();
std::string timestampToString(Timestamp timestamp);
Timestamp timestampFromString(const std::string& text);

}

#endif
