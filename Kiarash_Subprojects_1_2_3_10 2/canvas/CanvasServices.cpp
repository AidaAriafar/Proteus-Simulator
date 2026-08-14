#include "canvas/CanvasServices.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace kiarash
{

CanvasPresetCatalog::CanvasPresetCatalog()
{
    presets_.emplace("A4", CanvasPreset{"A4", "A4", Size{297.0, 210.0}});
    presets_.emplace("A3", CanvasPreset{"A3", "A3", Size{420.0, 297.0}});
    presets_.emplace("HD", CanvasPreset{"HD", "HD Canvas", Size{1920.0, 1080.0}});
}

std::optional<CanvasPreset> CanvasPresetCatalog::find(const std::string& id) const
{
    const auto found = presets_.find(id);
    if(found == presets_.end()) return std::nullopt;
    return found->second;
}

std::vector<CanvasPreset> CanvasPresetCatalog::all() const
{
    std::vector<CanvasPreset> result;
    for(const auto& item : presets_) result.push_back(item.second);
    return result;
}

Result<Size> CanvasPresetCatalog::validateSize(Size size) const
{
    if(size.width <= 0 || size.height <= 0)
    {
        return Result<Size>::failure(ErrorCode::InvalidArgument, "Canvas dimensions must be positive.");
    }
    if(size.width < 50 || size.height < 50)
    {
        return Result<Size>::failure(ErrorCode::InvalidArgument, "Canvas dimensions are below the minimum of 50 scene units.");
    }
    if(size.width > 100000 || size.height > 100000)
    {
        return Result<Size>::failure(ErrorCode::InvalidArgument, "Canvas dimensions exceed the maximum of 100000 scene units.");
    }
    return Result<Size>::success(size);
}

GridModel::GridModel(const GridSettingsData& settings)
:
settings_(settings)
{
}

Result<GridModel> GridModel::create(const GridSettingsData& settings)
{
    GridModel model(settings);
    auto spacingResult = model.setSpacing(settings.gridSpacing);
    if(!spacingResult.ok()) return Result<GridModel>::failure(spacingResult.code(), spacingResult.message());
    auto opacityResult = model.setOpacity(settings.gridOpacity);
    if(!opacityResult.ok()) return Result<GridModel>::failure(opacityResult.code(), opacityResult.message());
    model.settings_.gridVisible = settings.gridVisible;
    model.settings_.snapEnabled = settings.snapEnabled;
    return Result<GridModel>::success(model);
}

const GridSettingsData& GridModel::settings() const
{
    return settings_;
}

Result<void> GridModel::setSpacing(double spacing)
{
    if(spacing <= 0)
    {
        return Result<void>::failure(ErrorCode::InvalidArgument, "Grid spacing must be positive.");
    }
    settings_.gridSpacing = spacing;
    return Result<void>::success();
}

void GridModel::setVisible(bool visible)
{
    settings_.gridVisible = visible;
}

void GridModel::setSnapEnabled(bool enabled)
{
    settings_.snapEnabled = enabled;
}

Result<void> GridModel::setOpacity(double opacity)
{
    if(opacity < 0.0 || opacity > 1.0)
    {
        return Result<void>::failure(ErrorCode::InvalidArgument, "Grid opacity must be between 0 and 1.");
    }
    settings_.gridOpacity = opacity;
    return Result<void>::success();
}

SnapService::SnapService(const GridSettingsData& settings)
:
settings_(settings)
{
}

void SnapService::updateSettings(const GridSettingsData& settings)
{
    settings_ = settings;
}

Point SnapService::snap(const Point& scenePoint) const
{
    if(!settings_.snapEnabled) return scenePoint;
    const double spacing = settings_.gridSpacing;
    return Point{
        std::round(scenePoint.x / spacing) * spacing,
        std::round(scenePoint.y / spacing) * spacing
    };
}

ViewportModel::ViewportModel(Size canvasSize, Size viewSize)
:
canvasSize_(canvasSize),
viewSize_(viewSize)
{
    state_.zoom = defaultZoom_;
    state_.offset = {0.0, 0.0};
}

const ViewportStateData& ViewportModel::state() const
{
    return state_;
}

void ViewportModel::setViewSize(Size viewSize)
{
    viewSize_ = viewSize;
    state_.offset = clampOffset(state_.offset);
}

void ViewportModel::setCanvasSize(Size canvasSize)
{
    canvasSize_ = canvasSize;
    state_.offset = clampOffset(state_.offset);
}

void ViewportModel::zoomBy(double factor, Point screenAnchor)
{
    const Point before = viewToScene(screenAnchor);
    state_.zoom = std::max(minZoom_, std::min(maxZoom_, state_.zoom * factor));
    state_.offset = Point{
        before.x - screenAnchor.x / state_.zoom,
        before.y - screenAnchor.y / state_.zoom
    };
    state_.offset = clampOffset(state_.offset);
}

void ViewportModel::zoomIn(Point screenAnchor)
{
    zoomBy(1.25, screenAnchor);
}

void ViewportModel::zoomOut(Point screenAnchor)
{
    zoomBy(0.80, screenAnchor);
}

void ViewportModel::resetZoom()
{
    state_.zoom = defaultZoom_;
    state_.offset = clampOffset(state_.offset);
}

void ViewportModel::fitToCanvas()
{
    if(canvasSize_.width <= 0 || canvasSize_.height <= 0) return;
    const double zoomX = viewSize_.width / canvasSize_.width;
    const double zoomY = viewSize_.height / canvasSize_.height;
    state_.zoom = std::max(minZoom_, std::min(maxZoom_, std::min(zoomX, zoomY)));
    state_.offset = {0.0, 0.0};
}

void ViewportModel::panBy(double screenDeltaX, double screenDeltaY)
{
    state_.offset = clampOffset(Point{
        state_.offset.x - screenDeltaX / state_.zoom,
        state_.offset.y - screenDeltaY / state_.zoom
    });
}

Point ViewportModel::viewToScene(Point screenPoint) const
{
    return Point{
        state_.offset.x + screenPoint.x / state_.zoom,
        state_.offset.y + screenPoint.y / state_.zoom
    };
}

Point ViewportModel::sceneToView(Point scenePoint) const
{
    return Point{
        (scenePoint.x - state_.offset.x) * state_.zoom,
        (scenePoint.y - state_.offset.y) * state_.zoom
    };
}

double ViewportModel::zoomPercent() const
{
    return state_.zoom * 100.0;
}

Point ViewportModel::clampOffset(Point offset) const
{
    const double maxX = std::max(0.0, canvasSize_.width - viewSize_.width / state_.zoom);
    const double maxY = std::max(0.0, canvasSize_.height - viewSize_.height / state_.zoom);
    return Point{
        std::max(0.0, std::min(maxX, offset.x)),
        std::max(0.0, std::min(maxY, offset.y))
    };
}

CoordinateFormatter::CoordinateFormatter(int precision)
:
precision_(precision)
{
}

std::string CoordinateFormatter::format(Point scenePoint) const
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision_)
           << "X: " << scenePoint.x << " Y: " << scenePoint.y;
    return stream.str();
}

void KeyboardShortcutRegistry::registerShortcut(const std::string& shortcut, const std::string& commandID)
{
    commandByShortcut_[shortcut] = commandID;
}

std::optional<std::string> KeyboardShortcutRegistry::commandFor(const std::string& shortcut) const
{
    const auto found = commandByShortcut_.find(shortcut);
    if(found == commandByShortcut_.end()) return std::nullopt;
    return found->second;
}

}
