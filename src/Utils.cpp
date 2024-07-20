#include "Utils.hpp"

#include <optional>

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

    std::string_view value(const tinyxml2::XMLNode &node) {
        const auto *value = node.Value();
        return value? std::string_view{value} : "";
    }

    std::string_view requiredAttrib(const tinyxml2::XMLElement *e, const std::string_view &attribute) {
        const char *attrib = e->Attribute(attribute.data());
        if (!attrib) {
            std::cerr << e->Value() << ":\n";
            auto *att = e->FirstAttribute();
            while (att) {
                std::cerr << "  " << att->Name() << "=" << att->Value() << "\n";
                att = att->Next();
            }
            throw AttributeNotFoundException{ ":" + std::to_string(e->GetLineNum()) + " missing XML attribute: " + std::string{ attribute } };
        }
        return attrib;
    }

    std::optional<std::string_view> attrib(const tinyxml2::XMLElement *e, const std::string_view &attribute) {
        const char *attrib = e->Attribute(attribute.data());
        if (!attrib) {
            return {};
        }
        return attrib;
    }



    Element& Element::operator=(tinyxml2::XMLElement *e) {
        data = e;
        return *this;
    }

    bool Element::operator==(const Element &o) const {
        return data == o.data;
    }

    tinyxml2::XMLElement* Element::operator->() const {
        return data;
    }

    tinyxml2::XMLElement* Element::toElement() const {
        return data;
    }

    Element Element::firstChild() const {
        return Element{ data->FirstChildElement() };
    }

    std::string_view Element::operator[](const std::string_view attrib) const {
        return xml::requiredAttrib(data, attrib);
    }

    std::string_view Element::required(const std::string_view attrib) const {
        return xml::requiredAttrib(data, attrib);
    }

    std::optional<std::string_view> Element::optional(const std::string_view attrib) const {
        return xml::attrib(data, attrib);
    }

    std::string_view Element::value() const {
        const auto *value = data->Value();
        return value? std::string_view{value} : "";
    };

    std::string_view Element::getNested(const std::string_view attrib) const {
        auto a = optional(attrib);
        if (a) {
            return a.value();
        }

        tinyxml2::XMLElement *elem = data->FirstChildElement(attrib.data());
        if (elem) {
            const auto *text = elem->GetText();
            if (text) {
                return std::string_view{ text };
            }
        }
        return "";
    }

}  // namespace vkgen::xml