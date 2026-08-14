#include "persistence/PersistenceServices.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace kiarash
{
namespace
{
std::string escapeJson(const std::string& text)
{
    std::string result;
    for(char ch : text)
    {
        if(ch == '\\') result += "\\\\";
        else if(ch == '"') result += "\\\"";
        else if(ch == '\n') result += "\\n";
        else result.push_back(ch);
    }
    return result;
}

std::string quote(const std::string& text)
{
    return "\"" + escapeJson(text) + "\"";
}

std::string findString(const std::string& text, const std::string& key)
{
    const std::string marker = "\"" + key + "\":";
    const auto start = text.find(marker);
    if(start == std::string::npos) return "";
    auto quoteStart = text.find('"', start + marker.size());
    if(quoteStart == std::string::npos) return "";
    ++quoteStart;
    std::string value;
    bool escaping = false;
    for(std::size_t i = quoteStart; i < text.size(); ++i)
    {
        const char ch = text[i];
        if(escaping)
        {
            value.push_back(ch == 'n' ? '\n' : ch);
            escaping = false;
        }
        else if(ch == '\\') escaping = true;
        else if(ch == '"') return value;
        else value.push_back(ch);
    }
    return "";
}

double findNumber(const std::string& text, const std::string& key, double fallback = 0.0)
{
    const std::string marker = "\"" + key + "\":";
    const auto start = text.find(marker);
    if(start == std::string::npos) return fallback;
    const auto valueStart = start + marker.size();
    return std::stod(text.substr(valueStart));
}

bool findBool(const std::string& text, const std::string& key)
{
    const std::string marker = "\"" + key + "\":";
    const auto start = text.find(marker);
    if(start == std::string::npos) return false;
    return text.substr(start + marker.size(), 4) == "true";
}

std::string findObject(const std::string& text, const std::string& key)
{
    const std::string marker = "\"" + key + "\":";
    const auto markerStart = text.find(marker);
    if(markerStart == std::string::npos) return "";
    const auto open = text.find('{', markerStart + marker.size());
    if(open == std::string::npos) return "";
    int depth = 0;
    for(std::size_t i = open; i < text.size(); ++i)
    {
        if(text[i] == '{') ++depth;
        else if(text[i] == '}')
        {
            --depth;
            if(depth == 0) return text.substr(open, i - open + 1);
        }
    }
    return "";
}

std::string findArray(const std::string& text, const std::string& key)
{
    const std::string marker = "\"" + key + "\":";
    const auto markerStart = text.find(marker);
    if(markerStart == std::string::npos) return "";
    const auto open = text.find('[', markerStart + marker.size());
    if(open == std::string::npos) return "";
    int depth = 0;
    for(std::size_t i = open; i < text.size(); ++i)
    {
        if(text[i] == '[') ++depth;
        else if(text[i] == ']')
        {
            --depth;
            if(depth == 0) return text.substr(open, i - open + 1);
        }
    }
    return "";
}

std::vector<std::string> objectsInArray(const std::string& arrayText)
{
    std::vector<std::string> result;
    for(std::size_t i = 0; i < arrayText.size(); ++i)
    {
        if(arrayText[i] != '{') continue;
        int depth = 0;
        for(std::size_t j = i; j < arrayText.size(); ++j)
        {
            if(arrayText[j] == '{') ++depth;
            else if(arrayText[j] == '}')
            {
                --depth;
                if(depth == 0)
                {
                    result.push_back(arrayText.substr(i, j - i + 1));
                    i = j;
                    break;
                }
            }
        }
    }
    return result;
}

std::map<std::string, std::string> parseStringMap(const std::string& objectText)
{
    std::map<std::string, std::string> result;
    std::size_t position = 0;
    while(true)
    {
        const auto keyStart = objectText.find('"', position);
        if(keyStart == std::string::npos) break;
        const auto keyEnd = objectText.find('"', keyStart + 1);
        if(keyEnd == std::string::npos) break;
        const auto valueStart = objectText.find('"', objectText.find(':', keyEnd) + 1);
        if(valueStart == std::string::npos) break;
        const auto valueEnd = objectText.find('"', valueStart + 1);
        if(valueEnd == std::string::npos) break;
        result[objectText.substr(keyStart + 1, keyEnd - keyStart - 1)] = objectText.substr(valueStart + 1, valueEnd - valueStart - 1);
        position = valueEnd + 1;
    }
    return result;
}

void appendStringMap(std::ostringstream& stream, const std::map<std::string, std::string>& values)
{
    stream << "{";
    bool first = true;
    for(const auto& item : values)
    {
        if(!first) stream << ",";
        first = false;
        stream << quote(item.first) << ":" << quote(item.second);
    }
    stream << "}";
}

Result<void> validateDocument(const ProjectDocumentData& document)
{
    if(document.metadata.id.empty()) return Result<void>::failure(ErrorCode::InvalidFormat, "Project identifier is missing.");
    if(document.metadata.name.empty()) return Result<void>::failure(ErrorCode::InvalidFormat, "Project name is missing.");
    if(document.canvas.size.width <= 0 || document.canvas.size.height <= 0)
    {
        return Result<void>::failure(ErrorCode::InvalidFormat, "Canvas size is invalid.");
    }
    std::set<std::string> componentIDs;
    for(const auto& component : document.components)
    {
        if(component.id.empty()) return Result<void>::failure(ErrorCode::InvalidFormat, "Component ID is missing.");
        if(!componentIDs.insert(component.id).second) return Result<void>::failure(ErrorCode::DuplicateID, "Duplicate component ID.");
    }
    std::set<std::string> connectionIDs;
    for(const auto& connection : document.connections)
    {
        if(!connectionIDs.insert(connection.id).second) return Result<void>::failure(ErrorCode::DuplicateID, "Duplicate connection ID.");
        if(componentIDs.count(connection.fromComponentID) == 0 || componentIDs.count(connection.toComponentID) == 0)
        {
            return Result<void>::failure(ErrorCode::MissingReference, "Connection references a missing component.");
        }
    }
    return Result<void>::success();
}
}

std::string ProjectSerializer::serialize(const ProjectDocumentData& document) const
{
    std::ostringstream stream;
    stream << "{\n";
    stream << "\"format\":\"ProteusSimulatorProject\",\n";
    stream << "\"formatVersion\":1,\n";
    stream << "\"applicationVersion\":\"kiarash-1.0\",\n";
    stream << "\"metadata\":{\"id\":" << quote(document.metadata.id)
           << ",\"name\":" << quote(document.metadata.name)
           << ",\"createdAt\":" << quote(timestampToString(document.metadata.createdAt))
           << ",\"lastModifiedAt\":" << quote(timestampToString(document.metadata.lastModifiedAt))
           << ",\"dirty\":" << (document.metadata.dirty ? "true" : "false") << "},\n";
    stream << "\"canvas\":{\"width\":" << document.canvas.size.width
           << ",\"height\":" << document.canvas.size.height
           << ",\"preset\":" << quote(document.canvas.presetName)
           << ",\"gridSpacing\":" << document.canvas.gridSpacing
           << ",\"gridVisible\":" << (document.canvas.gridVisible ? "true" : "false")
           << ",\"snapEnabled\":" << (document.canvas.snapEnabled ? "true" : "false")
           << ",\"gridOpacity\":" << document.canvas.gridOpacity << "},\n";
    stream << "\"viewport\":{\"zoom\":" << document.viewport.zoom
           << ",\"offsetX\":" << document.viewport.offset.x
           << ",\"offsetY\":" << document.viewport.offset.y << "},\n";
    stream << "\"components\":[";
    for(std::size_t i = 0; i < document.components.size(); ++i)
    {
        const auto& component = document.components[i];
        if(i != 0) stream << ",";
        stream << "{\"id\":" << quote(component.id)
               << ",\"typeName\":" << quote(component.typeName)
               << ",\"displayName\":" << quote(component.displayName)
               << ",\"x\":" << component.position.x
               << ",\"y\":" << component.position.y
               << ",\"rotation\":" << component.rotationDegrees
               << ",\"mirrorH\":" << (component.mirroredHorizontally ? "true" : "false")
               << ",\"mirrorV\":" << (component.mirroredVertically ? "true" : "false")
               << ",\"properties\":";
        appendStringMap(stream, component.properties);
        stream << "}";
    }
    stream << "],\n\"connections\":[";
    for(std::size_t i = 0; i < document.connections.size(); ++i)
    {
        const auto& connection = document.connections[i];
        if(i != 0) stream << ",";
        stream << "{\"id\":" << quote(connection.id)
               << ",\"fromComponentID\":" << quote(connection.fromComponentID)
               << ",\"fromPinID\":" << quote(connection.fromPinID)
               << ",\"toComponentID\":" << quote(connection.toComponentID)
               << ",\"toPinID\":" << quote(connection.toPinID)
               << ",\"metadata\":";
        appendStringMap(stream, connection.metadata);
        stream << "}";
    }
    stream << "],\n\"activeLibraryItems\":[";
    for(std::size_t i = 0; i < document.activeLibraryItems.size(); ++i)
    {
        if(i != 0) stream << ",";
        stream << "{\"descriptorID\":" << quote(document.activeLibraryItems[i].descriptorID) << "}";
    }
    stream << "]";
    if(document.simulationSnapshot)
    {
        stream << ",\n\"simulationSnapshot\":{\"providerName\":" << quote(document.simulationSnapshot->providerName)
               << ",\"payload\":" << quote(document.simulationSnapshot->payload) << "}";
    }
    stream << "\n}\n";
    return stream.str();
}

Result<ProjectDocumentData> ProjectDeserializer::deserialize(const std::string& text) const
{
    if(text.find('{') == std::string::npos || text.find("\"format\"") == std::string::npos)
    {
        return Result<ProjectDocumentData>::failure(ErrorCode::InvalidFormat, "Malformed or unsupported JSON.");
    }
    if(findString(text, "format") != "ProteusSimulatorProject")
    {
        return Result<ProjectDocumentData>::failure(ErrorCode::InvalidFormat, "Unsupported project format.");
    }
    if(static_cast<int>(findNumber(text, "formatVersion", 0)) != 1)
    {
        return Result<ProjectDocumentData>::failure(ErrorCode::UnsupportedVersion, "Unsupported project format version.");
    }

    ProjectDocumentData document;
    const std::string metadata = findObject(text, "metadata");
    document.metadata.id = findString(metadata, "id");
    document.metadata.name = findString(metadata, "name");
    document.metadata.createdAt = timestampFromString(findString(metadata, "createdAt"));
    document.metadata.lastModifiedAt = timestampFromString(findString(metadata, "lastModifiedAt"));
    document.metadata.dirty = findBool(metadata, "dirty");

    const std::string canvas = findObject(text, "canvas");
    document.canvas.size = {findNumber(canvas, "width"), findNumber(canvas, "height")};
    document.canvas.presetName = findString(canvas, "preset");
    document.canvas.gridSpacing = findNumber(canvas, "gridSpacing", 20.0);
    document.canvas.gridVisible = findBool(canvas, "gridVisible");
    document.canvas.snapEnabled = findBool(canvas, "snapEnabled");
    document.canvas.gridOpacity = findNumber(canvas, "gridOpacity", 0.35);

    const std::string viewport = findObject(text, "viewport");
    document.viewport.zoom = findNumber(viewport, "zoom", 1.0);
    document.viewport.offset = {findNumber(viewport, "offsetX"), findNumber(viewport, "offsetY")};

    for(const auto& object : objectsInArray(findArray(text, "components")))
    {
        ComponentRecordData component;
        component.id = findString(object, "id");
        component.typeName = findString(object, "typeName");
        component.displayName = findString(object, "displayName");
        component.position = {findNumber(object, "x"), findNumber(object, "y")};
        component.rotationDegrees = static_cast<int>(findNumber(object, "rotation"));
        component.mirroredHorizontally = findBool(object, "mirrorH");
        component.mirroredVertically = findBool(object, "mirrorV");
        component.properties = parseStringMap(findObject(object, "properties"));
        document.components.push_back(component);
    }

    for(const auto& object : objectsInArray(findArray(text, "connections")))
    {
        ConnectionRecordData connection;
        connection.id = findString(object, "id");
        connection.fromComponentID = findString(object, "fromComponentID");
        connection.fromPinID = findString(object, "fromPinID");
        connection.toComponentID = findString(object, "toComponentID");
        connection.toPinID = findString(object, "toPinID");
        connection.metadata = parseStringMap(findObject(object, "metadata"));
        document.connections.push_back(connection);
    }

    for(const auto& object : objectsInArray(findArray(text, "activeLibraryItems")))
    {
        document.activeLibraryItems.push_back({findString(object, "descriptorID")});
    }

    const std::string snapshot = findObject(text, "simulationSnapshot");
    if(!snapshot.empty())
    {
        document.simulationSnapshot = SimulationSnapshotData{findString(snapshot, "providerName"), findString(snapshot, "payload")};
    }

    const auto validation = validateDocument(document);
    if(!validation.ok()) return Result<ProjectDocumentData>::failure(validation.code(), validation.message());
    return Result<ProjectDocumentData>::success(document);
}

Result<void> ProjectFileService::saveTo(const ProjectDocumentData& document, const std::filesystem::path& path) const
{
    const auto normalized = normalizeProjectPath(path);
    const auto tempPath = normalized.string() + ".tmp";
    std::ofstream output(tempPath, std::ios::binary);
    if(!output) return Result<void>::failure(ErrorCode::FileWriteFailed, "Cannot open temporary project file for writing.");
    output << serializer_.serialize(document);
    output.close();
    if(!output) return Result<void>::failure(ErrorCode::FileWriteFailed, "Failed while writing project file.");
    std::error_code error;
    std::filesystem::rename(tempPath, normalized, error);
    if(error)
    {
        std::filesystem::remove(normalized, error);
        error.clear();
        std::filesystem::rename(tempPath, normalized, error);
        if(error) return Result<void>::failure(ErrorCode::FileWriteFailed, "Could not replace project file atomically.");
    }
    return Result<void>::success();
}

Result<ProjectDocumentData> ProjectFileService::loadFrom(const std::filesystem::path& path) const
{
    if(!std::filesystem::exists(path)) return Result<ProjectDocumentData>::failure(ErrorCode::FileNotFound, "Project file does not exist.");
    std::ifstream input(path, std::ios::binary);
    if(!input) return Result<ProjectDocumentData>::failure(ErrorCode::FileUnreadable, "Project file is unreadable.");
    std::ostringstream buffer;
    buffer << input.rdbuf();
    auto result = deserializer_.deserialize(buffer.str());
    if(result.ok()) result.value().metadata.filePath = path.string();
    return result;
}

std::filesystem::path ProjectFileService::normalizeProjectPath(std::filesystem::path path)
{
    if(path.extension() != ".proteusjson") path.replace_extension(".proteusjson");
    return path;
}

Result<void> ICommand::redo()
{
    return execute();
}

CommandHistory::CommandHistory(std::size_t maxHistorySize)
:
maxHistorySize_(maxHistorySize)
{
}

Result<void> CommandHistory::execute(std::unique_ptr<ICommand> command)
{
    if(!command) return Result<void>::failure(ErrorCode::InvalidArgument, "Command is null.");
    auto result = command->execute();
    if(!result.ok()) return result;
    redoStack_.clear();
    undoStack_.push_back(std::move(command));
    while(undoStack_.size() > maxHistorySize_) undoStack_.erase(undoStack_.begin());
    return Result<void>::success();
}

Result<void> CommandHistory::undo()
{
    if(!canUndo()) return Result<void>::failure(ErrorCode::InvalidArgument, "Nothing to undo.");
    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    auto result = command->undo();
    if(!result.ok()) return result;
    redoStack_.push_back(std::move(command));
    return Result<void>::success();
}

Result<void> CommandHistory::redo()
{
    if(!canRedo()) return Result<void>::failure(ErrorCode::InvalidArgument, "Nothing to redo.");
    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    auto result = command->redo();
    if(!result.ok()) return result;
    undoStack_.push_back(std::move(command));
    return Result<void>::success();
}

bool CommandHistory::canUndo() const { return !undoStack_.empty(); }
bool CommandHistory::canRedo() const { return !redoStack_.empty(); }
void CommandHistory::clear() { undoStack_.clear(); redoStack_.clear(); }

std::optional<std::string> CommandHistory::undoDescription() const
{
    if(!canUndo()) return std::nullopt;
    return undoStack_.back()->description();
}

std::optional<std::string> CommandHistory::redoDescription() const
{
    if(!canRedo()) return std::nullopt;
    return redoStack_.back()->description();
}

SetProjectNameCommand::SetProjectNameCommand(ProjectDocumentData& document, std::string newName)
:
document_(document),
newName_(std::move(newName))
{
}

Result<void> SetProjectNameCommand::execute()
{
    oldName_ = document_.metadata.name;
    document_.metadata.name = newName_;
    document_.metadata.dirty = true;
    return Result<void>::success();
}

Result<void> SetProjectNameCommand::undo()
{
    document_.metadata.name = oldName_;
    document_.metadata.dirty = true;
    return Result<void>::success();
}

std::string SetProjectNameCommand::description() const { return "Set project name"; }

ChangeGridSettingsCommand::ChangeGridSettingsCommand(ProjectDocumentData& document, GridSettingsData newSettings)
:
document_(document),
newSettings_(newSettings)
{
}

Result<void> ChangeGridSettingsCommand::execute()
{
    oldSettings_ = GridSettingsData{document_.canvas.gridSpacing, document_.canvas.gridVisible, document_.canvas.snapEnabled, document_.canvas.gridOpacity};
    document_.canvas.gridSpacing = newSettings_.gridSpacing;
    document_.canvas.gridVisible = newSettings_.gridVisible;
    document_.canvas.snapEnabled = newSettings_.snapEnabled;
    document_.canvas.gridOpacity = newSettings_.gridOpacity;
    document_.metadata.dirty = true;
    return Result<void>::success();
}

Result<void> ChangeGridSettingsCommand::undo()
{
    document_.canvas.gridSpacing = oldSettings_.gridSpacing;
    document_.canvas.gridVisible = oldSettings_.gridVisible;
    document_.canvas.snapEnabled = oldSettings_.snapEnabled;
    document_.canvas.gridOpacity = oldSettings_.gridOpacity;
    document_.metadata.dirty = true;
    return Result<void>::success();
}

std::string ChangeGridSettingsCommand::description() const { return "Change grid settings"; }

ChangeCanvasSizeCommand::ChangeCanvasSizeCommand(ProjectDocumentData& document, Size newSize)
:
document_(document),
newSize_(newSize)
{
}

Result<void> ChangeCanvasSizeCommand::execute()
{
    oldSize_ = document_.canvas.size;
    document_.canvas.size = newSize_;
    document_.metadata.dirty = true;
    return Result<void>::success();
}

Result<void> ChangeCanvasSizeCommand::undo()
{
    document_.canvas.size = oldSize_;
    document_.metadata.dirty = true;
    return Result<void>::success();
}

std::string ChangeCanvasSizeCommand::description() const { return "Change canvas size"; }

ChangeViewportCommand::ChangeViewportCommand(ProjectDocumentData& document, ViewportStateData newState)
:
document_(document),
newState_(newState)
{
}

Result<void> ChangeViewportCommand::execute()
{
    oldState_ = document_.viewport;
    document_.viewport = newState_;
    document_.metadata.dirty = true;
    return Result<void>::success();
}

Result<void> ChangeViewportCommand::undo()
{
    document_.viewport = oldState_;
    document_.metadata.dirty = true;
    return Result<void>::success();
}

std::string ChangeViewportCommand::description() const { return "Change viewport"; }

AddActiveLibraryItemCommand::AddActiveLibraryItemCommand(ProjectDocumentData& document, std::string descriptorID)
:
document_(document),
descriptorID_(std::move(descriptorID))
{
}

Result<void> AddActiveLibraryItemCommand::execute()
{
    const auto exists = std::find_if(document_.activeLibraryItems.begin(), document_.activeLibraryItems.end(), [this](const ActiveLibraryItemData& item)
    {
        return item.descriptorID == descriptorID_;
    }) != document_.activeLibraryItems.end();
    if(!exists)
    {
        document_.activeLibraryItems.push_back({descriptorID_});
        added_ = true;
    }
    document_.metadata.dirty = true;
    return Result<void>::success();
}

Result<void> AddActiveLibraryItemCommand::undo()
{
    if(added_)
    {
        document_.activeLibraryItems.erase(std::remove_if(document_.activeLibraryItems.begin(), document_.activeLibraryItems.end(), [this](const ActiveLibraryItemData& item)
        {
            return item.descriptorID == descriptorID_;
        }), document_.activeLibraryItems.end());
    }
    document_.metadata.dirty = true;
    return Result<void>::success();
}

std::string AddActiveLibraryItemCommand::description() const { return "Add active library item"; }

RemoveActiveLibraryItemCommand::RemoveActiveLibraryItemCommand(ProjectDocumentData& document, std::string descriptorID)
:
document_(document),
descriptorID_(std::move(descriptorID))
{
}

Result<void> RemoveActiveLibraryItemCommand::execute()
{
    removedIndex_.reset();
    for(std::size_t i = 0; i < document_.activeLibraryItems.size(); ++i)
    {
        if(document_.activeLibraryItems[i].descriptorID == descriptorID_)
        {
            removedIndex_ = i;
            document_.activeLibraryItems.erase(document_.activeLibraryItems.begin() + static_cast<long>(i));
            break;
        }
    }
    document_.metadata.dirty = true;
    return Result<void>::success();
}

Result<void> RemoveActiveLibraryItemCommand::undo()
{
    if(removedIndex_)
    {
        document_.activeLibraryItems.insert(document_.activeLibraryItems.begin() + static_cast<long>(*removedIndex_), {descriptorID_});
    }
    document_.metadata.dirty = true;
    return Result<void>::success();
}

std::string RemoveActiveLibraryItemCommand::description() const { return "Remove active library item"; }

ImageExportService::ImageExportService(ICanvasImageRenderer& renderer)
:
renderer_(renderer)
{
}

std::optional<std::string> ImageExportService::formatFromPath(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
    if(extension == ".png") return "png";
    if(extension == ".jpg" || extension == ".jpeg") return "jpg";
    return std::nullopt;
}

Result<void> ImageExportService::exportTo(const ProjectDocumentData& document, const std::filesystem::path& outputPath, const ExportOptions& options)
{
    const auto format = formatFromPath(outputPath);
    if(!format) return Result<void>::failure(ErrorCode::InvalidArgument, "Unsupported export extension.");
    auto rendered = renderer_.renderCanvas(document, options, *format);
    if(!rendered.ok()) return Result<void>::failure(rendered.code(), rendered.message());
    if(rendered.value().bytes.empty()) return Result<void>::failure(ErrorCode::RendererFailure, "Renderer returned no image data.");
    std::ofstream output(outputPath, std::ios::binary);
    if(!output) return Result<void>::failure(ErrorCode::FileWriteFailed, "Could not open export file.");
    output.write(reinterpret_cast<const char*>(rendered.value().bytes.data()), static_cast<std::streamsize>(rendered.value().bytes.size()));
    return output ? Result<void>::success() : Result<void>::failure(ErrorCode::FileWriteFailed, "Could not write export file.");
}

ProjectCommands::ProjectCommands(ProjectFileService& fileService)
:
fileService_(fileService)
{
}

void ProjectCommands::setChooseSavePath(std::function<std::optional<std::filesystem::path>()> callback) { chooseSavePath_ = std::move(callback); }
void ProjectCommands::setConfirmOverwrite(std::function<bool(const std::filesystem::path&)> callback) { confirmOverwrite_ = std::move(callback); }
void ProjectCommands::setDecideUnsaved(std::function<UnsavedDecision()> callback) { decideUnsaved_ = std::move(callback); }
void ProjectCommands::setRecentProjectRecorder(std::function<void(const std::string&, const std::filesystem::path&, Timestamp)> callback) { recordRecentProject_ = std::move(callback); }

Result<void> ProjectCommands::save(ProjectDocumentData& document)
{
    if(!document.metadata.filePath)
    {
        if(!chooseSavePath_) return Result<void>::failure(ErrorCode::InvalidArgument, "No save path is available.");
        const auto chosen = chooseSavePath_();
        if(!chosen) return Result<void>::failure(ErrorCode::OperationCancelled, "Save cancelled.");
        return saveAs(document, *chosen);
    }
    auto result = fileService_.saveTo(document, *document.metadata.filePath);
    if(result.ok())
    {
        document.metadata.lastModifiedAt = now();
        document.metadata.dirty = false;
        if(recordRecentProject_) recordRecentProject_(document.metadata.name, *document.metadata.filePath, document.metadata.lastModifiedAt);
    }
    return result;
}

Result<void> ProjectCommands::saveAs(ProjectDocumentData& document, const std::filesystem::path& path)
{
    const auto normalized = ProjectFileService::normalizeProjectPath(path);
    if(std::filesystem::exists(normalized) && confirmOverwrite_ && !confirmOverwrite_(normalized))
    {
        return Result<void>::failure(ErrorCode::OperationCancelled, "Overwrite cancelled.");
    }
    auto copy = document;
    copy.metadata.filePath = normalized.string();
    copy.metadata.lastModifiedAt = now();
    auto result = fileService_.saveTo(copy, normalized);
    if(result.ok())
    {
        document = copy;
        document.metadata.dirty = false;
        if(recordRecentProject_) recordRecentProject_(document.metadata.name, normalized, document.metadata.lastModifiedAt);
    }
    return result;
}

Result<ProjectDocumentData> ProjectCommands::open(ProjectDocumentData* currentDocument, const std::filesystem::path& path)
{
    if(currentDocument != nullptr && currentDocument->metadata.dirty)
    {
        if(!decideUnsaved_) return Result<ProjectDocumentData>::failure(ErrorCode::OperationCancelled, "Unsaved changes decision is required.");
        const UnsavedDecision decision = decideUnsaved_();
        if(decision == UnsavedDecision::Cancel) return Result<ProjectDocumentData>::failure(ErrorCode::OperationCancelled, "Open cancelled.");
        if(decision == UnsavedDecision::Save)
        {
            auto saveResult = save(*currentDocument);
            if(!saveResult.ok()) return Result<ProjectDocumentData>::failure(saveResult.code(), saveResult.message());
        }
    }
    auto result = fileService_.loadFrom(path);
    if(result.ok() && recordRecentProject_) recordRecentProject_(result.value().metadata.name, path, now());
    return result;
}

}
