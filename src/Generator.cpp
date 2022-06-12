#include "Generator.hpp"

#include <fstream>

static std::string
regex_replace(const std::string &input, const std::regex &regex,
              std::function<std::string(std::smatch const &match)> format) {

    std::ostringstream output;
    std::sregex_iterator begin(input.begin(), input.end(), regex), end;
    for (; begin != end; begin++) {
        output << begin->prefix() << format(*begin);
    }
    output << input.substr(input.size() - begin->position());
    return output.str();
}

void Generator::bindVars(Variables &vars) const {
    const auto findVar = [&](const std::string &id) {
        for (auto &p : vars) {
            if (p->original.identifier() == id) {
                return p;
            }
        }
        std::cerr << "can't find param (" + id + ")" << std::endl;
        return std::make_shared<VariableData>(VariableData::TYPE_INVALID);
    };

    for (auto &p : vars) {
        const std::string &len = p->getLenAttribIdentifier();
        if (!len.empty()) {
            const auto &var = findVar(len);
            if (!var->isInvalid()) {
                p->bindLengthVar(var);
            }
        }
    }
}

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

bool Generator::idLookup(const std::string &name,
                         std::string_view &protect) const {
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
                            std::function<void(std::string &)> function) const {
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
                       std::function<void(std::string &)> function) const {
    std::string output;
    std::string_view protect;
    if (!idLookup(name, protect)) {
        return "";
    }

    withProtect(output, protect.data(), function);

    return output;
}

std::string Generator::strRemoveTag(std::string &str) const {
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

std::string Generator::strWithoutTag(const std::string &str) const {
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
Generator::snakeToCamelPair(std::string str) const {
    std::string suffix = strRemoveTag(str);
    std::string out = convertSnakeToCamel(str);

    out = std::regex_replace(out, std::regex("bit"), "Bit");
    out = std::regex_replace(out, std::regex("Rgba10x6"), "Rgba10X6");
    out = std::regex_replace(out, std::regex("1d"), "1D");
    out = std::regex_replace(out, std::regex("2d"), "2D");
    out = std::regex_replace(out, std::regex("3d"), "3D");
    if (out.size() >= 2) {
        for (int i = 0; i < out.size() - 1; i++) {
            char c = out[i];
            bool cond = c == 'r' || c == 'g' || c == 'b' || c == 'a';
            if (cond && std::isdigit(out[i + 1])) {
                out[i] = std::toupper(c);
            }
        }
    }

    return std::make_pair(out, suffix);
}

std::string Generator::snakeToCamel(const std::string &str) const {
    const auto &p = snakeToCamelPair(str);
    return p.first + p.second;
}

std::string Generator::enumConvertCamel(const std::string &enumName,
                                        std::string value) const {

    bool dbg = value == "VK_PRESENT_MODE_IMMEDIATE_KHR";
    if (dbg) {
        int dummy;
    }

    std::string enumSnake = enumName;
    std::string tag = strRemoveTag(enumSnake);
    if (!tag.empty()) {
        tag = "_" + tag;
    }
    enumSnake = camelToSnake(enumSnake);

    strStripPrefix(value, "VK_");

    const auto &tokens = split(enumSnake, "_");

    for (auto it = tokens.begin(); it != tokens.end(); it++) {
        std::string token = *it + "_";
        if (!value.starts_with(token)) {
            break;
        }
        value.erase(0, token.size());
    }

    if (value.ends_with(tag)) {
        value.erase(value.size() - tag.size());
    }

    for (auto it = tokens.rbegin(); it != tokens.rend(); it++) {
        std::string token = "_" + *it;
        if (!value.ends_with(token)) {
            break;
        }
        value.erase(value.size() - token.size());
    }

    value = strFirstUpper(snakeToCamel(value));
    return "e" + value;
}

void Generator::generateOutput() {
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

    generateOutput();

    std::string out;

    std::string fileProtect = cfg.fileProtect;
    out += "#ifndef " + fileProtect + "\n";
    out += "#define " + fileProtect + "\n";

    out += format(RES_HEADER, headerVersion);

    out += genMacro(cfg.mcName);

    if (cfg.mcName.usesDefine) {
        out += format(R"(
#define VULKAN_HPP_NAMESPACE_STRING   VULKAN_HPP_STRINGIFY( {0} )
)",
                      cfg.mcName.get());
    } else {
        out +=
            "#define VULKAN_HPP_NAMESPACE_STRING   VULKAN_HPP_STRINGIFY( \"" +
            cfg.mcName.value + "\" )\n";
    }
    out += "\n";
    out += "namespace " + cfg.mcName.get() + " {\n";

    out += RES_BASE_TYPES;
    out += "\n";
    out += format(R"(

template <typename EnumType, EnumType value>
struct CppType
{};

template <typename Type>
struct isVulkanHandleType
{
static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = false;
};

{INLINE} std::string toHexString(uint32_t value)
{
    std::stringstream stream;
    stream << std::hex << value;
    return stream.str();
}
)");
    out += "\n";

    out += RES_ARRAY_PROXY_HPP;
    out += RES_FLAGS_HPP;
    out += format(R"(
  template <typename RefType>
  class Optional {
  public:
    Optional( RefType & reference ) {NOEXCEPT}
    {
      m_ptr = &reference;
    }
    Optional( RefType * ptr ) {NOEXCEPT}
    {
      m_ptr = ptr;
    }
    Optional( std::nullptr_t ) {NOEXCEPT}
    {
      m_ptr = nullptr;
    }

    operator RefType *() const {NOEXCEPT}
    {
      return m_ptr;
    }
    RefType const * operator->() const {NOEXCEPT}
    {
      return m_ptr;
    }
    explicit operator bool() const {NOEXCEPT}
    {
      return !!m_ptr;
    }

  private:
    RefType * m_ptr;
  };

)");
    out += "\n";
    out += RES_RESULT_VALUE_STRUCT_HPP;
    out += "\n";

    out += this->output + "\n";

    out += RES_ERROR_HPP;
    out += "\n";

    out += R"(
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
    out += "\n";

    out += RES_RESULT_VALUE_HPP;
    out += "\n";

    out += outputFuncs; // class methods
    out += "\n";

    out += "}\n";
    out += "#endif //" + fileProtect + "\n";

    return out;
}

Generator::Variables Generator::parseStructMembers(XMLElement *node, std::string &structType,
                              std::string &structTypeValue) {
    Variables members;
    // iterate contents of <type>, filter only <member> children
    for (XMLElement *member : Elements(node) | ValueFilter("member")) {        

        XMLVariableParser parser{member, *this}; // parse <member>

        std::string type = parser.type();
        std::string name = parser.identifier();

        if (const char *values = member->ToElement()->Attribute("values")) {
            std::string value = enumConvertCamel(type, values);
            parser.setAssignment(type + "::" + value);
            if (name == "sType") { // save sType information for structType
                structType = type;
                structTypeValue = value;
            }
        }

        members.push_back(std::make_shared<VariableData>(parser));
    }
    bindVars(members);
    return members;
}

void Generator::generateStructCode(std::string name,
                                   const std::string &structType,
                                   const std::string &structTypeValue,
                                   const Variables &members) {
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

        Line(const std::string &text, const std::string &assignment)
            : text(text), assignment(assignment) {}
    };

    std::vector<Line> lines;

    auto it = members.begin();
    const auto hasConverted = [&]() {
        if (!cfg.genStructProxy) {
            return false;
        }
        std::string id = it->get()->identifier();
        if (!it->get()->isPointer() && it->get()->type() == "uint32_t" &&
            id.ends_with("Count")) {
            auto n = std::next(it);
            if (n != members.end() && n->get()->isPointer()) {
                std::string substr = id.substr(0, id.size() - 5);
                // std::cout << "  >>> " << id << " -> " << substr << ", " <<
                // n->identifier() << std::endl;
                if (std::regex_match(n->get()->identifier(),
                                     std::regex(".*" + substr + ".*",
                                                std::regex_constants::icase))) {
                    auto var = *n;
                    var->removeLastAsterisk();
                    lines.push_back(Line{"union {", ""});
                    lines.push_back(Line{"  struct {", ""});
                    lines.push_back(Line{"    " + it->get()->toString() + ";", ""});
                    lines.push_back(Line{"    " + n->get()->toString() + ";", ""});
                    lines.push_back(Line{"  };", ""});
                    lines.push_back(Line{"  ArrayProxy<" + var->fullType() +
                                         "> " + substr + "Proxy", "{}"});
                    lines.push_back(Line{"};", ""});
                    it = n;
                    return true;
                }
            }
        }
        return false;
    };

    while (it != members.end()) {
        // if (!hasConverted()) {
            std::string assignment = "{}";
            if (it->get()->original.type() == "VkStructureType") {
                assignment = structTypeValue.empty()?
                cfg.mcName.get() + "::StructureType::eApplicationInfo" :
                "structureType";
            }
            lines.push_back(Line{it->get()->toString(), assignment});
        // }
        it++;
    }
    std::string funcs;

    for (const auto &m : members) {
        if (m->hasLengthVar()) {
            std::string arr = m->identifier();
            if (arr.size() >= 3 && arr.starts_with("pp") && std::isupper(arr[2])) {
                arr = arr.substr(1);
            }
            else if (arr.size() >= 2 && arr.starts_with("p") && std::isupper(arr[1])) {
                arr = arr.substr(1);
            }
            arr = strFirstUpper(arr);
            std::string id = strFirstLower(arr);

            // funcs += "    // " + m->originalFullType() + "\n";
            std::string modif;
            if (m->original.type() == "void") {
                funcs += "    template <typename DataType>\n";
                m->setType("DataType");
                modif = " * sizeof(DataType)";
            }

            m->removeLastAsterisk();
            funcs += "    " + name + "& set" + arr + "(ArrayProxyNoTemporaries<" + m->fullType() + "> const &" + id + ") {\n";
            funcs += "      " + m->getLengthVar()->identifier() + " = " + id + ".size()" + modif + ";\n";
            funcs += "      " + m->identifier() + " = " + id + ".data();\n";
            funcs += "      return *this;\n";
            funcs += "    }\n\n";
        }
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

    output +=
        format("    operator {NAMESPACE}::{0}*() { return this; }\n", name);

    output += "    operator Vk" + name +
              " const&() const { return *std::bit_cast<const Vk" + name +
              "*>(this); }\n";
    output += "    operator Vk" + name + " &() { return *std::bit_cast<Vk" +
              name + "*>(this); }\n";

    output += funcs;

    output += "  };\n";
}

void Generator::generateUnionCode(std::string name,
                                  const Variables &members) {
    output += "  union " + name + " {\n";

    //  union members
    for (const auto &m : members) {
        output += "    " + m->toString() + ";\n";
    }

    output +=
        format("    operator {NAMESPACE}::{0}*() { return this; }\n", name);

    output += "    operator Vk" + name +
              " const&() const { return *std::bit_cast<const Vk" + name +
              "*>(this); }\n";
    output += "    operator Vk" + name + " &() { return *std::bit_cast<Vk" +
              name + "*>(this); }\n";

    output += "  };";
}

void Generator::generateStruct(std::string name, const std::string &structType,
                               const std::string &structTypeValue,
                               const Variables &members,
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
    Variables members =
        parseStructMembers(node, structType, structTypeValue);

    std::vector<std::string> typeList;
    for (const auto &m : members) {
        std::string t = m->type();
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
            Variables members =
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
    auto en = enums.find(name);
    if (en == enums.end()) {
        std::cerr << "cant find " << name << "in enums" << std::endl;
        return;
    }

    auto alias = en->second.aliases;

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

        auto &enumMembersList = en->second.members;
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

        size_t extSize = en->second.extendValues.size();
        for (size_t i = 0; i < values.size(); ++i) {
            std::string c = values[i].first;
            genEnumValue(output, c, i == values.size() - 1 && extSize == 0,
                         nullptr, values[i].second == nullptr);
        }

        for (size_t i = 0; i < extSize; ++i) {
            const auto &v = en->second.extendValues[i];
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
                            alias, EnumData{.aliases = std::vector<std::string>{
                                                {name}}});
                    } else {
                        it->second.aliases.push_back(name);
                    }
                } else {
                    enums.emplace(name, EnumData{});
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
                HandleData d(name);

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
        else if (strcmp(cat, "define") == 0) {
            XMLDefineParser parser{type, *this};
            if (parser.name ==  "VK_HEADER_VERSION") {
                std::cout << ">>> version: " << parser.value << std::endl;
                headerVersion = parser.value;
            }
        }
    }

    if (headerVersion.empty()) {
        throw std::runtime_error("header version not found.");
    }

    for (auto &h : handleBuffer) {
        const auto &t = handles.find(h.first);
        const auto &p = handles.find(h.second);
        if (t != handles.end() && p != handles.end()) {
            t->second.parent = &(*p);
        }
    }

    for (auto &h : handles) {
        h.second.superclass =
            strStripVk(std::string(getHandleSuperclass(h.second)));
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

void Generator::generateClassDecl(const HandleData &data) {
    genOptional(data.name.original, [&] {
        output += "  class " + data.name + ";\n";
        if (data.uniqueVariant) {
            output += "  class Unique" + data.name + ";\n";
        }
    });
}

std::string Generator::generateClassString(const std::string &className,
                                           const GenOutputClass &from) {
    std::string output = "  class " + className;
    if (!from.inherits.empty()) {
        output += " : " + from.inherits;
    }
    output += " {\n";

    const auto addSection = [&](const std::string &visibility,
                                const std::string &s) {
        if (!s.empty()) {
            output += "  " + visibility + ":\n" + s;
        }
    };

    addSection("public", from.sPublic);
    addSection("private", from.sPrivate);
    addSection("protected", from.sProtected);
    output += "  };\n";
    return output;
}

void Generator::genTypes() {
    std::cout << "Generating types" << std::endl;

    output += "  class " + cfg.loaderClassName + ";\n";
    for (auto &e : handles) {
        generateClassDecl(e.second);
    }
    for (auto &e : structs) {
        generateStructDecl(e.first, e.second);
    }

    loader.name.convert("Vk" + cfg.loaderClassName, true);
    loader.calculate(*this, cfg.loaderClassName);
    for (auto &e : handles) {
        e.second.calculate(*this, cfg.loaderClassName);
    }

    output += generateLoader();
    for (auto &e : handles) {
        generateClass(e.first.data(), e.second);
    }

    for (auto &e : structs) {
        parseStruct(e.second.node->ToElement(), e.first,
                    e.second.type == StructData::VK_STRUCT);
    }

    for (auto it = genStructStack.begin(); it != genStructStack.end(); ++it) {
        std::string structType{}, structTypeValue{}; // placeholders
        Variables members =
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
    std::string protoArgs = m.createProtoArguments(true);
    std::string innerArgs = m.createPassArguments();
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

void Generator::evalCommand(CommandData &cmd) const {
    std::string name = cmd.name;
    std::string tag = strWithoutTag(name);
    cmd.nameCat = evalNameCategory(name);
}

Generator::MemberNameCategory
Generator::evalNameCategory(const std::string &name) {
    if (name.starts_with("get")) {
        return MemberNameCategory::GET;
    }
    if (name.starts_with("allocate")) {
        return MemberNameCategory::ALLOCATE;
    }
    if (name.starts_with("create")) {
        return MemberNameCategory::CREATE;
    }
    if (name.starts_with("enumerate")) {
        return MemberNameCategory::ENUMERATE;
    }
    if (name.starts_with("write")) {
        return MemberNameCategory::WRITE;
    }
    if (name.starts_with("destroy")) {
        return MemberNameCategory::DESTROY;
    }
    if (name.starts_with("free")) {
        return MemberNameCategory::FREE;
    }
    return MemberNameCategory::UNKNOWN;
}

template <typename T>
void Generator::createOverload(
    MemberContext &ctx, const std::string &name,
    std::vector<std::unique_ptr<MemberResolver>> &secondary) {

    const auto getType = [](const MemberContext &ctx) {
        const auto &vars = ctx.getFilteredProtoVars();
        if (vars.empty()) {
            return std::string();
        }
        return vars.begin()->get()->original.type();
    };

    MemberContext c{ctx};
    c.name = name;
    std::string type = getType(c);
    if (type.empty() || !isHandle(type)) {
        // std::cout << ">>> drop: " << c.name.original << std::endl;
        return;
    }

    // std::cout << "Checking destroy for " << type << std::endl;
    for (const auto &g : ctx.cls->generated) {
        if (g.name == c.name) {
            std::string gtype = getType(g);
            if (type == gtype) {
                // std::cout << "  >> conflict here: " << gtype << std::endl;
                return;
            }
        }
    }
    secondary.push_back(std::make_unique<T>(c));
}

void Generator::generateClassMemberCpp(MemberContext &ctx,
                                       GenOutputClass &out) {
    std::vector<std::unique_ptr<MemberResolver>> secondary;

    const auto getPrimaryResolver = [&]() {
        std::unique_ptr<MemberResolver> resolver;
        const auto &last = ctx.getLastVar(); // last argument
        bool lastUniqueVariant = false;
        if (isHandle(last->original.type())) {
            const auto &handle = findHandle(last->original.type());
            if (handle.uniqueVariant) {
                lastUniqueVariant = true;
            }
        }

        if (ctx.pfnReturn == PFNReturnCategory::OTHER) {
            resolver = std::make_unique<MemberResolver>(ctx);
            resolver->dbgtag = "PFN return";
            return resolver;
        }

        switch (ctx.nameCat) {
        case MemberNameCategory::GET:
        case MemberNameCategory::WRITE: {
            if (ctx.containsPointerVariable()) {
                resolver = std::make_unique<MemberResolverGet>(ctx);
                return resolver;
            }
            break;
        }
        case MemberNameCategory::CREATE:
            if (ctx.lastHandleVariant && !last->flagArray()) {
                secondary.push_back(
                    std::make_unique<MemberResolverCreateHandle>(ctx));
            }
            if (lastUniqueVariant && !ctx.getLastPointerVar()->flagArray()) {
                 secondary.push_back(std::make_unique<MemberResolverCreateUnique>(ctx));
            }
            resolver = std::make_unique<MemberResolverCreate>(ctx);
            return resolver;
        case MemberNameCategory::ALLOCATE:
            if (lastUniqueVariant && !ctx.getLastPointerVar()->flagArray()) {
                secondary.push_back(std::make_unique<MemberResolverCreateUnique>(ctx));
            }
            resolver = std::make_unique<MemberResolverAllocate>(ctx);
            return resolver;
        case MemberNameCategory::ENUMERATE:
            resolver = std::make_unique<MemberResolverEnumerate>(ctx);
            return resolver;
        case MemberNameCategory::DESTROY:
            createOverload<MemberResolver>(ctx, "destroy", secondary);
            break;
        case MemberNameCategory::FREE:
            createOverload<MemberResolver>(ctx, "free", secondary);
            break;
        default:
            break;
        }

        if (last->isPointer() && !last->isConst() && !last->flagArrayIn()) {
            resolver = std::make_unique<MemberResolverGet>(ctx);
            resolver->dbgtag = "get (default)";
            return resolver;
        }
        return std::make_unique<MemberResolver>(ctx);
    };

    const auto &resolver = getPrimaryResolver();
    resolver->generate(out.sPublic, outputFuncs);

    for (const auto &resolver : secondary) {
        resolver->generate(out.sPublic, outputFuncs);
    }

    //    std::cout << "C: " << ctx.cls->name << ": " <<
    //    ctx.cls->generated.size() << std::endl;
}

void Generator::generateClassMembers(const HandleData &data,
                                     GenOutputClass &out) {

    const auto &className = data.name;
    const auto &handle = data.vkhandle;
    const std::string &ldr = cfg.loaderClassName;

    const auto &superclass = findHandle("Vk" + data.superclass);
    VariableData super(superclass.name);
    super.setConst(true);
    VariableData parent(VariableData::TYPE_INVALID);
    if (data.parent && data.superclass != data.parent->second.name) {
        parent = VariableData(data.parent->second.name);
        parent.setConst(true);
    }

    // PFN function pointers
    for (const ClassCommandData &m : data.members) {
        out.sProtected +=
            genOptional(m.name.original, [&](std::string &output) {
                const std::string &name = m.name.original;
                output += format("    PFN_{0} m_{0} = {};\n", name);
            });
    }

    if (data.getAddrCmd.has_value()) {
        const std::string &getAddr = data.getAddrCmd->name.original;
        out.sProtected += "    PFN_" + getAddr + " m_" + getAddr + " = {};\n";

        // getProcAddr member
        out.sPublic += format(R"(
    template<typename T>
    inline T getProcAddr(const std::string_view &name) const {
      return std::bit_cast<T>(m_{0}({1}, name.data()));
    }
)",
                              getAddr, handle);
    }

    if (data.effectiveMembers > 0) {
        std::string declParams = super.toString();
        std::string defParams = declParams;

        if (super.type() == ldr) {
            super.setAssignment("libLoader");
            declParams = super.toStringWithAssignment();
        }

        out.sProtected += "  void loadPFNs(" + declParams + ");\n";

        outputFuncs +=
            "  void " + className + "::loadPFNs(" + defParams + ") {\n";

        if (data.getAddrCmd.has_value()) {
            const std::string &getAddr = data.getAddrCmd->name.original;
            outputFuncs += "    m_" + getAddr + " = " + super.identifier();
            if (super.type() == ldr) {
                outputFuncs += ".getInstanceProcAddr();\n";
            } else {
                outputFuncs +=
                    ".getProcAddr<PFN_" + getAddr + ">(\"" + getAddr + "\");\n";
            }
        }

        std::string loadSrc;
        if (data.getAddrCmd->name.empty()) {
            loadSrc = super.identifier() + ".";
        }
        // function pointers initialization
        for (const ClassCommandData &m : data.members) {
            outputFuncs +=
                genOptional(m.name.original, [&](std::string &output) {
                    const std::string &name = m.name.original;
                    output += format(
                        "    m_{0} = {1}getProcAddr<PFN_{0}>(\"{0}\");\n", name,
                        loadSrc);
                });
        }
        outputFuncs += "  }\n";
    }

    // wrapper functions
    for (const ClassCommandData &m : data.members) {
        MemberContext ctx{m};
        generateClassMemberCpp(ctx, out);
    }
}

void Generator::generateClassConstructors(const HandleData &data,
                                          GenOutputClass &out) {
    const std::string &superclass = data.superclass;

    out.sPublic += format(R"(
    {CONSTEXPR} {0}() = default;
    {CONSTEXPR} {0}(std::nullptr_t) {NOEXCEPT} {}
    )",
                          data.name);

    if (data.totalMembers() == 1) {
        out.sPublic +=
            format(R"(
    {EXPLICIT} {0}(Vk{0} {1}) {NOEXCEPT}  : {2}({1}) {}
)",
                   data.name, strFirstLower(data.name), data.vkhandle);
    }

    std::string loadCall;
    if (data.effectiveMembers > 0) {
    loadCall = "\n      loadPFNs(owner);\n    ";
    }

    if (data.parentMember) {
        out.sPublic += format(R"(
    {EXPLICIT} {0}(Vk{0} {1}, const {3} &owner) {NOEXCEPT} : {2}({1}), m_owner(&owner) {{4}}
)",
                              data.name, strFirstLower(data.name),
                              data.vkhandle, data.superclass, loadCall);
    } else {
        out.sPublic += format(R"(
    {EXPLICIT} {0}(Vk{0} {1}, const {3} &owner) {NOEXCEPT} : {2}({1}) {{4}}
)",
                              data.name, strFirstLower(data.name),
                              data.vkhandle, data.superclass, loadCall);
    }

    for (auto &m : data.ctorCmds) {
        const auto &parent = m.params.begin()->get()->type();

        MemberContext ctx{m};
        MemberResolverInit resolver{ctx};
        if (!resolver.checkMethod()) {
            // std::cout << ">> constructor skipped: " <<  parent << "." <<
            // resolver.getName() << std::endl;
        } else {
            resolver.generate(out.sPublic, outputFuncs);
        }

        if (parent != superclass) {
            const auto &handle = findHandle("Vk" + superclass);
            MemberContext ctx{m};
            std::shared_ptr<VariableData> var =
                std::make_shared<VariableData>(handle.name);
            var->setConst(true);

            ctx.params.insert(ctx.params.begin(), var);
            MemberResolverInit resolver{ctx};
            if (resolver.checkMethod()) {
                resolver.generate(out.sPublic, outputFuncs);
            } else {
                // std::cout << ">> alt constructor skipped: " << parent << "."
                // << resolver.getName() << std::endl;
            }
        }
    }
}

void Generator::generateUniqueClass(const HandleData &data) {
    GenOutputClass out;
    const std::string &base = data.name;
    const std::string &className = "Unique" + base;
    const std::string &handle = data.vkhandle;
    const std::string &superclass = data.superclass;

    out.inherits = "public " + base;    

    bool isSubclass = data.isSubclass();
    bool parent = !data.parentMember && isSubclass;

    if (parent) {
        out.sPrivate += "    const " + superclass + " *m_owner;\n";
    }

    std::string method = data.creationCat == HandleCreationCategory::CREATE ? "destroy" : "free";

    std::string destroyCall = "std::cout << \"dbg: destroy " + base + "\" << std::endl;\n      ";
    if (isSubclass) {
        destroyCall +=
            "m_owner->" + method + "(this->get(), /*allocationCallbacks=*/nullptr);";
    } else {
        destroyCall += base + "::" + method + "(/*allocationCallbacks=*/nullptr);";
    }

    out.sPublic += format(parent ? R"(
    {0}() : m_owner(), {1}() {}

    {EXPLICIT} {0}({1} const &value, const {2} *owner) : m_owner(owner), {1}(value) {}

    {0}({0} && other) {NOEXCEPT}
      : m_owner(std::move(other.m_owner)), {1}(other)
    {
      other.{3} = nullptr;
    }
)"
                                 : R"(
    {0}() : {1}() {}

    {EXPLICIT} {0}({1} const &value) : {1}(value) {}

    {0}({0} && other) {NOEXCEPT}
      : {1}(other)
    {
      other.{3} = nullptr;
    }
)",
                          className, base, superclass, handle);

    out.sPublic += format(R"(

    {0}({0} const &) = delete;

    ~{0}() {NOEXCEPT} {
        std::cout << "~{0}: " << {1} << std::endl;
      if ({1}) {
        this->destroy();
      }
    }

    void destroy();

    {0}& operator=({0} const&) = delete;
)",
                          className, handle);

    out.sPublic += format(parent ? R"(
    {0}& operator=({0} && other) {NOEXCEPT} {
      if ({1}) {
        this->destroy();
      }
      *static_cast<{2}*>(this) = other;
      m_owner = std::move(other.m_owner);
      other.{1} = nullptr;      
      return *this;
    }
)"
                                 : R"(
    {0}& operator=({0} && other) {NOEXCEPT} {
      if ({1}) {
        this->destroy();
      }
      *static_cast<{2}*>(this) = other;
      other.{1} = nullptr;
      return *this;
    }
)",
                          className, handle, base);

    out.sPublic +=
        format(R"(

    {1} const * operator->() const {NOEXCEPT} {
      return this;
    }

    {1} * operator->() {NOEXCEPT} {
      return this;
    }

    {1} const & operator*() const {NOEXCEPT} {
      return *this;
    }

    {1} & operator*() {NOEXCEPT} {
      return *this;
    }

    const {1}& get() const {NOEXCEPT} {
      return *this;
    }

    {1}& get() {NOEXCEPT} {
      return *this;
    }

    void reset({1} const &value = {1}());

    {1} release() {NOEXCEPT} {
      {1} value = *this;
      {2} = nullptr;
      return value;
    }

    void swap({0} &rhs) {NOEXCEPT} {
      std::swap(*this, rhs);
    }

)",
               className, base, handle);

    for (const auto &m : data.ctorCmds) {
        const auto &parent = m.params.begin()->get()->type();

        MemberContext ctx{m};
        MemberResolverInit resolver{ctx};
        if (resolver.checkMethod()) {
            //            out.sPublic += resolver.generateDeclaration();
            //            outputFuncs += resolver.generateDefinition();
            out.sPublic += "  // ctor (" + ctx.createProtoArguments() +
                           ") : " + base + "(" + ctx.createPassArguments() +
                           ")\n\n";
        }

        if (parent != superclass) {
            const auto &handle = findHandle("Vk" + superclass);
            MemberContext ctx{m};
            std::shared_ptr<VariableData> var =
                std::make_shared<VariableData>(handle.name);
            var->setConst(true);

            ctx.params.insert(ctx.params.begin(), var);
            MemberResolverInit resolver{ctx};
            if (resolver.checkMethod()) {
                out.sPublic += "    // ctor (" + ctx.createProtoArguments() +
                               ") : " + base + "(" + ctx.createPassArguments() +
                               ")\n\n";
                //                out.sPublic += resolver.generateDeclaration();
                //                outputFuncs += resolver.generateDefinition();
            }
        }
    }

    output += genOptional(data.name.original, [&](std::string &output) {
        output += generateClassString(className, out);
        output += format(R"(
  {INLINE} void swap({0} &lhs, {0} &rhs) {NOEXCEPT} {
    lhs.swap(rhs);
  }

)",
                         className);
    });

    outputFuncs += genOptional(data.name.original, [&](std::string &output) {
        output += format(R"(
  void {0}::destroy() {
    {3}
    {2} = nullptr;
  }

  void {0}::reset({1} const &value) {
    if ({2} != value ) {
      if ({2}) {
        {3}
      }
      *static_cast<{1}*>(this) = value;
    }
  }
)",
                         className, base, handle, destroyCall);
    });
}

std::string_view Generator::getHandleSuperclass(const HandleData &data) {
    if (!data.parent) {
        return cfg.loaderClassName;
    }
    if (data.name.original == "VkSwapchainKHR") {
        return "VkDevice";
    }
    auto *it = data.parent;
    while (it->second.parent) {
        if (it->first == "VkInstance" || it->first == "VkDevice") {
            break;
        }
        it = it->second.parent;
    }
    return it->first;
}

void Generator::generateClass(const std::string &name, HandleData data) {
    genOptional(name, [&] {
        GenOutputClass out;

        const std::string &className = data.name;
        const std::string &classNameLower = strFirstLower(className);
        const std::string &handle = data.vkhandle;
        const std::string &superclass = data.superclass;

        // std::cout << "Gen class: " << className << std::endl;
        // std::cout << "  ->superclass: " << superclass << std::endl;

        std::string debugReportValue = "Unknown";
        auto en = enums.find("VkDebugReportObjectTypeEXT");
        if (en != enums.end() && en->second.containsValue("e" + className)) {
            debugReportValue = className;
        }

        out.sPublic += format(R"(
    using CType      = Vk{0};
    using NativeType = Vk{0};

    static VULKAN_HPP_CONST_OR_CONSTEXPR {NAMESPACE}::ObjectType objectType =
      {NAMESPACE}::ObjectType::e{0};
    static VULKAN_HPP_CONST_OR_CONSTEXPR {NAMESPACE}::DebugReportObjectTypeEXT debugReportObjectType =
      {NAMESPACE}::DebugReportObjectTypeEXT::e{1};

)",
                              className, debugReportValue);

        generateClassConstructors(data, out);

        out.sProtected += "    " + name + " " + handle + " = {};\n";
        if (data.parentMember) {
            out.sProtected += "    " + superclass + "* m_owner = {};\n";

            out.sPublic += format(R"(
    {0}* getOwner() const {
      return m_owner;
    }
)",
                                  superclass);
        }

        out.sPublic += format(R"(
    operator Vk{0}() const {
      return {2};
    }

    explicit operator bool() const {NOEXCEPT} {
      return {2} != VK_NULL_HANDLE;
    }

    bool operator!() const {NOEXCEPT} {
      return {2} == VK_NULL_HANDLE;
    }

#if defined( VULKAN_HPP_TYPESAFE_CONVERSION )
    {0} & operator=( Vk{0} {1} ) VULKAN_HPP_NOEXCEPT
    {
      {2} = {1};
      return *this;
    }
#endif

    {0} & operator=( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
    {
      {2} = {};
      return *this;
    }

#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
    auto operator<=>( {0} const & ) const = default;
#else
    bool operator==( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return {2} == rhs.{2};
    }

    bool operator!=( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return {2} != rhs.{2};
    }

    bool operator<( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return {2} < rhs.{2};
    }
#endif
)",
                              className, classNameLower, handle);

        generateClassMembers(data, out);

        output += generateClassString(className, out);

        output += format(R"(
  template <>
  struct CppType<{NAMESPACE}::ObjectType, {NAMESPACE}::ObjectType::e{0}>
  {
    using Type = {NAMESPACE}::{0};
  };

)",
                         className, debugReportValue);

        if (debugReportValue != "Unknown") {
            output += format(R"(
  template <>
  struct CppType<{NAMESPACE}::DebugReportObjectTypeEXT,
                 {NAMESPACE}::DebugReportObjectTypeEXT::e{1}>
  {
    using Type = {NAMESPACE}::{0};
  };

)",
                             className, debugReportValue);
        }

        output += format(R"(
  template <>
  struct isVulkanHandleType<{NAMESPACE}::{0}>
  {
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = true;
  };

)",
                         className);

        if (data.uniqueVariant) {
            generateUniqueClass(data);
        }
    });
}

void Generator::parseCommands(XMLNode *node) {
    std::cout << "Parsing commands" << std::endl;

    std::vector<std::string_view> deviceObjects;
    std::vector<std::string_view> instanceObjects;

    std::vector<CommandData> elementsUnassigned;
    std::vector<CommandData> elementsStatic;
    std::vector<CommandData> commands;

    for (auto &h : handles) {
        if (h.first == "VkDevice" || h.second.superclass == "Device") {
            deviceObjects.push_back(h.first);
        } else if (h.first == "VkInstance" ||
                   h.second.superclass == "Instance") {
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

    const auto addCommand = [&](const std::string &className,
                                const CommandData &command)
    {
        auto &handle = findHandle("Vk" + className);
        handle.addCommand(*this, command);

        auto alias = aliased.find(command.name.original);
        if (alias != aliased.end()) {
            for (auto &a : alias->second) {
                CommandData data = command;
                data.setName(*this, std::string(a));
                handle.addCommand(*this, data);
            }
        }
    };

    const auto assignGetProc = [&](const std::string &className,
                                   const CommandData &command) {
        if (command.name.original == "vkGet" + className + "ProcAddr") {
            auto &handle = findHandle("Vk" + className);
            handle.getAddrCmd = ClassCommandData{*this, &handle, command};
            return true;
        }
        return false;
    };

    const auto assignConstruct = [&](const CommandData &command) {
        if (command.params.rbegin()->get()->flagArray()) {
            return false;
        }

        const auto commandExists = [&](const std::string &name) {
            for (const auto &c : commands) {
                if (c.name.original == name) {
                    return true;
                }
            }
            return false;
        };

        if (command.nameCat == MemberNameCategory::CREATE ||
            command.nameCat == MemberNameCategory::ALLOCATE) {

            std::string name = command.params.rbegin()->get()->original.type();
            try {
                auto &handle = findHandle(name);

                ClassCommandData data{*this, &handle, command};
                handle.ctorCmds.push_back(data);

                bool destroyExists = false;
                if (command.nameCat == MemberNameCategory::CREATE) {
                    destroyExists = commandExists("vkDestroy" + handle.name);
                    handle.creationCat = HandleCreationCategory::CREATE;
                } else {
                    destroyExists = commandExists("vkFree" + handle.name);
                    handle.creationCat = HandleCreationCategory::ALLOCATE;
                }
                handle.uniqueVariant = destroyExists;

                auto superclass = handle.superclass;

                const auto &first = command.params.begin();
                auto type = first->get()->type();
                if (type == "Instance" || type == "PhysicalDevice") {
                    superclass = "Instance";
                } else if (type == "Device") {
                    superclass = "Device";
                }

                // if (data.name.original == "vkCreateInstance") {
                // std::cout << ">>>> " << superclass << std::endl;
                //}

                auto &parent = findHandle("Vk" + superclass);
                parent.addCommand(*this, command);

                return true;
            } catch (std::runtime_error e) {
                std::cerr << "warning: can't assign constructor: " << name
                          << " (from " << command.name << "): " << e.what()
                          << std::endl;
                // std::cout << ">>> ldr " << loader.name.original << std::endl;
            }
        }
        return false;
    };

    const auto assignCommand = [&](const CommandData &command) {
        return assignConstruct(command) ||
               assignGetProc("Instance", command) ||
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

        CommandData command = parseClassMember(element, "");
        commands.push_back(command);
    }

    for (const auto &command : commands) {

        // std::cout << std::endl << "Parsed: " << command.name << std::endl;

        if (command.params.size() > 0) {
            if (assignCommand(command)) {
                continue;
            }

            // first argument of a command
            std::string_view first = command.params.at(0)->original.type();
            if (isInContainter(deviceObjects,
                               first)) { // command is for device
                addCommand("Device", command);
            } else if (isInContainter(instanceObjects,
                                      first)) { // command is for instance
                addCommand("Instance", command);
            } else {
                elementsStatic.push_back(command);
            }
        } else {
            elementsUnassigned.push_back(command);
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
    std::cout << "Static commands: " << elementsStatic.size() << std::endl;
    for (auto &c : elementsStatic) {
        std::cout << "  " << c.name << std::endl;
    }

    for (const auto &h : handles) {
        std::cout << h.second.name;
        if (h.second.parent) {
            std::cout << ", parent: " << h.second.parent->second.name;
        }
        std::cout << ", super: " << h.second.superclass << " {" << std::endl;
        for (const auto &c : h.second.ctorCmds) {
            std::cout << "  " << c.name << std::endl;
        }
        std::cout << "}" << std::endl;
    }
}

std::string Generator::generateLoader() {
    GenOutputClass out;
    for (const auto &m : loader.members) {

        MemberContext ctx{m};
        generateClassMemberCpp(ctx, out);
    }

    std::string output;
    output += R"(
#ifdef _WIN32
#  define LIBHANDLE HINSTANCE
#else
#  define LIBHANDLE void*
#endif
)";

    out.sProtected += R"(
    LIBHANDLE lib;
    uint32_t m_version;
    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr;
    PFN_vkCreateInstance m_vkCreateInstance;

    template<typename T>
    inline T getProcAddr(const char *name) const {
      return std::bit_cast<T>(m_vkGetInstanceProcAddr(nullptr, name));
    }
)";
    out.sPublic += format(R"(
#ifdef _WIN32
    static constexpr char const* defaultName = "vulkan-1.dll";
#else
    static constexpr char const* defaultName = "libvulkan.so.1";
#endif

    {0}() {
      m_vkGetInstanceProcAddr = nullptr;
    }

    {0}(const std::string &name) {
      load(name);
    }

    void load(const std::string &name = defaultName) {

#ifdef _WIN32
      lib = LoadLibraryA(name.c_str());
#else
      lib = dlopen(name.c_str(), RTLD_NOW);
#endif
      if (!lib) {
          throw std::runtime_error("Cant load library");
      }

#ifdef _WIN32
      m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
#else
      m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
#endif
      if (!m_vkGetInstanceProcAddr) {
          throw std::runtime_error("Cant load vkGetInstanceProcAddr");
      }

      PFN_vkEnumerateInstanceVersion m_vkEnumerateInstanceVersion = getProcAddr<PFN_vkEnumerateInstanceVersion>("vkEnumerateInstanceVersion");

      if (m_vkEnumerateInstanceVersion) {
          m_vkEnumerateInstanceVersion(&m_version);
      }
      else {
          m_version = VK_API_VERSION_1_0;
      }

      m_vkCreateInstance = getProcAddr<PFN_vkCreateInstance>("vkCreateInstance");
    }

    void unload() {
      if (lib) {
#ifdef _WIN32
        FreeLibrary(lib);
#else
        dlclose(lib);
#endif
        lib = nullptr;
        m_vkGetInstanceProcAddr = nullptr;
        m_vkCreateInstance = nullptr;
      }
    }

    uint32_t version() const {
      return m_version;
    }

    ~LibraryLoader() {
      unload();
    }

    PFN_vkGetInstanceProcAddr getInstanceProcAddr() const {
      // if (!m_vkGetInstanceProcAddr) {
      //  load();
      // }
      return m_vkGetInstanceProcAddr;
    }
)",
                          cfg.loaderClassName);

    output += generateClassString(cfg.loaderClassName, out);

    output += "  static " + cfg.loaderClassName + " libLoader;\n";

    return output;
}

std::string Generator::genMacro(const Macro &m) {
    std::string out;

    if (m.usesDefine) {
        out += format(R"(
#if !defined( {0} )
#  define {0} {1}
#endif
)",
                      m.define, m.value);
    }
    return out;
}

Generator::Generator() : loader(HandleData{""}) {
    unload();
    resetConfig();
}

void Generator::resetConfig() {
    cfg.genStructNoinit = true;
    cfg.genStructProxy = true;
    cfg.genObjectParents = true;

    cfg.dbgMethodTags = false;

    cfg.loaderClassName = "LibraryLoader";

    const auto setMacro = [](Macro &m, std::string define, std::string value,
                             bool uses) {
        m.define = define;
        m.value = value;
        m.usesDefine = uses;
    };

    setMacro(cfg.mcName, "VULKAN_HPP_NAMESPACE", "vk20", true);
    setMacro(cfg.mcConstexpr, "VULKAN_HPP_CONSTEXPR", "constexpr", true);
    setMacro(cfg.mcInline, "VULKAN_HPP_INLINE", "inline", true);
    setMacro(cfg.mcNoexcept, "VULKAN_HPP_NOEXCEPT", "noexcept", true);
    setMacro(cfg.mcExplicit, "VULKAN_HPP_TYPESAFE_EXPLICIT", "explicit", true);

    // temporary override
    cfg.dbgMethodTags = true;

    cfg.mcName.usesDefine = false;
    cfg.mcConstexpr.usesDefine = false;
    cfg.mcInline.usesDefine = false;
    cfg.mcNoexcept.usesDefine = false;
    cfg.mcExplicit.usesDefine = false;
}

void Generator::setOutputFilePath(const std::string &path) {
    outputFilePath = path;
    if (isOuputFilepathValid()) {
        std::string filename = std::filesystem::path(path).filename().string();
        filename = camelToSnake(filename);
        cfg.fileProtect = std::regex_replace(filename, std::regex("\\."), "_");
    } else {
        cfg.fileProtect = "";
    }
}

void Generator::load(const std::string &xmlPath) {
    unload();

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

    loaded = true;

    std::cout << "loaded: " << xmlPath << std::endl;
}

void Generator::unload() {
    root = nullptr;
    loaded = false;

    headerVersion.clear();
    platforms.clear();
    enums.clear();
    enumMembers.clear();
    tags.clear();
    defines.clear();
    flags.clear();
    handles.clear();
    structs.clear();
    extensions.clear();
    namemap.clear();
    loader = HandleData{"Vk" + cfg.loaderClassName};
}

void Generator::generate() {

    std::cout << "generating" << std::endl;
    output.clear();
    outputFuncs.clear();
    generatedStructs.clear();
    genStructStack.clear();
    for (auto &e : enums) {
        e.second.members.clear();
    }
    for (auto &h : handles) {
        if (!h.second.generated.empty()) {
            std::cerr << "Warning: handle has uncleaned generated members"
                      << std::endl;
        }
    }

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

bool Generator::isInNamespace(const std::string &str) const {
    if (enums.contains(str)) {
        return true;
    }
    if (flags.contains(str)) {
        return true;
    }
    if (structs.contains(str)) {
        return true;
    }
    if (handles.contains(str)) {
        return true;
    }
    return false;
}

template <class... Args>
std::string Generator::format(const std::string &format,
                              const Args... args) const {

    std::vector<std::string> list;
    static const std::unordered_map<std::string, const Macro &> macros = {
        {"NAMESPACE", cfg.mcName},
        {"CONSTEXPR", cfg.mcConstexpr},
        {"NOEXCEPT", cfg.mcNoexcept},
        {"INLINE", cfg.mcInline},
        {"EXPLICIT", cfg.mcExplicit}};

    static const std::regex rgx = [&] {
        std::string s = "\\{([0-9]+";
        for (const auto &k : macros) {
            s += "|" + k.first;
        }
        s += ")\\}";
        return std::regex(s);
    }();
    (
        [&](const auto &arg) {
            list.push_back((std::stringstream() << arg).str());
        }(args),
        ...);
    bool matched = false;
    std::string str = regex_replace(format, rgx, [&](auto match) {
        const std::string &str = match[1];
        const auto &m = macros.find(str);
        matched = true;
        if (m != macros.end()) {
            return m->second.get();
        }
        int index = std::stoi(str);
        if (index >= list.size()) {
            throw std::runtime_error{"format index out of range"};
        }
        return list[index];
    });
    if (!matched) {
        return format;
    }
    return str;
}

void Generator::HandleData::calculate(const Generator &gen,
                                      const std::string &loaderClassName) {
    effectiveMembers = 0;
    for (const auto &m : members) {
        gen.genOptional(m.name.original,
                        [&](std::string &output) { effectiveMembers++; });
    }
    const auto &cfg = gen.getConfig();
    parentMember = cfg.genObjectParents && effectiveMembers > 0 &&
                   name != "Instance" && name != "Device" &&
                   name != loaderClassName;
    if (!members.empty() || parentMember) {
        std::cout << name << ": " << effectiveMembers << " commands from "
                  << members.size() << ". parent member: " << parentMember
                  << std::endl;
    }
}

int Generator::HandleData::totalMembers() const {
    return 1 + effectiveMembers + (parentMember ? 1 : 0);
}

bool Generator::HandleData::usesHandleVariant() const {
    return parentMember || effectiveMembers > 0;
}

void Generator::HandleData::addCommand(const Generator &gen,
                                       const CommandData &cmd) {
    // std::cout << "Added to " << name << ", " << cmd.name << std::endl;
    members.push_back(ClassCommandData{gen, this, cmd});
}

bool Generator::EnumData::containsValue(const std::string &value) {
    for (const auto &m : members) {
        if (m.first == value) {
            return true;
        }
    }
    return false;
}

std::string Generator::MemberResolver::getDbgtag() {
    if (!ctx.gen.getConfig().dbgMethodTags) {
        return "";
    }
    std::string out = "// <" + dbgtag + ">";
    //    if (ctx.flagLastArray()) {
    //        out += ", last array";
    //        std::string desc;
    //        if (ctx.flagLastArrayIn()) {
    //            desc += "in";
    //        }
    //        if (ctx.flagLastArrayOut()) {
    //            if (!desc.empty()) {
    //                desc += "|";
    //            }
    //            desc += "out";
    //        }
    //        if (!desc.empty()) {
    //            out += "(" + desc + ")";
    //        }
    //    }
    //    if (ctx.flagLastHandle()) {
    //        out += ", last handle";
    //        if (ctx.lastHandleVariant) {
    //            out += "(V)";
    //        }
    //    }
    out += "\n";
    return out;
}
