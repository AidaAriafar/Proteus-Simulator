#ifndef KIARASH_PERSISTENCE_SERVICES_H
#define KIARASH_PERSISTENCE_SERVICES_H

#include "common/ProjectData.h"
#include "common/Result.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kiarash
{

class IComponentPersistenceAdapter
{
public:
    virtual ~IComponentPersistenceAdapter() = default;
    virtual std::vector<ComponentRecordData> componentRecords() const = 0;
};

class IConnectionPersistenceAdapter
{
public:
    virtual ~IConnectionPersistenceAdapter() = default;
    virtual std::vector<ConnectionRecordData> connectionRecords() const = 0;
};

class ISimulationStateSnapshotProvider
{
public:
    virtual ~ISimulationStateSnapshotProvider() = default;
    virtual std::optional<SimulationSnapshotData> snapshot() const = 0;
};

class ProjectSerializer
{
public:
    std::string serialize(const ProjectDocumentData& document) const;
};

class ProjectDeserializer
{
public:
    Result<ProjectDocumentData> deserialize(const std::string& text) const;
};

class ProjectFileService
{
private:
    ProjectSerializer serializer_;
    ProjectDeserializer deserializer_;

public:
    Result<void> saveTo(const ProjectDocumentData& document, const std::filesystem::path& path) const;
    Result<ProjectDocumentData> loadFrom(const std::filesystem::path& path) const;
    static std::filesystem::path normalizeProjectPath(std::filesystem::path path);
};

enum class UnsavedDecision
{
    Save,
    Discard,
    Cancel
};

class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual Result<void> execute() = 0;
    virtual Result<void> undo() = 0;
    virtual Result<void> redo();
    virtual std::string description() const = 0;
};

class CommandHistory
{
private:
    std::vector<std::unique_ptr<ICommand>> undoStack_;
    std::vector<std::unique_ptr<ICommand>> redoStack_;
    std::size_t maxHistorySize_;

public:
    explicit CommandHistory(std::size_t maxHistorySize = 100);
    Result<void> execute(std::unique_ptr<ICommand> command);
    Result<void> undo();
    Result<void> redo();
    bool canUndo() const;
    bool canRedo() const;
    void clear();
    std::optional<std::string> undoDescription() const;
    std::optional<std::string> redoDescription() const;
};

class SetProjectNameCommand : public ICommand
{
private:
    ProjectDocumentData& document_;
    std::string newName_;
    std::string oldName_;

public:
    SetProjectNameCommand(ProjectDocumentData& document, std::string newName);
    Result<void> execute() override;
    Result<void> undo() override;
    std::string description() const override;
};

class ChangeGridSettingsCommand : public ICommand
{
private:
    ProjectDocumentData& document_;
    GridSettingsData newSettings_;
    GridSettingsData oldSettings_;

public:
    ChangeGridSettingsCommand(ProjectDocumentData& document, GridSettingsData newSettings);
    Result<void> execute() override;
    Result<void> undo() override;
    std::string description() const override;
};

class ChangeCanvasSizeCommand : public ICommand
{
private:
    ProjectDocumentData& document_;
    Size newSize_;
    Size oldSize_;

public:
    ChangeCanvasSizeCommand(ProjectDocumentData& document, Size newSize);
    Result<void> execute() override;
    Result<void> undo() override;
    std::string description() const override;
};

class ChangeViewportCommand : public ICommand
{
private:
    ProjectDocumentData& document_;
    ViewportStateData newState_;
    ViewportStateData oldState_;

public:
    ChangeViewportCommand(ProjectDocumentData& document, ViewportStateData newState);
    Result<void> execute() override;
    Result<void> undo() override;
    std::string description() const override;
};

class AddActiveLibraryItemCommand : public ICommand
{
private:
    ProjectDocumentData& document_;
    std::string descriptorID_;
    bool added_{false};

public:
    AddActiveLibraryItemCommand(ProjectDocumentData& document, std::string descriptorID);
    Result<void> execute() override;
    Result<void> undo() override;
    std::string description() const override;
};

class RemoveActiveLibraryItemCommand : public ICommand
{
private:
    ProjectDocumentData& document_;
    std::string descriptorID_;
    std::optional<std::size_t> removedIndex_;

public:
    RemoveActiveLibraryItemCommand(ProjectDocumentData& document, std::string descriptorID);
    Result<void> execute() override;
    Result<void> undo() override;
    std::string description() const override;
};

struct ExportOptions
{
    Size imageSize{800, 600};
    bool includeGrid{true};
    bool includeBackground{true};
};

struct RenderedImage
{
    std::vector<unsigned char> bytes;
    std::string format;
};

class ICanvasImageRenderer
{
public:
    virtual ~ICanvasImageRenderer() = default;
    virtual Result<RenderedImage> renderCanvas(const ProjectDocumentData& document, const ExportOptions& options, const std::string& format) = 0;
};

class ImageExportService
{
private:
    ICanvasImageRenderer& renderer_;

public:
    explicit ImageExportService(ICanvasImageRenderer& renderer);
    Result<void> exportTo(const ProjectDocumentData& document, const std::filesystem::path& outputPath, const ExportOptions& options);
    static std::optional<std::string> formatFromPath(const std::filesystem::path& path);
};

class ProjectCommands
{
private:
    ProjectFileService& fileService_;
    std::function<std::optional<std::filesystem::path>()> chooseSavePath_;
    std::function<bool(const std::filesystem::path&)> confirmOverwrite_;
    std::function<UnsavedDecision()> decideUnsaved_;
    std::function<void(const std::string&, const std::filesystem::path&, Timestamp)> recordRecentProject_;

public:
    explicit ProjectCommands(ProjectFileService& fileService);
    void setChooseSavePath(std::function<std::optional<std::filesystem::path>()> callback);
    void setConfirmOverwrite(std::function<bool(const std::filesystem::path&)> callback);
    void setDecideUnsaved(std::function<UnsavedDecision()> callback);
    void setRecentProjectRecorder(std::function<void(const std::string&, const std::filesystem::path&, Timestamp)> callback);
    Result<void> save(ProjectDocumentData& document);
    Result<void> saveAs(ProjectDocumentData& document, const std::filesystem::path& path);
    Result<ProjectDocumentData> open(ProjectDocumentData* currentDocument, const std::filesystem::path& path);
};

}

#endif
