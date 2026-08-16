#ifndef KIARASH_LIBRARY_COMPONENT_LIBRARY_MANAGEMENT_H
#define KIARASH_LIBRARY_COMPONENT_LIBRARY_MANAGEMENT_H

#include "common/ProjectData.h"
#include "common/Result.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace kiarash
{

struct PinSummary
{
    std::string name;
    std::string direction;
};

struct ComponentDescriptor
{
    std::string id;
    std::string displayName;
    std::string typeName;
    std::string categoryID;
    std::vector<std::string> tags;
    std::vector<PinSummary> pins;
    std::string description;
    std::string previewResource;
};

struct ComponentCategoryNode
{
    std::string id;
    std::string displayName;
    std::optional<std::string> parentID;
    bool expanded{false};
};

struct ComponentPreview
{
    std::string displayName;
    std::string typeName;
    std::string categoryID;
    std::string description;
    std::string previewResource;
    std::vector<PinSummary> pins;
    bool fallback{false};
};

class IComponentCatalogSource
{
public:
    virtual ~IComponentCatalogSource() = default;
    virtual std::vector<ComponentDescriptor> descriptors() const = 0;
};

class IComponentPreviewProvider
{
public:
    virtual ~IComponentPreviewProvider() = default;
    virtual ComponentPreview previewFor(const ComponentDescriptor& descriptor) const = 0;
};

class IComponentPlacementRequestSink
{
public:
    virtual ~IComponentPlacementRequestSink() = default;
    virtual void requestPlacement(const ComponentDescriptor& descriptor) = 0;
};

class ComponentCategoryTree
{
private:
    std::map<std::string, ComponentCategoryNode> categories_;

public:
    Result<void> registerCategory(const ComponentCategoryNode& category);
    std::optional<ComponentCategoryNode> find(const std::string& id) const;
    std::vector<ComponentCategoryNode> childrenOf(const std::optional<std::string>& parentID) const;
    void setExpanded(const std::string& id, bool expanded);
    std::string displayNameFor(const std::string& id) const;
    std::vector<ComponentCategoryNode> all() const;
};

class ComponentCatalog
{
private:
    std::map<std::string, ComponentDescriptor> descriptors_;

public:
    Result<void> add(const ComponentDescriptor& descriptor);
    std::optional<ComponentDescriptor> find(const std::string& id) const;
    std::vector<ComponentDescriptor> all() const;
    std::vector<ComponentDescriptor> byCategory(const std::string& categoryID) const;
};

class ComponentSearchIndex
{
private:
    ComponentCatalog catalog_;
    ComponentCategoryTree tree_;
    static std::string normalize(const std::string& text);

public:
    ComponentSearchIndex(ComponentCatalog catalog, ComponentCategoryTree tree);
    std::vector<ComponentDescriptor> search(const std::string& query, const std::optional<std::string>& categoryID = std::nullopt) const;
};

class DefaultPreviewProvider : public IComponentPreviewProvider
{
public:
    ComponentPreview previewFor(const ComponentDescriptor& descriptor) const override;
};

class ActiveComponentList
{
private:
    std::vector<std::string> descriptorIDs_;
    std::optional<std::string> selectedID_;

public:
    bool add(const std::string& descriptorID);
    bool remove(const std::string& descriptorID);
    void clear();
    bool select(const std::string& descriptorID);
    std::optional<std::string> selected() const;
    std::vector<std::string> items() const;
};

class ComponentLibraryController
{
private:
    ComponentCatalog catalog_;
    ComponentCategoryTree categoryTree_;
    ComponentSearchIndex searchIndex_;
    ActiveComponentList activeList_;
    const IComponentPreviewProvider& previewProvider_;
    IComponentPlacementRequestSink* placementSink_{nullptr};

public:
    ComponentLibraryController(ComponentCatalog catalog, ComponentCategoryTree categoryTree, const IComponentPreviewProvider& previewProvider);
    std::vector<ComponentDescriptor> filter(const std::string& query, const std::optional<std::string>& categoryID = std::nullopt) const;
    ComponentPreview preview(const std::string& descriptorID) const;
    bool addActive(const std::string& descriptorID);
    bool removeActive(const std::string& descriptorID);
    bool selectActive(const std::string& descriptorID);
    std::optional<ComponentDescriptor> selectedPlacementDescriptor() const;
    void setPlacementSink(IComponentPlacementRequestSink* sink);
    bool forwardPlacementRequest();
    const ActiveComponentList& activeList() const;
};

}

#endif
