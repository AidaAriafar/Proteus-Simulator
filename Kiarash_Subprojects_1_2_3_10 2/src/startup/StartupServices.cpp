#include "startup/StartupServices.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace kiarash
{

RecentProjectsService::RecentProjectsService(std::filesystem::path storagePath)
:
storagePath_(std::move(storagePath))
{
}

void RecentProjectsService::addOrUpdate(const RecentProjectEntry& entry)
{
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&entry](const RecentProjectEntry& existing)
    {
        return std::filesystem::absolute(existing.path) == std::filesystem::absolute(entry.path);
    }), entries_.end());
    entries_.insert(entries_.begin(), entry);
    if(entries_.size() > maxEntries_) entries_.resize(maxEntries_);
}

void RecentProjectsService::removeMissingFiles()
{
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [](const RecentProjectEntry& entry)
    {
        return !std::filesystem::exists(entry.path);
    }), entries_.end());
}

const std::vector<RecentProjectEntry>& RecentProjectsService::entries() const
{
    return entries_;
}

Result<void> RecentProjectsService::load()
{
    entries_.clear();
    if(!std::filesystem::exists(storagePath_)) return Result<void>::success();
    std::ifstream input(storagePath_);
    if(!input) return Result<void>::failure(ErrorCode::FileUnreadable, "Recent projects file is unreadable.");
    std::string line;
    while(std::getline(input, line))
    {
        std::istringstream stream(line);
        std::string name;
        std::string path;
        std::string timestamp;
        if(std::getline(stream, name, '\t') && std::getline(stream, path, '\t') && std::getline(stream, timestamp))
        {
            addOrUpdate({name, path, timestampFromString(timestamp)});
        }
    }
    removeMissingFiles();
    return Result<void>::success();
}

Result<void> RecentProjectsService::save() const
{
    std::ofstream output(storagePath_);
    if(!output) return Result<void>::failure(ErrorCode::FileWriteFailed, "Recent projects file is not writable.");
    for(const auto& entry : entries_)
    {
        output << entry.projectName << '\t' << entry.path.string() << '\t' << timestampToString(entry.lastOpenedOrModified) << '\n';
    }
    return Result<void>::success();
}

Result<ProjectDocumentData> ProjectCreationService::create(const NewProjectRequest& request) const
{
    if(request.projectName.empty())
    {
        return Result<ProjectDocumentData>::failure(ErrorCode::InvalidArgument, "Project name cannot be empty.");
    }

    Size size;
    std::string presetName;
    if(request.presetID)
    {
        const auto preset = presetCatalog_.find(*request.presetID);
        if(!preset) return Result<ProjectDocumentData>::failure(ErrorCode::InvalidArgument, "Unknown canvas preset.");
        size = preset->sceneSize;
        presetName = preset->id;
        if(request.customSize)
        {
            const auto validation = presetCatalog_.validateSize(*request.customSize);
            if(!validation.ok()) return Result<ProjectDocumentData>::failure(validation.code(), validation.message());
        }
    }
    else if(request.customSize)
    {
        const auto validation = presetCatalog_.validateSize(*request.customSize);
        if(!validation.ok()) return Result<ProjectDocumentData>::failure(validation.code(), validation.message());
        size = validation.value();
    }
    else
    {
        size = Size{1600.0, 1000.0};
    }

    ProjectDocumentData document;
    document.metadata.id = generateID("project");
    document.metadata.name = request.projectName;
    document.metadata.createdAt = now();
    document.metadata.lastModifiedAt = document.metadata.createdAt;
    document.metadata.filePath = request.saveLocation ? std::optional<std::string>(request.saveLocation->string()) : std::nullopt;
    document.metadata.dirty = true;
    document.canvas.size = size;
    document.canvas.presetName = presetName;
    document.viewport.zoom = 1.0;
    document.viewport.offset = {0.0, 0.0};
    return Result<ProjectDocumentData>::success(document);
}

StartMenuController::StartMenuController(IStartMenuView& view, ProjectCreationService& creationService, RecentProjectsService& recentProjects, ProjectFileService& fileService)
:
view_(view),
creationService_(creationService),
recentProjects_(recentProjects),
fileService_(fileService)
{
}

Result<ProjectDocumentData> StartMenuController::newProject(const NewProjectRequest& request)
{
    auto result = creationService_.create(request);
    if(!result.ok())
    {
        view_.showError(result.message());
        return result;
    }
    view_.openDocument(result.value());
    return result;
}

Result<ProjectDocumentData> StartMenuController::openProject(const std::filesystem::path& path)
{
    auto result = fileService_.loadFrom(path);
    if(!result.ok())
    {
        view_.showError(result.message());
        return result;
    }
    recentProjects_.addOrUpdate({result.value().metadata.name, path, now()});
    recentProjects_.save();
    view_.openDocument(result.value());
    return result;
}

void StartMenuController::refreshRecentProjects()
{
    recentProjects_.removeMissingFiles();
    view_.showRecentProjects(recentProjects_.entries());
}

}
