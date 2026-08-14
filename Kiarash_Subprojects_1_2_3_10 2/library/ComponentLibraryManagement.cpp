#include "library/ComponentLibraryManagement.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace kiarash
{

Result<void> ComponentCategoryTree::registerCategory(const ComponentCategoryNode& category)
{
    if(category.id.empty()) return Result<void>::failure(ErrorCode::InvalidArgument, "Category ID cannot be empty.");
    if(category.parentID && categories_.count(*category.parentID) == 0)
    {
        return Result<void>::failure(ErrorCode::MissingReference, "Parent category does not exist.");
    }
    categories_[category.id] = category;
    return Result<void>::success();
}

std::optional<ComponentCategoryNode> ComponentCategoryTree::find(const std::string& id) const
{
    const auto found = categories_.find(id);
    if(found == categories_.end()) return std::nullopt;
    return found->second;
}

std::vector<ComponentCategoryNode> ComponentCategoryTree::childrenOf(const std::optional<std::string>& parentID) const
{
    std::vector<ComponentCategoryNode> result;
    for(const auto& entry : categories_)
    {
        if(entry.second.parentID == parentID) result.push_back(entry.second);
    }
    return result;
}

void ComponentCategoryTree::setExpanded(const std::string& id, bool expanded)
{
    auto found = categories_.find(id);
    if(found != categories_.end()) found->second.expanded = expanded;
}

std::string ComponentCategoryTree::displayNameFor(const std::string& id) const
{
    const auto found = categories_.find(id);
    return found == categories_.end() ? "" : found->second.displayName;
}

std::vector<ComponentCategoryNode> ComponentCategoryTree::all() const
{
    std::vector<ComponentCategoryNode> result;
    for(const auto& entry : categories_) result.push_back(entry.second);
    return result;
}

Result<void> ComponentCatalog::add(const ComponentDescriptor& descriptor)
{
    if(descriptor.id.empty()) return Result<void>::failure(ErrorCode::InvalidArgument, "Descriptor ID cannot be empty.");
    if(descriptors_.count(descriptor.id) != 0) return Result<void>::failure(ErrorCode::DuplicateID, "Duplicate component descriptor ID.");
    descriptors_[descriptor.id] = descriptor;
    return Result<void>::success();
}

std::optional<ComponentDescriptor> ComponentCatalog::find(const std::string& id) const
{
    const auto found = descriptors_.find(id);
    if(found == descriptors_.end()) return std::nullopt;
    return found->second;
}

std::vector<ComponentDescriptor> ComponentCatalog::all() const
{
    std::vector<ComponentDescriptor> result;
    for(const auto& entry : descriptors_) result.push_back(entry.second);
    return result;
}

std::vector<ComponentDescriptor> ComponentCatalog::byCategory(const std::string& categoryID) const
{
    std::vector<ComponentDescriptor> result;
    for(const auto& entry : descriptors_)
    {
        if(entry.second.categoryID == categoryID) result.push_back(entry.second);
    }
    return result;
}

ComponentSearchIndex::ComponentSearchIndex(ComponentCatalog catalog, ComponentCategoryTree tree)
:
catalog_(std::move(catalog)),
tree_(std::move(tree))
{
}

std::string ComponentSearchIndex::normalize(const std::string& text)
{
    std::string result;
    bool previousSpace = true;
    for(unsigned char ch : text)
    {
        if(std::isspace(ch))
        {
            if(!previousSpace) result.push_back(' ');
            previousSpace = true;
        }
        else
        {
            result.push_back(static_cast<char>(std::tolower(ch)));
            previousSpace = false;
        }
    }
    if(!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

std::vector<ComponentDescriptor> ComponentSearchIndex::search(const std::string& query, const std::optional<std::string>& categoryID) const
{
    const std::string normalizedQuery = normalize(query);
    std::vector<ComponentDescriptor> result;
    for(const auto& descriptor : catalog_.all())
    {
        if(categoryID && descriptor.categoryID != *categoryID) continue;

        std::ostringstream haystack;
        haystack << descriptor.displayName << ' ' << descriptor.typeName << ' '
                 << tree_.displayNameFor(descriptor.categoryID) << ' '
                 << descriptor.description;
        for(const auto& tag : descriptor.tags) haystack << ' ' << tag;

        if(normalizedQuery.empty() || normalize(haystack.str()).find(normalizedQuery) != std::string::npos)
        {
            result.push_back(descriptor);
        }
    }
    std::sort(result.begin(), result.end(), [](const ComponentDescriptor& left, const ComponentDescriptor& right)
    {
        if(left.displayName == right.displayName) return left.id < right.id;
        return left.displayName < right.displayName;
    });
    return result;
}

ComponentPreview DefaultPreviewProvider::previewFor(const ComponentDescriptor& descriptor) const
{
    return ComponentPreview{
        descriptor.displayName,
        descriptor.typeName,
        descriptor.categoryID,
        descriptor.description,
        descriptor.previewResource.empty() ? "preview-unavailable" : descriptor.previewResource,
        descriptor.pins,
        descriptor.previewResource.empty()
    };
}

bool ActiveComponentList::add(const std::string& descriptorID)
{
    if(std::find(descriptorIDs_.begin(), descriptorIDs_.end(), descriptorID) != descriptorIDs_.end()) return false;
    descriptorIDs_.push_back(descriptorID);
    if(!selectedID_) selectedID_ = descriptorID;
    return true;
}

bool ActiveComponentList::remove(const std::string& descriptorID)
{
    const auto oldSize = descriptorIDs_.size();
    descriptorIDs_.erase(std::remove(descriptorIDs_.begin(), descriptorIDs_.end(), descriptorID), descriptorIDs_.end());
    if(selectedID_ == descriptorID) selectedID_.reset();
    return descriptorIDs_.size() != oldSize;
}

void ActiveComponentList::clear()
{
    descriptorIDs_.clear();
    selectedID_.reset();
}

bool ActiveComponentList::select(const std::string& descriptorID)
{
    if(std::find(descriptorIDs_.begin(), descriptorIDs_.end(), descriptorID) == descriptorIDs_.end()) return false;
    selectedID_ = descriptorID;
    return true;
}

std::optional<std::string> ActiveComponentList::selected() const
{
    return selectedID_;
}

std::vector<std::string> ActiveComponentList::items() const
{
    return descriptorIDs_;
}

ComponentLibraryController::ComponentLibraryController(ComponentCatalog catalog, ComponentCategoryTree categoryTree, const IComponentPreviewProvider& previewProvider)
:
catalog_(std::move(catalog)),
categoryTree_(std::move(categoryTree)),
searchIndex_(catalog_, categoryTree_),
previewProvider_(previewProvider)
{
}

std::vector<ComponentDescriptor> ComponentLibraryController::filter(const std::string& query, const std::optional<std::string>& categoryID) const
{
    return searchIndex_.search(query, categoryID);
}

ComponentPreview ComponentLibraryController::preview(const std::string& descriptorID) const
{
    const auto descriptor = catalog_.find(descriptorID);
    if(!descriptor)
    {
        return ComponentPreview{"Unavailable", descriptorID, "", "", "preview-unavailable", {}, true};
    }
    return previewProvider_.previewFor(*descriptor);
}

bool ComponentLibraryController::addActive(const std::string& descriptorID)
{
    return catalog_.find(descriptorID).has_value() && activeList_.add(descriptorID);
}

bool ComponentLibraryController::removeActive(const std::string& descriptorID)
{
    return activeList_.remove(descriptorID);
}

bool ComponentLibraryController::selectActive(const std::string& descriptorID)
{
    return activeList_.select(descriptorID);
}

std::optional<ComponentDescriptor> ComponentLibraryController::selectedPlacementDescriptor() const
{
    const auto selected = activeList_.selected();
    if(!selected) return std::nullopt;
    return catalog_.find(*selected);
}

void ComponentLibraryController::setPlacementSink(IComponentPlacementRequestSink* sink)
{
    placementSink_ = sink;
}

bool ComponentLibraryController::forwardPlacementRequest()
{
    const auto descriptor = selectedPlacementDescriptor();
    if(!descriptor || placementSink_ == nullptr) return false;
    placementSink_->requestPlacement(*descriptor);
    return true;
}

const ActiveComponentList& ComponentLibraryController::activeList() const
{
    return activeList_;
}

}
