#include "Utils.hpp"

static bool checkVulkanElement(const tinyxml2::XMLElement *e, const std::string_view attribute, const std::string_view target) {
    using namespace vkgen;

    const auto attr = xml::attrib(e, attribute);
    if (!attr) {
        return true;
    }
    if (attr->find(',') != std::string::npos) {
        const auto api = split(std::string{ attr.value() }, ",");
        for (const auto &t : api) {
            if (t == target) {
                return true;
            }
        }
        return false;
    }
    return attr == target;
}

namespace vkgen::xml
{

    bool isVulkan(const Element e) {
        return checkVulkanElement(e.toElement(), "api", "vulkan");
    }

    bool isVulkanExtension(const Element e) {
        return checkVulkanElement(e.toElement(), "supported", "vulkan");
    }

}  // namespace vkgen::xml