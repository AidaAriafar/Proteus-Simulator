# Integration Guide

This module is designed as an independent boundary for Kiarash's subprojects. It does not own Aida's component model or Niloofar's wiring/simulation systems.

## Aida Component Catalog Registration

Aida's component subsystem can expose library items by translating its public component metadata into `kiarash::ComponentDescriptor`.

Required stable fields:

- `id`: stable descriptor ID, such as `aida.resistor`.
- `displayName`: user-facing name.
- `typeName`: technical type or factory key.
- `categoryID`: category registered in `ComponentCategoryTree`.
- `tags`: search keywords.
- `pins`: display-only pin summary.
- `previewResource`: optional preview resource path or renderer key.

The integration should implement `IComponentCatalogSource` and pass descriptors into `ComponentCatalog`.

## Placement Requests To Aida Editor

The active component list stores descriptor IDs only. When the user chooses a quick-access item, `ComponentLibraryController::forwardPlacementRequest()` calls `IComponentPlacementRequestSink`.

Aida's editor should implement:

```cpp
void requestPlacement(const kiarash::ComponentDescriptor& descriptor);
```

The sink should translate `descriptor.typeName` or `descriptor.id` into Aida's own placement/factory request. Kiarash's module must not construct live circuit components.

## Aida Commands In Undo/Redo

Aida can submit editor commands to `CommandHistory` by implementing `ICommand`.

Expectations:

- `execute()` performs the action once.
- `undo()` reverses it without dangling references.
- `redo()` may reuse `execute()` or provide a specialized implementation.
- Commands should store stable IDs or safe handles, not raw owning pointers.

## Niloofar Wire Serialization

Niloofar's wire subsystem can provide connection DTOs through `IConnectionPersistenceAdapter`.

Required connection fields:

- stable connection ID
- source component ID
- source pin ID
- target component ID
- target pin ID
- optional metadata

`ProjectDeserializer` validates that connection component references exist in the loaded document. Wire geometry and routing internals remain owned by Niloofar.

## Simulation Snapshot Persistence

Execution state is optional. A simulation owner can implement `ISimulationStateSnapshotProvider` and return:

- provider name
- opaque serialized payload

If no provider is connected, projects save and load normally without simulation state.

## Ownership And Lifetime

- Kiarash DTOs are value types.
- Services do not retain raw owning pointers.
- UI callbacks and adapters must outlive controllers that reference them.
- File loading returns a temporary validated `ProjectDocumentData`; callers replace the active document only after success.

## Stable ID Requirements

Stable IDs are required for:

- project documents
- component records
- connection records
- component descriptors
- active library items

IDs must not be memory addresses or UI-widget identifiers.

## Failure Behavior

- Invalid files return `Result` failures and do not mutate active documents.
- Save clears dirty state only after a successful write.
- Save As requires explicit overwrite approval when a file exists.
- Open can be cancelled when the current document has unsaved changes.
- Export fails on unsupported extensions or renderer errors.
