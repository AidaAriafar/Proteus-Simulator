#ifndef KIARASH_STARTUP_SERVICES_H
#define KIARASH_STARTUP_SERVICES_H

#include "canvas/CanvasServices.h"
#include "common/ProjectData.h"
#include "common/Result.h"
#include "persistence/PersistenceServices.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kiarash
{

struct NewProjectRequest
{
    std::string projectName;
    std::optional<std::string> presetID;
    std::optional<Size> customSize;
    std::optional<std::filesystem::path> saveLocation;
};

struct RecentProjectEntry
{
    std::string projectName;
    std::filesystem::path path;
    Timestamp lastOpenedOrModified{};
};

class IStartMenuView
{
public:
    virtual ~IStartMenuView() = default;
    virtual void showError(const std::string& message) = 0;
    virtual void showRecentProjects(const std::vector<RecentProjectEntry>& entries) = 0;
    virtual void openDocument(const ProjectDocumentData& document) = 0;
};

class RecentProjectsService
{
private:
    std::filesystem::path storagePath_;
    std::vector<RecentProjectEntry> entries_;
    std::size_t maxEntries_{5};

public:
    explicit RecentProjectsService(std::filesystem::path storagePath);
    void addOrUpdate(const RecentProjectEntry& entry);
    void removeMissingFiles();
    const std::vector<RecentProjectEntry>& entries() const;
    Result<void> load();
    Result<void> save() const;
};

class ProjectCreationService
{
private:
    CanvasPresetCatalog presetCatalog_;

public:
    Result<ProjectDocumentData> create(const NewProjectRequest& request) const;
};

class StartMenuController
{
private:
    IStartMenuView& view_;
    ProjectCreationService& creationService_;
    RecentProjectsService& recentProjects_;
    ProjectFileService& fileService_;

public:
    StartMenuController(IStartMenuView& view, ProjectCreationService& creationService, RecentProjectsService& recentProjects, ProjectFileService& fileService);
    Result<ProjectDocumentData> newProject(const NewProjectRequest& request);
    Result<ProjectDocumentData> openProject(const std::filesystem::path& path);
    void refreshRecentProjects();
};

}

#endif
