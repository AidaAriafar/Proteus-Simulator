# Kiarash Subprojects 1, 2, 3, and 10

This directory is an independent implementation area for Kiarash's assigned Proteus Simulator subprojects:

- 1: Start Menu
- 2: Main Canvas and Design Environment
- 3: Component Library Management
- 10: File Management and Additional Capabilities

No source files in Aida's root `src/` tree or Niloofar's `Project_7_8_9/` tree are modified or required for this module to build.

## Architecture

The module is intentionally backend-first because the repository root has no shared GUI framework. UI code can be added later as thin adapters over these controller and service APIs.

- `common`: neutral DTOs, timestamps, IDs, and result/error types.
- `canvas`: canvas presets, grid settings, snap service, viewport zoom/pan, coordinate formatting, shortcut registry.
- `library`: component descriptors, category tree, search index, preview provider boundary, active component list.
- `startup`: start-menu controller, project creation service, recent-project persistence.
- `persistence`: JSON save/load, file commands, command history, image export abstraction.
- `tests`: CTest-based automated tests with test doubles.
- `examples`: minimal console demonstration.

## Build

From the repository root:

```sh
cmake -S Kiarash_Subprojects_1_2_3_10 -B build-kiarash
cmake --build build-kiarash
ctest --test-dir build-kiarash --output-on-failure
```

Targets:

- `kiarash_project_modules`: reusable library target.
- `kiarash_project_tests`: automated test executable.
- `kiarash_demo`: minimal runnable demonstration.

## Creating a Project

```cpp
kiarash::ProjectCreationService creation;
auto result = creation.create({"My Circuit", "A4", std::nullopt, std::nullopt});
if (result.ok()) {
    kiarash::ProjectDocumentData document = result.value();
}
```

The created document receives a stable project ID, timestamps, empty component and connection records, canvas dimensions, default viewport, and dirty state.

## Saving and Loading

```cpp
kiarash::ProjectFileService files;
auto save = files.saveTo(document, "my_project.proteusjson");
auto loaded = files.loadFrom("my_project.proteusjson");
```

The format is JSON with `format` and `formatVersion` fields. Loading validates duplicate component IDs and missing connection references before returning a document.

## Exporting

```cpp
class Renderer : public kiarash::ICanvasImageRenderer {
public:
    kiarash::Result<kiarash::RenderedImage> renderCanvas(
        const kiarash::ProjectDocumentData&,
        const kiarash::ExportOptions&,
        const std::string& format) override;
};

Renderer renderer;
kiarash::ImageExportService exportService(renderer);
exportService.exportTo(document, "circuit.png", {{1024, 768}, true, true});
```

The export service validates `.png`, `.jpg`, and `.jpeg` paths and writes only bytes returned by the canvas renderer. It does not capture windows.

## Undo and Redo

```cpp
kiarash::CommandHistory history(100);
history.execute(std::make_unique<kiarash::SetProjectNameCommand>(document, "New Name"));
history.undo();
history.redo();
```

The history supports command descriptions, maximum size, redo-stack clearing after new commands, and `Ctrl+Z` / `Ctrl+Y` / `Ctrl+Shift+Z` hooks through `KeyboardShortcutRegistry`.

## Current Limitations

- No concrete GUI is fabricated; UI integration is through interfaces.
- No component electrical behavior, wire routing, simulation engine, DRC, or measurement tools are implemented here.
- JSON parsing is intentionally scoped to the project schema emitted by `ProjectSerializer`.
- Image export requires a real `ICanvasImageRenderer` implementation from the eventual GUI/canvas layer.

Teammate directories were not modified by this implementation.
