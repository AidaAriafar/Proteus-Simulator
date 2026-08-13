#include "canvas/CanvasServices.h"
#include "library/ComponentLibraryManagement.h"
#include "persistence/PersistenceServices.h"
#include "startup/StartupServices.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace kiarash;

namespace
{

bool near(double a, double b)
{
    return std::abs(a - b) < 0.001;
}

std::filesystem::path tempPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("kiarash_" + name);
}

ProjectDocumentData sampleDocument()
{
    ProjectCreationService creation;
    auto created = creation.create({"Demo", std::nullopt, Size{800, 600}, std::nullopt});
    assert(created.ok());
    auto document = created.value();
    document.components.push_back({"c1", "Resistor", "Resistor", {10, 20}, 90, true, false, {{"resistance", "1000"}}});
    document.components.push_back({"c2", "LED", "LED", {30, 40}, 0, false, false, {{"color", "red"}}});
    document.connections.push_back({"w1", "c1", "P1", "c2", "A", {{"net", "n1"}}});
    document.activeLibraryItems.push_back({"resistor"});
    return document;
}

ComponentCategoryTree sampleTree()
{
    ComponentCategoryTree tree;
    assert(tree.registerCategory({"root", "All", std::nullopt, true}).ok());
    assert(tree.registerCategory({"passive", "Passive", "root", false}).ok());
    assert(tree.registerCategory({"sources", "Sources", "root", false}).ok());
    assert(tree.registerCategory({"digital", "Digital", "root", false}).ok());
    return tree;
}

ComponentCatalog sampleCatalog()
{
    ComponentCatalog catalog;
    assert(catalog.add({"resistor", "Resistor", "Resistor", "passive", {"ohm", "analog"}, {{"P1", "power"}, {"P2", "power"}}, "Limits current", "resistor.png"}).ok());
    assert(catalog.add({"capacitor", "Capacitor", "Capacitor", "passive", {"farad", "analog"}, {{"P1", "power"}, {"P2", "power"}}, "Stores charge", ""}).ok());
    assert(catalog.add({"dc", "DC Source", "DCVoltageSource", "sources", {"voltage", "power"}, {{"Positive", "power"}}, "Constant source", ""}).ok());
    return catalog;
}

class CapturingView : public IStartMenuView
{
public:
    std::string error;
    std::vector<RecentProjectEntry> recent;
    std::optional<ProjectDocumentData> opened;

    void showError(const std::string& message) override { error = message; }
    void showRecentProjects(const std::vector<RecentProjectEntry>& entries) override { recent = entries; }
    void openDocument(const ProjectDocumentData& document) override { opened = document; }
};

class CapturingPlacementSink : public IComponentPlacementRequestSink
{
public:
    std::string requestedID;
    void requestPlacement(const ComponentDescriptor& descriptor) override { requestedID = descriptor.id; }
};

class FakeRenderer : public ICanvasImageRenderer
{
public:
    bool fail{false};
    bool sawGrid{false};
    bool sawBackground{false};
    Result<RenderedImage> renderCanvas(const ProjectDocumentData&, const ExportOptions& options, const std::string& format) override
    {
        if(fail) return Result<RenderedImage>::failure(ErrorCode::RendererFailure, "renderer failed");
        sawGrid = options.includeGrid;
        sawBackground = options.includeBackground;
        if(format == "png") return Result<RenderedImage>::success({{0x89, 'P', 'N', 'G', 1, 2, 3}, "png"});
        return Result<RenderedImage>::success({{0xFF, 0xD8, 0xFF, 1, 2, 3}, "jpg"});
    }
};

class CountingCommand : public ICommand
{
public:
    int& value;
    int executeCount{0};
    explicit CountingCommand(int& valueRef) : value(valueRef) {}
    Result<void> execute() override { ++executeCount; ++value; return Result<void>::success(); }
    Result<void> undo() override { --value; return Result<void>::success(); }
    std::string description() const override { return "count"; }
};

void testStartMenu()
{
    ProjectCreationService creation;
    auto first = creation.create({"One", "A4", std::nullopt, std::nullopt});
    auto second = creation.create({"Two", "A4", std::nullopt, std::nullopt});
    assert(first.ok() && second.ok());
    assert(first.value().metadata.id != second.value().metadata.id);
    assert(near(first.value().canvas.size.width, 297));
    assert(near(first.value().canvas.size.height, 210));
    assert(first.value().components.empty());
    assert(first.value().connections.empty());
    assert(first.value().viewport.zoom == 1.0);
    assert(first.value().metadata.dirty);

    auto custom = creation.create({"Custom", std::nullopt, Size{500, 400}, std::nullopt});
    assert(custom.ok());
    assert(near(custom.value().canvas.size.width, 500));
    assert(!creation.create({"Bad", std::nullopt, Size{-1, 100}, std::nullopt}).ok());
    assert(!creation.create({"Bad", std::nullopt, Size{10, 10}, std::nullopt}).ok());
    assert(!creation.create({"Bad", "missing", std::nullopt, std::nullopt}).ok());

    const auto recentFile = tempPath("recent.tsv");
    std::filesystem::remove(recentFile);
    RecentProjectsService recent(recentFile);
    std::vector<std::filesystem::path> files;
    for(int i = 0; i < 6; ++i)
    {
        auto path = tempPath("project_" + std::to_string(i) + ".proteusjson");
        std::ofstream(path) << "x";
        files.push_back(path);
        recent.addOrUpdate({"P" + std::to_string(i), path, now()});
    }
    assert(recent.entries().size() == 5);
    assert(recent.entries().front().projectName == "P5");
    recent.addOrUpdate({"P3b", files[3], now()});
    assert(recent.entries().front().projectName == "P3b");
    recent.save();
    RecentProjectsService loaded(recentFile);
    assert(loaded.load().ok());
    assert(loaded.entries().size() == 5);
    std::filesystem::remove(files[3]);
    loaded.removeMissingFiles();
    assert(loaded.entries().front().projectName != "P3b");

    CapturingView view;
    ProjectFileService fileService;
    StartMenuController controller(view, creation, recent, fileService);
    auto opened = controller.openProject(tempPath("not_here.proteusjson"));
    assert(!opened.ok());
    assert(!view.error.empty());
}

void testCanvas()
{
    GridSettingsData grid;
    grid.gridSpacing = 10;
    SnapService snap(grid);
    assert(near(snap.snap({14.9, 15.1}).x, 10));
    assert(near(snap.snap({14.9, 15.1}).y, 20));
    assert(near(snap.snap({-14.9, -15.1}).x, -10));
    assert(near(snap.snap({-14.9, -15.1}).y, -20));
    grid.snapEnabled = false;
    snap.updateSettings(grid);
    assert(near(snap.snap({14.9, 15.1}).x, 14.9));

    auto invalidGrid = GridModel::create(GridSettingsData{-1, true, true, 0.5});
    assert(!invalidGrid.ok());
    auto modelResult = GridModel::create(GridSettingsData{20, true, true, 0.5});
    assert(modelResult.ok());
    auto gridModel = modelResult.value();
    gridModel.setVisible(false);
    gridModel.setSnapEnabled(false);
    assert(!gridModel.settings().gridVisible);
    assert(!gridModel.settings().snapEnabled);

    ViewportModel viewport({1000, 800}, {500, 400});
    viewport.zoomBy(100, {250, 200});
    assert(near(viewport.state().zoom, 5.0));
    viewport.zoomBy(0.0001, {250, 200});
    assert(near(viewport.state().zoom, 0.10));
    viewport.resetZoom();
    assert(near(viewport.state().zoom, 1.0));
    viewport.panBy(-100, -50);
    auto scene = viewport.viewToScene({0, 0});
    assert(near(scene.x, 100));
    assert(near(scene.y, 50));
    assert(near(viewport.sceneToView(scene).x, 0));
    CoordinateFormatter formatter(1);
    assert(formatter.format({1.25, 2.75}) == "X: 1.2 Y: 2.8");

    KeyboardShortcutRegistry shortcuts;
    shortcuts.registerShortcut("Ctrl+Z", "undo");
    shortcuts.registerShortcut("Ctrl+Y", "redo");
    shortcuts.registerShortcut("Ctrl+Shift+Z", "redo");
    assert(shortcuts.commandFor("Ctrl+Z") == "undo");
}

void testLibrary()
{
    auto tree = sampleTree();
    assert(tree.find("passive").has_value());
    assert(tree.childrenOf("root").size() == 3);
    tree.setExpanded("passive", true);
    assert(tree.find("passive")->expanded);

    auto catalog = sampleCatalog();
    assert(!catalog.add({"resistor", "Duplicate", "Duplicate", "passive", {}, {}, "", ""}).ok());
    DefaultPreviewProvider previewProvider;
    ComponentLibraryController controller(catalog, tree, previewProvider);

    assert(controller.filter("", "passive").size() == 2);
    assert(controller.filter("resistor", std::nullopt).front().id == "resistor");
    assert(controller.filter("PASSIVE", std::nullopt).size() == 2);
    assert(controller.filter("ohm", std::nullopt).size() == 1);
    assert(controller.filter("source", "sources").size() == 1);
    assert(controller.filter("nothing", std::nullopt).empty());

    auto fallback = controller.preview("capacitor");
    assert(fallback.fallback);
    auto missing = controller.preview("missing");
    assert(missing.fallback);

    assert(controller.addActive("resistor"));
    assert(!controller.addActive("resistor"));
    assert(controller.addActive("capacitor"));
    assert(controller.activeList().items().size() == 2);
    assert(controller.selectActive("capacitor"));
    assert(controller.selectedPlacementDescriptor()->id == "capacitor");
    CapturingPlacementSink sink;
    controller.setPlacementSink(&sink);
    assert(controller.forwardPlacementRequest());
    assert(sink.requestedID == "capacitor");
    assert(controller.removeActive("capacitor"));
}

void testPersistence()
{
    ProjectFileService files;
    auto document = sampleDocument();
    document.simulationSnapshot = SimulationSnapshotData{"sim", "{}"};
    const auto path = tempPath("roundtrip.proteusjson");
    std::filesystem::remove(path);
    assert(files.saveTo(document, path).ok());
    auto loaded = files.loadFrom(path);
    assert(loaded.ok());
    assert(loaded.value().metadata.name == document.metadata.name);
    assert(near(loaded.value().canvas.size.width, 800));
    assert(loaded.value().components.size() == 2);
    assert(loaded.value().connections.size() == 1);
    assert(loaded.value().simulationSnapshot.has_value());

    ProjectDeserializer deserializer;
    assert(!deserializer.deserialize("{bad").ok());
    std::string unsupported = ProjectSerializer().serialize(document);
    auto versionPosition = unsupported.find("\"formatVersion\":1");
    unsupported.replace(versionPosition, 17, "\"formatVersion\":99");
    assert(deserializer.deserialize(unsupported).code() == ErrorCode::UnsupportedVersion);

    auto duplicate = document;
    duplicate.components.push_back(duplicate.components.front());
    assert(deserializer.deserialize(ProjectSerializer().serialize(duplicate)).code() == ErrorCode::DuplicateID);
    auto missingReference = document;
    missingReference.connections.front().toComponentID = "missing";
    assert(deserializer.deserialize(ProjectSerializer().serialize(missingReference)).code() == ErrorCode::MissingReference);

    auto active = document;
    active.metadata.filePath = path.string();
    active.metadata.dirty = true;
    ProjectCommands commands(files);
    commands.setConfirmOverwrite([](const std::filesystem::path&){ return true; });
    int recentUpdates = 0;
    commands.setRecentProjectRecorder([&recentUpdates](const std::string&, const std::filesystem::path&, Timestamp)
    {
        ++recentUpdates;
    });
    assert(commands.save(active).ok());
    assert(!active.metadata.dirty);
    const auto saveAsPath = tempPath("save_as_without_ext");
    std::filesystem::remove(ProjectFileService::normalizeProjectPath(saveAsPath));
    assert(commands.saveAs(active, saveAsPath).ok());
    assert(active.metadata.filePath->find(".proteusjson") != std::string::npos);
    assert(recentUpdates >= 2);

    auto before = active;
    commands.setDecideUnsaved([]{ return UnsavedDecision::Cancel; });
    active.metadata.dirty = true;
    assert(!commands.open(&active, path).ok());
    assert(active.metadata.name == before.metadata.name);
    active.metadata.dirty = false;
    auto opened = commands.open(&active, path);
    assert(opened.ok());
    assert(recentUpdates >= 3);
}

void testUndoRedo()
{
    auto document = sampleDocument();
    CommandHistory history(2);
    assert(!history.canUndo());
    assert(history.execute(std::make_unique<SetProjectNameCommand>(document, "Next")).ok());
    assert(document.metadata.name == "Next");
    assert(history.canUndo());
    assert(history.undoDescription() == "Set project name");
    assert(history.undo().ok());
    assert(document.metadata.name == "Demo");
    assert(history.canRedo());
    assert(history.redo().ok());
    assert(document.metadata.name == "Next");

    assert(history.execute(std::make_unique<ChangeCanvasSizeCommand>(document, Size{900, 700})).ok());
    assert(!history.canRedo());
    assert(history.execute(std::make_unique<AddActiveLibraryItemCommand>(document, "led")).ok());
    assert(history.canUndo());
    history.clear();
    assert(!history.canUndo());

    int value = 0;
    CommandHistory countHistory;
    assert(countHistory.execute(std::make_unique<CountingCommand>(value)).ok());
    assert(value == 1);
    assert(countHistory.undo().ok());
    assert(value == 0);
    assert(countHistory.redo().ok());
    assert(value == 1);
}

void testExport()
{
    auto document = sampleDocument();
    FakeRenderer renderer;
    ImageExportService exportService(renderer);
    const auto png = tempPath("export.png");
    const auto jpg = tempPath("export.jpg");
    std::filesystem::remove(png);
    std::filesystem::remove(jpg);
    assert(exportService.exportTo(document, png, ExportOptions{{320, 200}, true, true}).ok());
    assert(exportService.exportTo(document, jpg, ExportOptions{{320, 200}, false, false}).ok());
    assert(std::filesystem::file_size(png) > 0);
    assert(std::filesystem::file_size(jpg) > 0);
    assert(!renderer.sawGrid);
    assert(!renderer.sawBackground);
    assert(!exportService.exportTo(document, tempPath("export.gif"), {}).ok());
    renderer.fail = true;
    assert(exportService.exportTo(document, tempPath("fail.png"), {}).code() == ErrorCode::RendererFailure);
}

}

int main()
{
    testStartMenu();
    testCanvas();
    testLibrary();
    testPersistence();
    testUndoRedo();
    testExport();
    std::cout << "Kiarash subprojects 1/2/3/10 tests passed." << std::endl;
    return 0;
}
