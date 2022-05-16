#pragma once
#include <string>
#include <filesystem>

namespace vkpbrt
{
/**
 * \brief A reference to a resource that can be loaded at runtime.
 */
class ResourceBase
{
public:
    /**
     * \brief Constructor
     * \param resource_file_path Relative path to the resource inside the resource directory.
     */
    explicit ResourceBase(std::filesystem::path resource_file_path);
    virtual ~ResourceBase() = default;

    /**
     * \brief Load the resource into memory.
     */
    void load();

    [[nodiscard]] bool is_loaded() const;

protected:
    virtual void load_internal() = 0;

    std::filesystem::path _resource_file_path;
    bool _is_loaded;
};
}  // namespace vkpbrt
