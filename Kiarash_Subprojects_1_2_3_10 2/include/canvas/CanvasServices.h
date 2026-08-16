#ifndef KIARASH_CANVAS_SERVICES_H
#define KIARASH_CANVAS_SERVICES_H

#include "common/ProjectData.h"
#include "common/Result.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kiarash
{

struct CanvasPreset
{
    std::string id;
    std::string displayName;
    Size sceneSize;
};

class CanvasPresetCatalog
{
private:
    std::map<std::string, CanvasPreset> presets_;

public:
    CanvasPresetCatalog();
    std::optional<CanvasPreset> find(const std::string& id) const;
    std::vector<CanvasPreset> all() const;
    Result<Size> validateSize(Size size) const;
};

class GridModel
{
private:
    GridSettingsData settings_;

public:
    GridModel() = default;
    explicit GridModel(const GridSettingsData& settings);
    static Result<GridModel> create(const GridSettingsData& settings);
    const GridSettingsData& settings() const;
    Result<void> setSpacing(double spacing);
    void setVisible(bool visible);
    void setSnapEnabled(bool enabled);
    Result<void> setOpacity(double opacity);
};

class SnapService
{
private:
    GridSettingsData settings_;

public:
    explicit SnapService(const GridSettingsData& settings);
    void updateSettings(const GridSettingsData& settings);
    Point snap(const Point& scenePoint) const;
};

class ViewportModel
{
private:
    ViewportStateData state_;
    Size canvasSize_;
    Size viewSize_;
    double minZoom_{0.10};
    double maxZoom_{5.00};
    double defaultZoom_{1.00};
    Point clampOffset(Point offset) const;

public:
    ViewportModel(Size canvasSize, Size viewSize);
    const ViewportStateData& state() const;
    void setViewSize(Size viewSize);
    void setCanvasSize(Size canvasSize);
    void zoomBy(double factor, Point screenAnchor);
    void zoomIn(Point screenAnchor);
    void zoomOut(Point screenAnchor);
    void resetZoom();
    void fitToCanvas();
    void panBy(double screenDeltaX, double screenDeltaY);
    Point viewToScene(Point screenPoint) const;
    Point sceneToView(Point scenePoint) const;
    double zoomPercent() const;
};

class CoordinateFormatter
{
private:
    int precision_;

public:
    explicit CoordinateFormatter(int precision = 2);
    std::string format(Point scenePoint) const;
};

struct StatusBarModel
{
    std::string coordinatesText;
    std::string zoomText{"100%"};
    std::string message;
};

struct ToolbarAction
{
    std::string id;
    std::string displayName;
    bool enabled{true};
};

class KeyboardShortcutRegistry
{
private:
    std::map<std::string, std::string> commandByShortcut_;

public:
    void registerShortcut(const std::string& shortcut, const std::string& commandID);
    std::optional<std::string> commandFor(const std::string& shortcut) const;
};

}

#endif
