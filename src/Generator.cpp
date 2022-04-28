#include "Generator.hpp"

#include <fstream>

void Generator::parsePlatforms(XMLNode *node) {
    std::cout << "Parsing platforms" << std::endl;
    // iterate contents of <platforms>, filter only <platform> children
    for (XMLElement *platform : Elements(node) | ValueFilter("platform")) {
        const char *name = platform->Attribute("name");
        const char *protect = platform->Attribute("protect");
        if (name && protect) {
            platforms.emplace(
                name, PlatformData{.protect = protect,
                                   .guiState = defaultWhitelistOption ? 1 : 0});
        }
    }
    std::cout << "Parsing platforms done" << std::endl;
}

void Generator::parseFeature(XMLNode *node) {
    std::cout << "Parsing feature" << std::endl;
    const char *name = node->ToElement()->Attribute("name");
    if (name) {
        for (XMLElement *require : Elements(node) | ValueFilter("require")) {
            for (XMLElement &entry : Elements(require)) {
                const std::string_view value = entry.Value();

                if (value == "enum") {
                    parseEnumExtend(entry, nullptr, true);
                }
            }
        }
    }

    std::cout << "Parsing feature done" << std::endl;
}

void Generator::parseExtensions(XMLNode *node) {
    std::cout << "Parsing extensions" << std::endl;
    // iterate contents of <extensions>, filter only <extension> children
    for (XMLElement *extension : Elements(node) | ValueFilter("extension")) {
        const char *supported = extension->Attribute("supported");
        bool flagSupported = true;
        if (supported && std::string_view(supported) == "disabled") {
            flagSupported = false;
        }

        const char *name = extension->Attribute("name");
        const char *platformAttrib = extension->Attribute("platform");
        const char *protect = nullptr;

        Platforms::pointer platform = nullptr;

        if (platformAttrib) {
            auto it = platforms.find(platformAttrib);
            if (it != platforms.end()) {
                platform = &(*it);
                protect = platform->second.protect.data();
            } else {
                std::cerr << "err: Unknown platform in extensions: "
                          << platformAttrib << std::endl;
            }
        } else {
            // std::cerr << "err: Cant find platform" << std::endl;
        }

        std::map<std::string, ExtensionData>::reference ext =
            *extensions
                 .emplace(name,
                          ExtensionData{.platform = platform,
                                        .enabled = flagSupported,
                                        .whitelisted = flagSupported &&
                                                       defaultWhitelistOption})
                 .first;

        // iterate contents of <extension>, filter only <require> children
        for (XMLElement *require :
             Elements(extension) | ValueFilter("require")) {
            // iterate contents of <require>
            for (XMLElement &entry : Elements(require)) {
                const std::string_view value = entry.Value();
                if (value == "command" && protect) {
                    const char *name = entry.Attribute("name");
                    if (name) { // pair extension name with platform protect
                        namemap.emplace(name, ext);
                    }
                } else if (value == "type" && protect) {
                    const char *name = entry.Attribute("name");
                    if (name) { // pair extension name with platform protect
                        namemap.emplace(name, ext);
                    }
                } else if (value == "enum") {
                    parseEnumExtend(entry, protect, flagSupported);
                }
            }
        }
    }

    std::cout << "Parsing extensions done" << std::endl;
}

void Generator::parseTags(XMLNode *node) {
    std::cout << "Parsing tags" << std::endl;
    // iterate contents of <tags>, filter only <tag> children
    for (XMLElement *tag : Elements(node) | ValueFilter("tag")) {
        const char *name = tag->Attribute("name");
        if (name) {
            tags.emplace(name);
        }
    }
    std::cout << "Parsing tags done" << std::endl;
}

Generator::ExtensionData *
Generator::findExtensionProtect(const std::string &str) {
    auto it = namemap.find(str);
    if (it != namemap.end()) {
        return &it->second.second;
    }
    return nullptr;
}

bool Generator::idLookup(const std::string &name, std::string_view &protect) {
    auto it = namemap.find(name);
    if (it != namemap.end()) {
        const ExtensionData &ext = it->second.second;
        if (!ext.enabled || !ext.whitelisted) {
            return false;
        }
        if (ext.platform) {
            //            if (!ext.platform->second.whitelisted) {
            //                return false;
            //            }
            protect = ext.platform->second.protect;
        }
    }
    return true;
}

void Generator::withProtect(std::string &output, const char *protect,
                            std::function<void(std::string &)> function) {
    if (protect) {
        output += "#if defined(" + std::string(protect) + ")\n";
    }

    function(output);

    if (protect) {
        output += "#endif //" + std::string(protect) + "\n";
    }
}

void Generator::genOptional(const std::string &name,
                            std::function<void()> function) {

    std::string_view protect;
    if (!idLookup(name, protect)) {
        return;
    }
    if (!protect.empty()) {
        output += "#if defined(" + std::string(protect) + ")\n";
    }

    function();

    if (!protect.empty()) {
        output += "#endif //" + std::string(protect) + "\n";
    };
}

std::string
Generator::genOptional(const std::string &name,
                       std::function<void(std::string &)> function) {
    std::string output;
    std::string_view protect;
    if (!idLookup(name, protect)) {
        return "";
    }

    withProtect(output, protect.data(), function);

    return output;
}

std::string Generator::strRemoveTag(std::string &str) {
    std::string suffix;
    auto it = str.rfind('_');
    if (it != std::string::npos) {
        suffix = str.substr(it + 1);
        if (tags.find(suffix) != tags.end()) {
            str.erase(it);
        } else {
            suffix.clear();
        }
    }

    for (auto &t : tags) {
        if (str.ends_with(t)) {
            str.erase(str.size() - t.size());
            return t;
        }
    }
    return suffix;
}

std::string Generator::strWithoutTag(const std::string &str) {
    std::string out = str;
    for (const std::string &tag : tags) {
        if (out.ends_with(tag)) {
            out.erase(out.size() - tag.size());
            break;
        }
    }
    return out;
}

std::pair<std::string, std::string>
Generator::snakeToCamelPair(std::string str) {
    std::string suffix = strRemoveTag(str);
    std::string out = convertSnakeToCamel(str);

    out = std::regex_replace(out, std::regex("bit"), "Bit");
    out = std::regex_replace(out, std::regex("Rgba10x6"), "Rgba10X6");

    return std::make_pair(out, suffix);
}

std::string Generator::snakeToCamel(const std::string &str) {
    const auto &p = snakeToCamelPair(str);
    return p.first + p.second;
}

std::string Generator::enumConvertCamel(const std::string &enumName,
                                        std::string value) {
    std::string out = snakeToCamel(value);
    strStripPrefix(out, "vk");

    size_t i, s = enumName.size();
    for (i = s; i >= 0; --i) {
        std::string p = enumName.substr(0, i);
        if ((i == s || std::isupper(enumName[i])) && out.starts_with(p)) {
            out.erase(0, i);
            break;
        }
    }

    for (int j = i; j < s; ++j) {
        std::string p = enumName.substr(j, s);
        if (std::isupper(p[0]) && out.ends_with(p)) {
            out.erase(out.size() - (s - j));
            break;
        }
    }

    for (size_t i = 0; i < out.size() - 1; ++i) {
        if (std::isdigit(out[i + 1])) {
            out[i] = std::toupper(out[i]);
        } else if (std::isdigit(out[i]) && out[i + 1] == 'd') {
            out[i + 1] = 'D';
        }
    }

    return "e" + out;
}

std::string Generator::toCppStyle(const std::string &str,
                                  bool firstLetterCapital) {
    std::string out = stripVkPrefix(str);
    if (!out.empty()) {
        if (!firstLetterCapital) {
            out[0] = std::tolower(out[0]);
        }
    }
    return out;
}

void Generator::parseXML() {
    // maps nodes from vk.xml registry to functions
    static const std::vector<OrderPair> genTable{
        {"enums",
         std::bind(&Generator::genEnums, this, std::placeholders::_1)}};

    int c = 0;
    for (XMLElement &node : Elements(root)) {
        std::string_view value{node.Value()};
        if (value == "enums") {
            genEnums(&node);
        }
    }

    genFlags();
    genTypes();
}

std::string Generator::generateFile() {
    parseXML();

    std::string output;

    output += "#ifndef " + FILEPROTECT + "\n";
    output += "#define " + FILEPROTECT + "\n";

    output += RES_HEADER;
    output += "\n";

    if (cfg.vkname.usesDefine) {
        output += format(R"(
#if !defined( {0} )
#  define {0} {1}
#endif

#define VULKAN_HPP_NAMESPACE_STRING   VULKAN_HPP_STRINGIFY( {0} )
)",
                         cfg.vkname.define, cfg.vkname.value);
    } else {
        output +=
            "#define VULKAN_HPP_NAMESPACE_STRING   VULKAN_HPP_STRINGIFY( \"" +
            cfg.vkname.value + "\" )\n";
    }
    output += "\nnamespace " + cfg.vkname.get() + " {\n";

    output += RES_BASE_TYPES;
    output += "\n";
    output += RES_OTHER_HPP;
    output += "\n";
    output += RES_ARRAY_PROXY_HPP;
    output += RES_FLAGS_HPP;
    output += "\n";
    output += RES_RESULT_VALUE_STRUCT_HPP;
    output += "\n";

    output += this->output + "\n";



    output += RES_ERROR_HPP;
    output += "\n";

    output += R"(
  class OutOfHostMemoryError : public SystemError
  {
  public:
    OutOfHostMemoryError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorOutOfHostMemory ), message )
    {}
    OutOfHostMemoryError( char const * message )
      : SystemError( make_error_code( Result::eErrorOutOfHostMemory ), message )
    {}
  };

  class OutOfDeviceMemoryError : public SystemError
  {
  public:
    OutOfDeviceMemoryError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorOutOfDeviceMemory ), message )
    {}
    OutOfDeviceMemoryError( char const * message )
      : SystemError( make_error_code( Result::eErrorOutOfDeviceMemory ), message )
    {}
  };

  class InitializationFailedError : public SystemError
  {
  public:
    InitializationFailedError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorInitializationFailed ), message )
    {}
    InitializationFailedError( char const * message )
      : SystemError( make_error_code( Result::eErrorInitializationFailed ), message )
    {}
  };

  class DeviceLostError : public SystemError
  {
  public:
    DeviceLostError( std::string const & message ) : SystemError( make_error_code( Result::eErrorDeviceLost ), message )
    {}
    DeviceLostError( char const * message ) : SystemError( make_error_code( Result::eErrorDeviceLost ), message ) {}
  };

  class MemoryMapFailedError : public SystemError
  {
  public:
    MemoryMapFailedError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorMemoryMapFailed ), message )
    {}
    MemoryMapFailedError( char const * message )
      : SystemError( make_error_code( Result::eErrorMemoryMapFailed ), message )
    {}
  };

  class LayerNotPresentError : public SystemError
  {
  public:
    LayerNotPresentError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorLayerNotPresent ), message )
    {}
    LayerNotPresentError( char const * message )
      : SystemError( make_error_code( Result::eErrorLayerNotPresent ), message )
    {}
  };

  class ExtensionNotPresentError : public SystemError
  {
  public:
    ExtensionNotPresentError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorExtensionNotPresent ), message )
    {}
    ExtensionNotPresentError( char const * message )
      : SystemError( make_error_code( Result::eErrorExtensionNotPresent ), message )
    {}
  };

  class FeatureNotPresentError : public SystemError
  {
  public:
    FeatureNotPresentError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorFeatureNotPresent ), message )
    {}
    FeatureNotPresentError( char const * message )
      : SystemError( make_error_code( Result::eErrorFeatureNotPresent ), message )
    {}
  };

  class IncompatibleDriverError : public SystemError
  {
  public:
    IncompatibleDriverError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorIncompatibleDriver ), message )
    {}
    IncompatibleDriverError( char const * message )
      : SystemError( make_error_code( Result::eErrorIncompatibleDriver ), message )
    {}
  };

  class TooManyObjectsError : public SystemError
  {
  public:
    TooManyObjectsError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorTooManyObjects ), message )
    {}
    TooManyObjectsError( char const * message )
      : SystemError( make_error_code( Result::eErrorTooManyObjects ), message )
    {}
  };

  class FormatNotSupportedError : public SystemError
  {
  public:
    FormatNotSupportedError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorFormatNotSupported ), message )
    {}
    FormatNotSupportedError( char const * message )
      : SystemError( make_error_code( Result::eErrorFormatNotSupported ), message )
    {}
  };

  class FragmentedPoolError : public SystemError
  {
  public:
    FragmentedPoolError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorFragmentedPool ), message )
    {}
    FragmentedPoolError( char const * message )
      : SystemError( make_error_code( Result::eErrorFragmentedPool ), message )
    {}
  };

  class UnknownError : public SystemError
  {
  public:
    UnknownError( std::string const & message ) : SystemError( make_error_code( Result::eErrorUnknown ), message ) {}
    UnknownError( char const * message ) : SystemError( make_error_code( Result::eErrorUnknown ), message ) {}
  };

  class OutOfPoolMemoryError : public SystemError
  {
  public:
    OutOfPoolMemoryError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorOutOfPoolMemory ), message )
    {}
    OutOfPoolMemoryError( char const * message )
      : SystemError( make_error_code( Result::eErrorOutOfPoolMemory ), message )
    {}
  };

  class InvalidExternalHandleError : public SystemError
  {
  public:
    InvalidExternalHandleError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorInvalidExternalHandle ), message )
    {}
    InvalidExternalHandleError( char const * message )
      : SystemError( make_error_code( Result::eErrorInvalidExternalHandle ), message )
    {}
  };

  class FragmentationError : public SystemError
  {
  public:
    FragmentationError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorFragmentation ), message )
    {}
    FragmentationError( char const * message ) : SystemError( make_error_code( Result::eErrorFragmentation ), message )
    {}
  };

  class InvalidOpaqueCaptureAddressError : public SystemError
  {
  public:
    InvalidOpaqueCaptureAddressError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorInvalidOpaqueCaptureAddress ), message )
    {}
    InvalidOpaqueCaptureAddressError( char const * message )
      : SystemError( make_error_code( Result::eErrorInvalidOpaqueCaptureAddress ), message )
    {}
  };

  class SurfaceLostKHRError : public SystemError
  {
  public:
    SurfaceLostKHRError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorSurfaceLostKHR ), message )
    {}
    SurfaceLostKHRError( char const * message )
      : SystemError( make_error_code( Result::eErrorSurfaceLostKHR ), message )
    {}
  };

  class NativeWindowInUseKHRError : public SystemError
  {
  public:
    NativeWindowInUseKHRError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorNativeWindowInUseKHR ), message )
    {}
    NativeWindowInUseKHRError( char const * message )
      : SystemError( make_error_code( Result::eErrorNativeWindowInUseKHR ), message )
    {}
  };

  class OutOfDateKHRError : public SystemError
  {
  public:
    OutOfDateKHRError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorOutOfDateKHR ), message )
    {}
    OutOfDateKHRError( char const * message ) : SystemError( make_error_code( Result::eErrorOutOfDateKHR ), message ) {}
  };

  class IncompatibleDisplayKHRError : public SystemError
  {
  public:
    IncompatibleDisplayKHRError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorIncompatibleDisplayKHR ), message )
    {}
    IncompatibleDisplayKHRError( char const * message )
      : SystemError( make_error_code( Result::eErrorIncompatibleDisplayKHR ), message )
    {}
  };

  class ValidationFailedEXTError : public SystemError
  {
  public:
    ValidationFailedEXTError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorValidationFailedEXT ), message )
    {}
    ValidationFailedEXTError( char const * message )
      : SystemError( make_error_code( Result::eErrorValidationFailedEXT ), message )
    {}
  };

  class InvalidShaderNVError : public SystemError
  {
  public:
    InvalidShaderNVError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorInvalidShaderNV ), message )
    {}
    InvalidShaderNVError( char const * message )
      : SystemError( make_error_code( Result::eErrorInvalidShaderNV ), message )
    {}
  };

  class InvalidDrmFormatModifierPlaneLayoutEXTError : public SystemError
  {
  public:
    InvalidDrmFormatModifierPlaneLayoutEXTError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorInvalidDrmFormatModifierPlaneLayoutEXT ), message )
    {}
    InvalidDrmFormatModifierPlaneLayoutEXTError( char const * message )
      : SystemError( make_error_code( Result::eErrorInvalidDrmFormatModifierPlaneLayoutEXT ), message )
    {}
  };

  class NotPermittedEXTError : public SystemError
  {
  public:
    NotPermittedEXTError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorNotPermittedEXT ), message )
    {}
    NotPermittedEXTError( char const * message )
      : SystemError( make_error_code( Result::eErrorNotPermittedEXT ), message )
    {}
  };

#  if defined( VK_USE_PLATFORM_WIN32_KHR )
  class FullScreenExclusiveModeLostEXTError : public SystemError
  {
  public:
    FullScreenExclusiveModeLostEXTError( std::string const & message )
      : SystemError( make_error_code( Result::eErrorFullScreenExclusiveModeLostEXT ), message )
    {}
    FullScreenExclusiveModeLostEXTError( char const * message )
      : SystemError( make_error_code( Result::eErrorFullScreenExclusiveModeLostEXT ), message )
    {}
  };
#  endif /*VK_USE_PLATFORM_WIN32_KHR*/
    )";
    output += "\n";

    output += RES_RESULT_VALUE_HPP;
    output += "\n";

    output += outputFuncs;
    output += "\n";

    output += "}\n";
    output += "#endif //" + FILEPROTECT + "\n";

    return output;
}

std::vector<VariableData>
Generator::parseStructMembers(XMLElement *node, std::string &structType,
                              std::string &structTypeValue) {
    std::vector<VariableData> members;
    // iterate contents of <type>, filter only <member> children
    for (XMLElement *member : Elements(node) | ValueFilter("member")) {
        std::string out;

        XMLVariableParser parser{member}; // parse <member>

        std::string type = parser.type();
        std::string name = parser.identifier();

        if (const char *values = member->ToElement()->Attribute("values")) {
            std::string value = enumConvertCamel(type, values);
            parser.setAssignment(type + "::" + value);
            if (name == "sType") { // save sType information for structType
                structType = type;
                structTypeValue = value;
            }
        } else {
            if (name == "sType") {
                out += " = StructureType::eApplicationInfo";
            } else {
                out += " = {}";
            }
        }

        members.push_back(parser);
    }
    return members;
}

void Generator::generateStructCode(std::string name,
                                   const std::string &structType,
                                   const std::string &structTypeValue,
                                   const std::vector<VariableData> &members) {
    output += "  struct " + name + " {\n";

    // output.pushIndent();
    if (!structType.empty() && !structTypeValue.empty()) { // structType
        output += "    static VULKAN_HPP_CONST_OR_CONSTEXPR " + structType +
                  " structureType = " + structType + "::" + structTypeValue +
                  ";\n";
        // output.get() << std::endl;
    }
    // structure members
    struct Line {
        std::string text;
        std::string assignment;

        Line(const std::string &text, const std::string &assignment = "{}")
            : text(text), assignment(assignment) {}
    };

    std::vector<Line> lines;

    auto it = members.begin();
    const auto hasConverted = [&]() {
        if (!cfg.genStructProxy) {
            return false;
        }
        std::string id = it->identifier();
        if (!it->isPointer() && it->type() == "uint32_t" && id.ends_with("Count")) {
            auto n = std::next(it);
            if (n != members.end() && n->isPointer()) {
                std::string substr = id.substr(0, id.size() - 5);
                // std::cout << "  >>> " << id << " -> " << substr << ", " <<
                // n->identifier() << std::endl;
                if (std::regex_match(n->identifier(),
                                     std::regex(".*" + substr + ".*",
                                                std::regex_constants::icase))) {
                    //  std::cout << "  MATCH" << std::endl;
                    //                    lines.push_back(Line{"// " +
                    //                    it->toString() + ";"});
                    //                    lines.push_back(Line{"// " +
                    //                    n->toString() + ";"});
                    auto var = *n;
                    var.removeLastAsterisk();
                    lines.push_back(Line{"union {", ""});
                    lines.push_back(Line{"  struct {", ""});
                    lines.push_back(Line{"    " + it->toString() + ";", ""});
                    lines.push_back(Line{"    " + n->toString() + ";", ""});
                    lines.push_back(Line{"  };", ""});
                    lines.push_back(Line{"  ArrayProxy<" + var.fullType() +
                                         "> " + substr + "Proxy"});
                    lines.push_back(Line{"};", ""});
                    it = n;
                    return true;
                }
            }
        }
        return false;
    };

    while (it != members.end()) {
        if (!hasConverted()) {
            lines.push_back(Line{it->toString()});
        }
        it++;
    }

    for (const auto &l : lines) {
        output += "    " + l.text;
        if (!l.assignment.empty()) {
            output += " = " + l.assignment + ";";
        }
        output += "\n";
    }
    output += "\n";

    if (cfg.genStructNoinit) {

        std::string alt = "Noinit";
        output += "    struct " + alt + " {\n";

        for (const auto &l : lines) {
            output += "      " + l.text;
            if (!l.assignment.empty()) {
                output += ";";
            }
            output += "\n";
        }

        output += "      " + alt + "() = default;\n";

        output += "      operator " + name +
                  " const&() const { return *std::bit_cast<const " + name +
                  "*>(this); }\n";
        output += "      operator " + name + " &() { return *std::bit_cast<" +
                  name + "*>(this); }\n";
        output += "    };\n";
    }

    output += "    operator " + cfg.vkname.get() + "::" + name +
              "*() { return this; }\n";

    output += "    operator Vk" + name +
              " const&() const { return *std::bit_cast<const Vk" + name +
              "*>(this); }\n";
    output += "    operator Vk" + name + " &() { return *std::bit_cast<Vk" +
              name + "*>(this); }\n";

    output += "  };\n";
}

void Generator::generateUnionCode(std::string name,
                                  const std::vector<VariableData> &members) {
    output += "  union " + name + " {\n";

    // output.pushIndent();
    //  union members
    for (const auto &m : members) {
        output += "    " + m.toString() + ";\n";
    }
    // output.get() << std::endl;

    output += "    operator " + cfg.vkname.get() + "::" + name +
              "*() { return this; }\n";

    output += "    operator Vk" + name +
              " const&() const { return *std::bit_cast<const Vk" + name +
              "*>(this); }\n";
    output += "    operator Vk" + name + " &() { return *std::bit_cast<Vk" +
              name + "*>(this); }\n";

    // output.popIndent();
    output += "  };";
}

void Generator::generateStruct(std::string name, const std::string &structType,
                               const std::string &structTypeValue,
                               const std::vector<VariableData> &members,
                               const std::vector<std::string> &typeList,
                               bool structOrUnion) {
    auto it = structs.find(name);

    genOptional(name, [&] {
        strStripVk(name);

        if (structOrUnion) {
            generateStructCode(name, structType, structTypeValue, members);
        } else {
            generateUnionCode(name, members);
        }

        if (it != structs.end()) {
            for (const auto &a : it->second.aliases) {
                output += "  using " + strStripVk(a) + " = " + name + ";\n";
                generatedStructs.emplace(strStripVk(a));
            }
        }

        generatedStructs.emplace(name);
    });
}

void Generator::parseStruct(XMLElement *node, std::string name,
                            bool structOrUnion) {
    if (const char *aliasAttrib = node->Attribute("alias")) {
        return;
    }

    std::string structType{}, structTypeValue{}; // placeholders
    std::vector<VariableData> members =
        parseStructMembers(node, structType, structTypeValue);

    std::vector<std::string> typeList;
    for (const auto &m : members) {
        std::string t = m.type();
        if (isStructOrUnion("Vk" + t)) {
            typeList.push_back(t);
        }
    }
    const auto hasAllDeps = [&](const std::vector<std::string> &types) {
        for (const auto &t : types) {
            if (isStructOrUnion("Vk" + t)) {
                const auto &it = generatedStructs.find(t);
                if (it == generatedStructs.end()) {
                    return false;
                }
            }
        }
        return true;
    };

    if (!hasAllDeps(typeList) && name != "VkBaseInStructure" &&
        name != "VkBaseOutStructure") {
        GenStructTempData d;
        d.name = name;
        d.node = node;
        d.structOrUnion = structOrUnion;
        for (const auto &t : typeList) {
            d.typeList.push_back(t);
        }
        // std::cout << d.name << std::endl;
        genStructStack.push_back(d);
        hasAllDeps(typeList);
        return;
    }

    generateStruct(name, structType, structTypeValue, members, typeList,
                   structOrUnion);

    for (auto it = genStructStack.begin(); it != genStructStack.end(); ++it) {
        if (hasAllDeps(it->typeList)) {
            std::string structType{}, structTypeValue{};
            std::vector<VariableData> members =
                parseStructMembers(it->node, structType, structTypeValue);
            generateStruct(it->name, structType, structTypeValue, members,
                           it->typeList, it->structOrUnion);
            it = genStructStack.erase(it);
            --it;
        }
    }
}

void Generator::parseEnumExtend(XMLElement &node, const char *protect,
                                bool flagSupported) {
    const char *extends = node.Attribute("extends");
    const char *bitpos = node.Attribute("bitpos");
    const char *name = node.Attribute("name");
    const char *alias = node.Attribute("alias");

    if (extends && name) {
        auto it = enums.find(extends);
        if (it != enums.end()) {
            EnumValue data;
            data.name = name;
            data.bitpos = bitpos;
            data.protect = protect;
            data.supported = flagSupported;
            data.alias = alias;

            bool dup = false;
            for (auto &v : it->second.extendValues) {
                if (std::string_view{v.name} == std::string_view{name}) {
                    dup = true;
                    break;
                }
            }

            if (!dup) {
                it->second.extendValues.push_back(data);
            }

        } else {
            std::cerr << "err: Cant find enum: " << extends << std::endl;
        }
    }
}

void Generator::generateEnum(std::string name, XMLNode *node,
                             const std::string &bitmask) {
    auto it = enums.find(name);
    if (it == enums.end()) {
        std::cerr << "cant find " << name << "in enums" << std::endl;
        return;
    }

    auto alias = it->second.aliases;

    std::string ext = name;
    if (!bitmask.empty()) {
        ext = std::regex_replace(ext, std::regex("FlagBits"), "Flags");
    }

    genOptional(ext, [&] {
        name = toCppStyle(name, true);
        std::string enumStr =
            bitmask.empty()
                ? name
                : std::regex_replace(name, std::regex("FlagBits"), "Bit");

        std::vector<std::pair<std::string, const char *>> values;

        // iterate contents of <enums>, filter only <enum> children
        for (XMLElement *e : Elements(node) | ValueFilter("enum")) {
            const char *name = e->Attribute("name");
            const char *alias = e->Attribute("alias");
            if (name) {
                values.push_back(std::make_pair(name, alias));
            }
        }

        std::string optionalInherit = "";
        if (!bitmask.empty()) {
            optionalInherit += " : " + bitmask;
        }
        output += "  enum class " + name + optionalInherit +
                  " {\n"; // + " // " + cname + " - " + tag);
        // output.pushIndent();

        std::vector<std::pair<std::string, const char *>> enumMembersList;
        const auto genEnumValue = [&](std::string &output, std::string value,
                                      bool last, const char *protect,
                                      bool add) {
            std::string cpp = enumConvertCamel(enumStr, value);
            // strRemoveTag(cpp);
            std::string comma = "";
            if (!last) {
                comma = ",";
            }
            bool dup = false;
            for (auto &e : enumMembersList) {
                if (e.first == cpp) {
                    dup = true;
                    break;
                }
            }

            if (!dup) {
                output += "    " + cpp + " = " + value + comma + "\n";
                if (add) {
                    enumMembersList.push_back(std::make_pair(cpp, protect));
                }
            }
        };

        size_t extSize = it->second.extendValues.size();
        for (size_t i = 0; i < values.size(); ++i) {
            std::string c = values[i].first;
            genEnumValue(output, c, i == values.size() - 1 && extSize == 0,
                         nullptr, values[i].second == nullptr);
        }

        for (size_t i = 0; i < extSize; ++i) {
            const auto &v = it->second.extendValues[i];
            if (!v.supported) {
                continue;
            }

            if (v.protect) {
                withProtect(output, v.protect, [&](std::string &output) {
                    genEnumValue(output, v.name.data(), i == extSize - 1,
                                 v.protect, v.alias == nullptr);
                });
            } else {
                genEnumValue(output, v.name.data(), i == extSize - 1, nullptr,
                             v.alias == nullptr);
            }
        }

        // output.popIndent();
        output += "  };\n";

        output +=
            "  VULKAN_HPP_INLINE std::string to_string(" + name + " value) {\n";
        // output.pushIndent();
        output += "    switch (value) {\n";
        // output.pushIndent();

        for (const auto &m : enumMembersList) {
            std::string str = m.first.c_str();
            strStripPrefix(str, "e");
            std::string code = "    case " + name + "::" + m.first +
                               ": return \"" + str + "\";\n";
            withProtect(output, m.second,
                        [&](std::string &output) { output += code; });
        }

        output += "    default: return \"invalid ( \" + "
                  "toHexString(static_cast<uint32_t>(value)) + \" )\";\n";

        // output.popIndent();
        output += "    }\n";
        // output.popIndent();
        output += "  }\n";

        for (auto &a : alias) {
            std::string n = toCppStyle(a, true);
            output += "  using " + n + " = " + name + ";\n";
        }

        enumMembers[name] = enumMembersList;
    });
}

void Generator::genEnums(XMLNode *node) {

    const char *name = node->ToElement()->Attribute("name");
    if (!name) {
        std::cerr << "Can't get name of enum" << std::endl;
        return;
    }

    const char *type = node->ToElement()->Attribute("type");
    if (!type) {
        return;
    }
    bool isBitmask = std::string_view(type) == "bitmask";
    bool isEnum = std::string_view(type) == "enum";
    std::string bitmask;
    if (isBitmask) {
        isEnum = true;

        auto it = flags.find(name);
        if (it != flags.end()) {
            bitmask = it->second.name;

        } else {
            bitmask = name;
            bitmask =
                std::regex_replace(bitmask, std::regex("FlagBits"), "Flags");
        }
    }

    if (isEnum) {
        generateEnum(name, node, bitmask);
    }
}

void Generator::genFlags() {
    std::cout << "Generating enum flags" << std::endl;

    for (auto &e : flags) {
        std::string l = toCppStyle(e.second.name, true);
        std::string r = toCppStyle(e.first, true);

        r = std::regex_replace(r, std::regex("Flags"), "FlagBits");

        genOptional(e.second.name, [&] {
            if (e.second.alias) {
                output += "  using " + l + " = " + r + ";\n";
            } else {
                std::string _l =
                    std::regex_replace(l, std::regex("Flags"), "FlagBits");
                if (!e.second.hasRequire &&
                    enumMembers.find(_l) == enumMembers.end()) {
                    std::string _r = e.first;
                    output += "  enum class " + _l + " : " + _r + " {\n";
                    output += "  };\n";
                }
                output += "  using " + l + " = Flags<" + r + ">;\n";

                bool hasInfo = false;
                const auto it = enumMembers.find(r);
                if (it != enumMembers.end()) {
                    hasInfo = !it->second.empty();
                }

                if (hasInfo) {
                    output += "  template <>\n";
                    output += "  struct FlagTraits<" + r + "> {\n";
                    // output.pushIndent();
                    output += "    enum : VkFlags {\n";
                    // output.pushIndent();

                    std::string flags = "";

                    // output.pushIndent();
                    const auto &members = it->second;
                    for (size_t i = 0; i < members.size(); ++i) {
                        const auto &pair = members[i];
                        std::string member =
                            "VkFlags(" + r + "::" + pair.first + ")";
                        if (i != 0) {
                            member = "| " + member;
                        }
                        withProtect(flags, pair.second,
                                    [&](std::string &flags) {
                                        flags += "        " + member + "\n";
                                    });
                    }
                    // output.popIndent();

                    if (flags.empty()) {
                        flags = "      allFlags = 0\n";
                    } else {
                        output += "      allFlags = \n";
                        output += flags;
                    }

                    // output.popIndent();
                    output += "    };\n";
                    // output.popIndent();
                    output += "  };\n";

                    const auto genOperator = [&](const std::string &op,
                                                 const std::string &body) {
                        std::string sig =
                            "  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR " + l +
                            " operator" + op + "(";

                        output += sig + r + " bit0, ";
                        for (int i = 0; i < sig.size(); i++) {
                            output += " ";
                        }
                        output += r + " bit1) VULKAN_HPP_NOEXCEPT {\n";
                        output += "    " + body;
                        output += "  }\n";
                    };

                    genOperator("|", "return " + l + "(bit0) | bit1;\n");
                    genOperator("&", "return " + l + "(bit0) & bit1;\n");
                    genOperator("^", "return " + l + "(bit0) ^ bit1;\n");

                    output += format(R"(
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR {0} operator~({1} bits) {
    return ~({0}(bits));
  }

                        )",
                                     l, r);
                }
            }
        });
    }

    std::cout << "Generating enum flags done" << std::endl;
}

void Generator::parseTypeDeclarations(XMLNode *node) {
    std::cout << "Parsing declarations" << std::endl;

    std::vector<std::pair<std::string_view, std::string_view>> handleBuffer;

    // iterate contents of <types>, filter only <type> children
    for (XMLElement *type : Elements(node) | ValueFilter("type")) {
        const char *cat = type->Attribute("category");
        if (!cat) {
            // warn?
            continue;
        }
        const char *name = type->Attribute("name");

        if (strcmp(cat, "enum") == 0) {
            if (name) {
                const char *alias = type->Attribute("alias");
                if (alias) {
                    auto it = enums.find(alias);
                    if (it == enums.end()) {
                        enums.emplace(
                            alias,
                            EnumDeclaration{
                                .aliases = std::vector<std::string>{{name}}});
                    } else {
                        it->second.aliases.push_back(name);
                    }
                } else {
                    enums.emplace(name, EnumDeclaration{});
                }
            }
        } else if (strcmp(cat, "bitmask") == 0) {
            // typedef VkFlags ...

            const char *name = type->Attribute("name");
            const char *aliasAttrib = type->Attribute("alias");
            std::string alias = aliasAttrib ? aliasAttrib : "";
            const char *reqAttrib = type->Attribute("requires");
            std::string req; // rename
            bool hasReq = false;
            bool hasAlias = false;

            if (!aliasAttrib) {
                XMLElement *nameElem = type->FirstChildElement("name");
                if (!nameElem) {
                    std::cerr << "Error: bitmap has no name" << std::endl;
                    continue;
                }
                name = nameElem->GetText();
                if (!name) {
                    std::cerr << "Error: bitmas alias has no name" << std::endl;
                    continue;
                }

                if (reqAttrib) {
                    req = reqAttrib;
                    hasReq = true;
                } else {
                    const char *bitAttrib = type->Attribute("bitvalues");

                    if (bitAttrib) {
                        req = bitAttrib;
                    } else {
                        req = name;
                    }
                }
            } else {
                if (!name) {
                    std::cerr << "Error: bitmap alias has no name" << std::endl;
                    continue;
                }
                req = alias;
                hasAlias = true;
            }

            flags.emplace(req, FlagData{.name = name,
                                        .hasRequire = hasReq,
                                        .alias = hasAlias});

        } else if (strcmp(cat, "handle") == 0) {
            XMLElement *nameElem = type->FirstChildElement("name");
            if (nameElem) {
                const char *name = nameElem->GetText();
                if (!name || std::string{name}.empty()) {
                    std::cerr << "Missing name in handle node" << std::endl;
                    continue;
                }
                const char *parent = type->Attribute("parent");
                const char *alias = type->Attribute("alias");
                HandleData d{.parent = nullptr};
                if (alias) {
                    d.alias = alias;
                }
                if (parent) {
                    handleBuffer.push_back(std::make_pair(name, parent));
                }
                handles.emplace(name, d);
            }
        } else if (strcmp(cat, "struct") == 0 || strcmp(cat, "union") == 0) {
            if (name) {
                const char *alias = type->Attribute("alias");
                StructData d;
                d.type = (strcmp(cat, "struct") == 0) ? StructData::VK_STRUCT
                                                      : StructData::VK_UNION;

                if (alias) {
                    const auto &it = structs.find(alias);
                    if (it == structs.end()) {
                        d.node = nullptr;
                        d.aliases.push_back(name);
                        structs.emplace(alias, d);
                    } else {
                        it->second.aliases.push_back(name);
                    }
                } else {
                    const auto &it = structs.find(name);
                    if (it == structs.end()) {
                        d.node = type;
                        structs.emplace(name, d);
                    } else {
                        it->second.node = type;
                    }
                }
            }
        }
    }

    for (auto &h : handleBuffer) {
        const auto &t = handles.find(h.first);
        const auto &p = handles.find(h.second);
        if (t != handles.end() && p != handles.end()) {
            t->second.parent = &(*p);
        }
    }

    for (auto &h : handles) {
        h.second.superclass = getHandleSuperclass(h.second);
    }

    std::cout << "Parsing declarations done" << std::endl;
}

void Generator::generateStructDecl(const std::string &name,
                                   const StructData &d) {
    genOptional(name, [&] {
        std::string cppname = strStripVk(name);

        if (d.type == StructData::VK_STRUCT) {
            output += "  struct " + cppname + ";\n";
        } else {
            output += "  union " + cppname + ";\n";
        }

        for (auto &a : d.aliases) {
            output += "  using " + strStripVk(a) + " = " + cppname + ";\n";
        }
    });
}

void Generator::generateClassDecl(const std::string &name) {
    genOptional(name, [&] {
        std::string className = toCppStyle(name, true);
        //        std::string handle = "m_" + toCppStyle(name);
        //        output += "  class " + className + "Base {\n";
        //        output += "  protected:\n";
        //        output += "    " + name + " " + handle + " = {};\n";
        //        output += "  public:\n";
        //        output += "    operator "
        //                  "Vk" +
        //                  className + "() const {\n";
        //        output += "      return " + handle + ";\n";
        //        output += "    }\n";
        //        output += "  };\n";

        output += "  class " + className + ";\n";
    });
}

void Generator::genTypes() {
    //        static const auto parseType = [&](XMLElement *type) {
    //            const char *cat = type->Attribute("category");
    //            if (!cat) {
    //                return;
    //            }
    //            const char *name = type->Attribute("name");
    //            if (strcmp(cat, "struct") == 0) {
    //                if (name) {
    //                    parseStruct(type, name, true);
    //                }
    //            } else if (strcmp(cat, "union") == 0) {
    //                if (name) {
    //                    parseStruct(type, name, false);
    //                }
    //            }
    //        };

    std::cout << "Generating types" << std::endl;

    output += "  class LibraryLoader;\n";
    for (auto &e : handles) {
        generateClassDecl(e.first.data());
    }
    for (auto &e : structs) {
        generateStructDecl(e.first, e.second);
    }

    output += format(RES_LIB_LOADER, outputLoaderMethods);
    output += "  static LibraryLoader libLoader;\n";
    for (auto &e : handles) {
        generateClass(e.first.data(), e.second);
    }

    for (auto &e : structs) {
        parseStruct(e.second.node->ToElement(), e.first,
                    e.second.type == StructData::VK_STRUCT);
    }

    for (auto it = genStructStack.begin(); it != genStructStack.end(); ++it) {
        std::string structType{}, structTypeValue{}; // placeholders
        std::vector<VariableData> members =
            parseStructMembers(it->node, structType, structTypeValue);
        generateStruct(it->name, structType, structTypeValue, members,
                       it->typeList, it->structOrUnion);
    }

    std::cout << "Generating types done" << std::endl;
}

std::string Generator::generateClassMemberCStyle(const std::string &className,
                                                 const std::string &handle,
                                                 const std::string &protoName,
                                                 const ClassCommandData &m) {
    std::string protoArgs = m.createProtoArguments(className, true);
    std::string innerArgs = m.createPFNArguments(className, handle, true);
    output += "    inline " + m.type + " " + protoName + "(" + protoArgs +
              ") { // C\n";

    // output.pushIndent();
    std::string cmdCall;
    if (m.type != "void") {
        cmdCall += "return ";
    }
    cmdCall += ("m_" + m.name + "(" + innerArgs + ");");
    output += "    " + cmdCall + "\n";
    // output.popIndent();

    output += "  }\n";
    return protoArgs;
}

Generator::MemberNameCategory
Generator::evalMemberNameCategory(MemberContext &ctx) {
    const std::string &name = ctx.protoName;

    bool containsCountVar = containsLengthAttrib(ctx.mdata.params);
    bool arrayFlag = containsCountVar;

    if (name.starts_with("get")) {
        return arrayFlag ? MemberNameCategory::GET_ARRAY
                         : MemberNameCategory::GET;
    }
    if (name.starts_with("allocate")) {
        if (arrayFlag) {
            return MemberNameCategory::ALLOCATE_ARRAY;
        } else {
            return MemberNameCategory::ALLOCATE;
        }
    }
    if (name.starts_with("create")) {
        return arrayFlag ? MemberNameCategory::CREATE_ARRAY
                         : MemberNameCategory::CREATE;
    }
    if (name.starts_with("enumerate")) {
        return MemberNameCategory::ENUMERATE;
    }
    return MemberNameCategory::UNKNOWN;
}

std::vector<Generator::MemberResolver *>
Generator::createMemberResolvers(MemberContext &ctx) {
    std::vector<MemberResolver *> resolvers;
    if (ctx.mdata.params.empty()) {
        std::cerr << " >>> Unhandled: no params" << ctx.mdata.name << std::endl;
        return resolvers;
    }

    VariableArray::reverse_iterator last =
        ctx.mdata.params.rbegin(); // last argument
    MemberNameCategory nameCategory = evalMemberNameCategory(ctx);

    if (ctx.pfnReturn == PFNReturnCategory::VK_OBJECT ||
        ctx.pfnReturn == PFNReturnCategory::OTHER) {
        MemberResolver *resolver = new MemberResolver(ctx);
        resolver->dbgtag = "PFN return";
        resolvers.push_back(resolver);
        return resolvers;
    }

    MemberResolver *resolver = nullptr;
    switch (nameCategory) {
    case MemberNameCategory::GET: {
        std::pair<VariableArray::reverse_iterator,
                  VariableArray::reverse_iterator>
            data;
        if (getLastTwo(ctx, data)) {
            if (isPointerToCArray(data.first->get()->identifier()) &&
                data.second->get()->identifier() ==
                    stripStartingP(data.first->get()->identifier()) +
                        "Size") { // refactor?

                resolvers.push_back(new MemberResolverReturnProxy(ctx));
                resolver = new MemberResolverReturnVectorOfProxy(ctx);
                resolver->dbgtag = "get";
                return resolvers;
            } else if (data.second->get()->identifier() ==
                       data.first->get()->identifier() + "Size") {
                if (isTypePointer(*data.second->get())) {
                    resolver = new MemberResolverReturnVectorData(ctx);
                } else {
                    resolver = new MemberResolverReturnProxy(ctx);
                }
            }
        }

        if (!resolver) { // fallback
            if (strContains(last->get()->suffix(), "*") &&
                !strContains(last->get()->prefix(), "const")) {
                resolver = new MemberResolverStruct(ctx);
            } else {
                resolver = new MemberResolver(ctx);
            }
        }
        resolver->dbgtag = "get";
    } break;
    case MemberNameCategory::GET_ARRAY:
        resolver = new MemberResolverGet(ctx);
        resolver->dbgtag = "get array";
        break;
    case MemberNameCategory::CREATE:
        resolver = new MemberResolverStruct(ctx);
        resolver->dbgtag = "create";
        break;
    case MemberNameCategory::CREATE_ARRAY:
        resolver = new MemberResolverCreateArray(ctx);
        resolver->dbgtag = "create array";
        break;
    case MemberNameCategory::ALLOCATE:
        resolver = new MemberResolverStruct(ctx);
        resolver->dbgtag = "allocate";
        break;
    case MemberNameCategory::ALLOCATE_ARRAY:
        resolver = new MemberResolverAllocateArray(ctx);
        resolver->dbgtag = "allocate array";
        break;
    case MemberNameCategory::ENUMERATE:
        resolver = new MemberResolverEnumerate(ctx);
        break;
    case MemberNameCategory::UNKNOWN:
        // std::cout << ">> HERE" << std::endl;
        if (strContains(last->get()->suffix(), "*") &&
            !strContains(last->get()->prefix(), "const")) {
            resolver = new MemberResolverStruct(ctx);
        } else {
            resolver = new MemberResolver(ctx);
        }
        break;
    }
    resolvers.push_back(resolver);
    return resolvers;
}

VariableData Generator::createClassVar(const std::string_view &type) {
    VariableData data;
    std::string id = toCppStyle(type.data(), true);
    data.setFullType("const ", id, "");
    id[0] = std::tolower(id[0]);
    data.setIdentifier(id);
    data.setReferenceFlag(true);
    return data;
}

void Generator::generateClassMembers(const std::string &className,
                                     const std::string &handle,
                                     const HandleData &data) {

    const std::string &getAddr = data.getAddrCmd.name;

    VariableData super = createClassVar(data.superclass);
    VariableData parent;
    if (data.parent && data.superclass != data.parent->first) {
        parent = createClassVar(data.parent->first);
    } else {
        parent.setSpecialType(VariableData::TYPE_INVALID);
    }

    // PFN function pointers
    for (const ClassCommandData &m : data.members) {
        genOptional(m.name, [&] {
            output += "    PFN_" + m.name + " m_" + m.name + " = {};\n";
        });
    }

    if (!getAddr.empty()) {
        output += "    PFN_" + getAddr + " m_" + getAddr + " = {};\n";
    }

    output += "  public:\n";

    if (!getAddr.empty()) {
        // getProcAddr member
        output += "    template<typename T>\n";
        output += "    inline T getProcAddr(const std::string_view &name) "
                  "const {\n";

        output += "      return std::bit_cast<T>(m_" + getAddr + "(" + handle +
                  ", name.data()));\n";
        output += "    }\n";
    }

    std::string initParams;
    if (super.type() == "LibraryLoader") {
        initParams = "LibraryLoader &libraryLoader = libLoader";
    } else {
        initParams = super.toString();
    }
    //    if (!parent.isInvalid()) {
    //        initParams += ", " + parent.toString();

    //    }
    //    if (!data.createCmd.name.empty()) {
    //        initParams += ", const " + className + "CreateInfo &createInfo";
    //    }
    output += "    void init(" + initParams + ");\n";
    if (super.type() == "LibraryLoader") {
        initParams = "LibraryLoader &libraryLoader";
    }

    outputFuncs += "  void " + className + "::init(" + initParams + ") {\n";

    if (!getAddr.empty()) {
        outputFuncs += "    m_" + getAddr + " = " + super.identifier();
        if (super.type() == "LibraryLoader") {
            outputFuncs += ".getInstanceProcAddr();\n";
        } else {
            outputFuncs +=
                ".getProcAddr<PFN_" + getAddr + ">(\"" + getAddr + "\");\n";
        }
    }

    std::string loadSrc;
    if (data.getAddrCmd.name.empty()) {
        loadSrc = super.identifier() + ".";
    }
    // function pointers initialization
    for (const ClassCommandData &m : data.members) {
        outputFuncs += genOptional(m.name, [&](std::string &output) {
            output += "    m_" + m.name + " = " + loadSrc + "getProcAddr<" +
                      "PFN_" + m.name + ">(\"" + m.name + "\");\n";
        });
    }
    outputFuncs += "  }\n";

    /*
        if (!data.createCmd.name.empty()) {
            outputFuncs += "    PFN_" + data.createCmd.name + " m_" +
                           data.createCmd.name + " = " + super.identifier() +
                           ".getProcAddr<PFN_" + data.createCmd.name + ">(\"" +
                           getAddr + "\");\n";
            MemberContext ctx{
                              .gen = *this,
                              .className = className,
                              .handle = handle,
                              .protoName = std::regex_replace(
                                  toCppStyle(data.createCmd.name),
       std::regex(className),
                                  ""), // filtered prototype name (without vk)
                              .pfnReturn =
       evaluatePFNReturn(data.createCmd.type), .mdata = data.createCmd};

            for (auto &m : ctx.mdata.params) {
                if (m->identifier() == "pCreateInfo") {
                    m->convertToReturn();
                    m->setIdentifier("createInfo");
                }
                if (m->original.type() == "VkAllocationCallbacks") {
                    m->setAltPFN("nullptr");
                }
                if (m->type() == className) {
                    m->convertToReturn();
                }
            }

            MemberResolverInit r(ctx);
            r.generateMemberBody();
        }
    */

    // wrapper functions
    for (const ClassCommandData &m : data.members) {
        // debug
        static const std::vector<std::string> debugNames;
        if (!debugNames.empty()) {
            bool contains = false;
            for (const auto &s : debugNames) {
                if (strContains(m.name, s)) {
                    contains = true;
                    break;
                }
            }
            if (!contains) {
                continue;
            }
        }

        MemberContext ctx{.gen = *this,
                          .className = className,
                          .handle = handle,
                          .protoName = std::regex_replace(
                              toCppStyle(m.name), std::regex(className),
                              ""), // prototype name (without vk)
                          .pfnReturn = evaluatePFNReturn(m.type),
                          .mdata = m};

        generateClassMemberCpp(ctx);
    }
}

std::string_view Generator::getHandleSuperclass(const HandleData &data) {
    if (!data.parent) {
        return "LibraryLoader";
    }
    auto *it = data.parent;
    while (it->second.parent) {
        if (it->first == "VkInstance" ||
            it->first == "VkDevice") { // it->first == "VkPhysicalDevice" ||
            break;
        }
        it = it->second.parent;
    }
    return it->first;
}

void Generator::generateClass(const std::string &name, HandleData &data) {
    genOptional(name, [&] {
        std::string className = toCppStyle(name, true);
        std::string classNameLower = toCppStyle(name);
        std::string handle = "m_" + classNameLower;

        std::string_view superclass = getHandleSuperclass(data);

        // std::cout << "Gen class: " << className << std::endl;
        // std::cout << "  ->superclass: " << superclass << std::endl;

        output += "  class " + className + " {\n";

        output += "  protected:\n";
        output += "    " + name + " " + handle + " = {};\n";

        if (!data.members.empty()) {
            generateClassMembers(className, handle, data);
        }

        output += "  public:\n";

        output +=
            "    VULKAN_HPP_CONSTEXPR         " + className + "() = default;\n";
        output += "    VULKAN_HPP_CONSTEXPR         " + className +
                  "( std::nullptr_t ) VULKAN_HPP_NOEXCEPT {}\n";

        output += "    VULKAN_HPP_TYPESAFE_EXPLICIT " + className + "(Vk" +
                  className + " " + classNameLower +
                  ") VULKAN_HPP_NOEXCEPT {\n";
        output += "      " + handle + " = " + classNameLower + ";\n";
        output += "    }\n";

        output += "    operator "
                  "Vk" +
                  className + "() const {\n";
        output += "      return " + handle + ";\n";
        output += "    }\n";

        output += "  };\n";
    });
}

void Generator::parseCommands(XMLNode *node) {
    std::cout << "Parsing commands" << std::endl;

    std::vector<std::string_view> deviceObjects;
    std::vector<std::string_view> instanceObjects;

    std::vector<ClassCommandData> elementsUnassigned;
    std::vector<ClassCommandData> elementsStatic;

    for (auto &h : handles) {
        if (h.first == "VkDevice" || h.second.superclass == "VkDevice") {
            deviceObjects.push_back(h.first);
        } else if (h.first == "VkInstance" ||
                   h.second.superclass == "VkInstance") {
            instanceObjects.push_back(h.first);
        }
    }

    //    std::cout << "Instance objects:" << std::endl;
    //    for (auto &o : instanceObjects) {
    //        std::cout << "  " << o << std::endl;
    //    }
    //    std::cout << "Device objects:" << std::endl;
    //    for (auto &o : deviceObjects) {
    //        std::cout << "  " << o << std::endl;
    //    }

    std::map<std::string_view, std::vector<std::string_view>> aliased;

    const auto addCommand = [&](XMLElement *element,
                                const std::string &className,
                                const ClassCommandData &command) {
        auto handle = findHandle("Vk" + className);
        handle->second.members.push_back(command);

        auto alias = aliased.find(command.name);
        if (alias != aliased.end()) {
            for (auto &a : alias->second) {
                ClassCommandData data = parseClassMember(element);
                data.name = a;
                handle->second.members.push_back(data);
            }
        }
    };

    const auto assignGetProc = [&](const std::string &className,
                                   const ClassCommandData &command) {
        if (command.name == "vkGet" + className + "ProcAddr") {
            auto handle = findHandle("Vk" + className);
            handle->second.getAddrCmd = command;
            return true;
        }
        return false;
    };

    const auto assignCommand = [&](XMLElement *element,
                                   const ClassCommandData &command) {
        std::smatch matches;

        if (std::regex_search(command.name, matches,
                              std::regex("vkCreate(.+)"))) {
            // std::string name = matches[1].str();
            std::string name = command.params.rbegin()->get()->original.type();
            //            std::cout << "match: " << command.name << std::endl;
            try {
                auto handle = findHandle(name);
                std::cout << "Handle: " << name << " -> "
                          << handle->second.superclass << std::endl;

                if (!handle->second.createCmd.name.empty()) {
                    std::cout << "  has multiple" << std::endl;
                }
                handle->second.createCmd = command;
                //                for (const auto &m : command.params) {
                //                    std::cout << m->toString() << std::endl;
                //                }
                //                std::cout << "}" << std::endl;
                if (!handle->second.superclass.empty()) {
                    ClassCommandData data = command;
                    if (handle->second.superclass == "LibraryLoader") {
                        loader.members.push_back(data);
                    } else {
                        addCommand(element,
                                   strStripVk(handle->second.superclass.data()),
                                   data);
                    }
                }

                return true;
            } catch (std::runtime_error e) {
                std::cerr << "can't assign vkcreate: " << name << " (from "
                          << command.name << ")" << std::endl;
            }
        }

        return assignGetProc("Instance", command) ||
               assignGetProc("Device", command);
    };

    for (XMLElement *commandElement : Elements(node) | ValueFilter("command")) {
        const char *alias = commandElement->Attribute("alias");
        if (alias) {
            const char *name = commandElement->Attribute("name");
            if (!name) {
                std::cerr << "Command has no name" << std::endl;
                continue;
            }
            auto it = aliased.find(alias);
            if (it == aliased.end()) {
                aliased.emplace(alias, std::vector<std::string_view>{{name}});
            } else {
                it->second.push_back(name);
            }
        }
    }

    // iterate contents of <commands>, filter only <command> children
    for (XMLElement *element : Elements(node) | ValueFilter("command")) {

        const char *alias = element->Attribute("alias");
        if (alias) {
            continue;
        }

        const ClassCommandData &command = parseClassMember(element);

        if (assignCommand(element, command)) {
            continue;
        }

        if (command.params.size() > 0) {
            // first argument of a command
            std::string_view first = command.params.at(0)->original.type();
            if (isInContainter(deviceObjects,
                               first)) { // command is for device
                addCommand(element, "Device", command);
            } else if (isInContainter(instanceObjects,
                                      first)) { // command is for instance
                addCommand(element, "Instance", command);
            } else {
                elementsStatic.push_back(command);
            }
        } else {
            addCommand(element, "Instance", command);
        }
    }

    std::cout << "Parsing commands done" << std::endl;

    if (!elementsUnassigned.empty()) {
        std::cout << "Unassigned commands: " << elementsUnassigned.size()
                  << std::endl;
        for (auto &c : elementsUnassigned) {
            std::cout << "  " << c.name << std::endl;
        }
    }
    //    std::cout << "Static commands: " << elementsStatic.size() <<
    //    std::endl; for (auto &c : elementsStatic) {
    //        std::cout << "  " << c.name << std::endl;
    //    }

    for (const auto &m : loader.members) {

        for (auto &p : m.params) {
            if (p->identifier() == "pCreateInfo") {
                p->setIdentifier("createInfo");
                p->setReferenceFlag(true);
                p->removeLastAsterisk();
            }
            //            if (p->original.type() == "VkAllocationCallbacks") {
            //                p->setAltPFN("nullptr");
            //            }
        }

        m.params.rbegin()->get()->convertToReturn();

        MemberContext ctx{.gen = *this,
                          .className = "LibraryLoader",
                          .handle = "",
                          .protoName = std::regex_replace(
                              toCppStyle(m.name), std::regex("LibraryLoader"),
                              ""), // filtered prototype name (without vk)
                          .pfnReturn = evaluatePFNReturn(m.type),
                          .mdata = m};

        MemberResolverInit r(ctx);

        outputLoaderMethods += r.generateDeclaration();
        outputFuncs += r.generateDefinition();
    }
}

Generator::Generator() {
    root = nullptr;

    cfg.genStructNoinit = true;
    cfg.genStructProxy = true;

    cfg.vkname.value = "vk20";
    cfg.vkname.define = "VULKAN_HPP_NAMESPACE";
    cfg.vkname.usesDefine = true;
}

void Generator::load(const std::string &xmlPath) {
    if (isLoaded()) {
        std::cout << "Already loaded!" << std::endl;
        return;
    }

    std::cout << "Gen load xml: " << xmlPath << std::endl;
    if (XMLError e = doc.LoadFile(xmlPath.c_str()); e != XML_SUCCESS) {
        throw std::runtime_error("XML load failed: " + std::to_string(e) +
                                 " (file: " + xmlPath + ")");
    }

    root = doc.RootElement();
    if (!root) {
        throw std::runtime_error("XML file is empty");
    }

    // specifies order of parsing vk.xml registry
    static const std::vector<OrderPair> loadOrder{
        {"platforms",
         std::bind(&Generator::parsePlatforms, this, std::placeholders::_1)},
        {"tags", std::bind(&Generator::parseTags, this, std::placeholders::_1)},
        {"types", std::bind(&Generator::parseTypeDeclarations, this,
                            std::placeholders::_1)},
        {"feature",
         std::bind(&Generator::parseFeature, this, std::placeholders::_1)},
        {"extensions",
         std::bind(&Generator::parseExtensions, this, std::placeholders::_1)},
        {"commands",
         std::bind(&Generator::parseCommands, this, std::placeholders::_1)}};

    std::string prev;
    // call each function in rootParseOrder with corresponding XMLNode
    for (auto &key : loadOrder) {
        for (XMLNode &n : Elements(root)) {
            if (key.first == n.Value()) {
                key.second(&n); // call function(XMLNode*)
            }
        }
    }
}

void Generator::generate() {

    output.clear();
    outputFuncs.clear();
    generatedStructs.clear();
    genStructStack.clear();
    loader.members.clear();
    outputLoaderMethods.clear();

    std::cout << "generating" << std::endl;
    std::cout << "cfg.genStructNoinit: " << cfg.genStructNoinit << std::endl;
    std::cout << "cfg.genStructProxy: " << cfg.genStructProxy << std::endl;


    const std::string &out = generateFile();

    std::ofstream file;
    file.open(outputFilePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Can't open file: " + outputFilePath);
    }

    file << out;

    file.flush();
    file.close();
}
