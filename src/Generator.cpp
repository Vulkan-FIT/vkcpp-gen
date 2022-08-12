#include "Generator.hpp"

#include <fstream>

static std::string
regex_replace(const std::string &input, const std::regex &regex,
              std::function<std::string(std::smatch const &match)> format) {

    std::string output;
    std::sregex_iterator it(input.begin(), input.end(), regex);
    std::sregex_iterator end = std::sregex_iterator();
    for (; it != end; it++) {
        output += it->prefix();
        output += format(*it);
    }
    if (input.size() > it->position()) {
        output += input.substr(input.size() - it->position());
    }
    return output;
}

void Generator::bindVars(Variables &vars) const {
    const auto findVar = [&](const std::string &id) {
        for (auto &p : vars) {
            if (p->original.identifier() == id) {
                return p;
            }
        }
        std::cerr << "can't find param (" + id + ")" << std::endl;
        return std::make_shared<VariableData>(*this, VariableData::TYPE_INVALID);
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
                    parseEnumExtend(entry, nullptr);
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
        const char *supportedAttrib = extension->Attribute("supported");
        bool supported = true;
        if (supportedAttrib && std::string_view(supportedAttrib) == "disabled") {
            supported = false;
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
                std::cerr << "Warn: Unknown platform in extensions: "
                          << platformAttrib << std::endl;
            }
        }

        std::map<std::string, ExtensionData>::reference ext =
            *extensions
                 .emplace(name,
                          ExtensionData{.platform = platform,
                                        .enabled = supported,
                                        .whitelisted = supported &&
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
                } else if (value == "enum" && supported) {
                    parseEnumExtend(entry, protect);
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
//        if (ext.platform) {
//            if (!ext.platform->second.whitelisted) {
//                return false;
//            }
//            protect = ext.platform->second.protect;
//            return false;
//        }
//        return false;
    }
    return true;
}

void Generator::withProtect(std::string &output, const char *protect,
                            std::function<void(std::string &)> function) const {
//    if (protect) {
//        return;
//    }
    if (protect) {
        output += "#if defined(" + std::string(protect) + ")\n";
    }

    function(output);

    if (protect) {
        output += "#endif //" + std::string(protect) + "\n";
    }
}

[[nodiscard]]
std::string Generator::genOptional(const std::string &name,
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
                                        std::string value, bool isBitmask) const {

//    bool dbg = value == "VK_PRESENT_MODE_IMMEDIATE_KHR";
//    if (dbg) {
//        int dummy;
//    }

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
    if (isBitmask) {
        strStripSuffix(value, "Bit");
    }
    return "e" + value;
}

std::string Generator::genNamespaceMacro(const Macro &m) {
    std::string output = genMacro(m);
    if (m.usesDefine) {
        output += format("#define {0}_STRING  VULKAN_HPP_STRINGIFY({1})\n", m.define, m.value);
    } else {
        output += format("#define {0}_STRING  \"{1}\"\n", m.define ,m.value);
    }
    return output;
}

std::string Generator::generateHeader() {
    std::string output;
    output += format(RES_HEADER, headerVersion);
    output += genNamespaceMacro(cfg.macro.mNamespace);
    output += "\n";
    return output;
}

void Generator::generateFiles(std::filesystem::path path) {

    std::string prefix = "vulkan20";
    std::string ext = ".hpp";

    const auto gen = [&](std::string suffix, std::string protect, const std::string &content, Namespace ns = Namespace::VK) {
        std::string filename = prefix + suffix + ext;
        std::string output;
        output += "#ifndef " + protect + "\n";
        output += "#define " + protect + "\n";
        if (ns == Namespace::NONE) {
            output += content;
        }
        else {
            output += beginNamespace(ns);
            output += content;
            output += endNamespace(ns);
        }
        output += "#endif\n";

        std::ofstream file;

        auto p = path.replace_filename(filename);
        file.open(p, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Can't open file: " + outputFilePath);
        }

        file << output;

        file.flush();
        file.close();

        std::cout << "Generated: " << p << std::endl;
    };

    gen("_enums", "VULKAN20_ENUMS_HPP", generateEnums());
    gen("_handles", "VULKAN20_HANDLES_HPP", generateHandles());
    gen("_structs", "VULKAN20_STRUCTS_HPP", generateStructs());
    gen("_funcs", "VULKAN20_FUNCS_HPP", outputFuncs);
    gen("_raii", "VULKAN20_RAII_HPP", generateRAII(), Namespace::NONE);
    gen("_raii_funcs", "VULKAN20_RAII_FUNCS_HPP", outputFuncsRAII, Namespace::RAII);

    gen("", "VULKAN20_HPP", generateMainFile(), Namespace::NONE);
}

std::string Generator::generateMainFile() {

    std::string out;    
    out += generateHeader();

    out += beginNamespace(Namespace::VK);

    out += format(RES_ARRAY_PROXY);
    // PROXY TEMPORARIES
    // ARRAY WRAPPER
    out += format(RES_FLAGS);
    out += format(RES_OPTIONAL);
    // STRUCTURE CHAINS
    // UNIQUE HANDLE

    out += generateDispatch();
    out += format(RES_BASE_TYPES);
    out += endNamespace(Namespace::VK);

//    out += beginNamespace(Namespace::VK);
//    out += generateEnums();
//    out += endNamespace(Namespace::VK);

    out += "#include \"vulkan20_enums.hpp\"\n";

    out += generateErrorClasses();
    out += "\n";
    out += format(RES_RESULT_VALUE);
    out += endNamespace(Namespace::VK);

    out += "#include \"vulkan20_handles.hpp\"\n";
    out += "#include \"vulkan20_structs.hpp\"\n";
    out += "#include \"vulkan20_funcs.hpp\"\n";

//    out += beginNamespace(Namespace::VK);
//    out += this->output + "\n";
//    out += "\n";
//    out += outputFuncs; // class methods
//    out += endNamespace(Namespace::VK);

    // out += beginNamespace(Namespace::VK);
    // STRUCT EXTENDS
    // DYNAMIC LOADER
    // out += endNamespace(Namespace::VK);

//    out += beginNamespace(Namespace::RAII);
//    out += "  using namespace " + cfg.macro.mNamespace.get() + ";\n";
//    out += format(RES_RAII);
//    out += this->outputRAII + "\n";
//    out += this->outputFuncsRAII + "\n";
//    out += endNamespace(Namespace::RAII);

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

std::string Generator::generateStructCode(std::string name,
                                   const std::string &structType,
                                   const std::string &structTypeValue,
                                   const Variables &members) {

    std::string output = "  struct " + name + " {\n";

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
        if (!cfg.gen.structProxy) {
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
                cfg.macro.mNamespace.get() + "::StructureType::eApplicationInfo" :
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

    if (cfg.gen.structNoinit) {

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
    return output;
}

std::string Generator::generateUnionCode(std::string name,
                                  const Variables &members) {
    std::string output = "  union " + name + " {\n";

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

    output += "  };\n";
    return output;
}

std::string Generator::generateStruct(std::string name, const std::string &structType,
                               const std::string &structTypeValue,
                               const Variables &members,
                               const std::vector<std::string> &typeList,
                               bool structOrUnion) {
    auto it = structs.find(name);

    return genOptional(name, [&](std::string &output) {
        strStripVk(name);

        if (structOrUnion) {
            output += generateStructCode(name, structType, structTypeValue, members);
        } else {
            output += generateUnionCode(name, members);
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

std::string Generator::parseStruct(XMLElement *node, std::string name,
                            bool structOrUnion) {
    std::string output;
    if (const char *aliasAttrib = node->Attribute("alias")) {
        return "";
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
        return "";
    }

    output += generateStruct(name, structType, structTypeValue, members, typeList,
                   structOrUnion);

    for (auto it = genStructStack.begin(); it != genStructStack.end(); ++it) {
        if (hasAllDeps(it->typeList)) {
            std::string structType{}, structTypeValue{};
            Variables members =
                parseStructMembers(it->node, structType, structTypeValue);
            output += generateStruct(it->name, structType, structTypeValue, members,
                           it->typeList, it->structOrUnion);
            it = genStructStack.erase(it);
            --it;
        }
    }
    return output;
}

void Generator::parseEnumExtend(XMLElement &node, const char *protect) {
    const char *extends = node.Attribute("extends");
    // const char *bitpos = node.Attribute("bitpos");
    const char *value = node.Attribute("name");
    const char *alias = node.Attribute("alias");

    if (extends && value) {
        auto it = enums.find(extends);
        if (it != enums.end()) {
            std::string cpp = enumConvertCamel(it->second.name, value, it->second.isBitmask);
            if (!it->second.containsValue(cpp)) {
                String v{cpp};
                v.original = value;
                EnumValue data{v, alias? true : false};
                data.protect = protect;
                it->second.members.push_back(data);
            }

        } else {
            std::cerr << "err: Cant find enum: " << extends << std::endl;
        }
    }
}

std::string Generator::generateEnum(std::string name, XMLNode *node,
                             const std::string &bitmask) {
    std::string output;
    /*
    auto en = enums.find(name);
    if (en == enums.end()) {
        std::cerr << "cant find " << name << "in enums" << std::endl;
        return "";
    }

    auto alias = en->second.aliases;

    std::string ext = name;
    if (!bitmask.empty()) {
        ext = std::regex_replace(ext, std::regex("FlagBits"), "Flags");
    }

    output += genOptional(ext, [&](std::string &output) {
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

    */
    return output;
}

std::string Generator::generateEnum(const EnumData &data) {
    return genOptional(data.name.original, [&](std::string &output) {

        if (data.isFlag) {
            std::string inherit = std::regex_replace(data.name, std::regex("Flags"), "FlagBits");
            output += genFlagTraits(data, inherit);
            return;
        }

        // prototype
        output += "  enum class " + data.name;
        if (data.isBitmask) {            
            output += " : " + data.name.original;
        }

        output += " {\n";

        std::string str;

        for (const auto &m : data.members) {
            withProtect(output, m.protect, [&](std::string &output) {
                output += "    " + m.value + " = " + m.value.original + ",\n";
            });
            if (!m.isAlias) {
                withProtect(str, m.protect, [&](std::string &output) {
                    std::string value = m.value;
                    strStripPrefix(value, "e");
                    output += "      case " + data.name + "::" + m.value + ": return \"" + value + "\";\n";
                });
            }
        }
        strStripSuffix(output, ",\n");
        output += "\n  };\n";

        for (const auto &a : data.aliases) {
           output += "  using " + a + " = " + data.name + ";\n";
        }

        if (str.empty()) {
            output += format(R"(
  {INLINE} std::string to_string({0} value) {
    return {1};
  }
)",         data.name, "\"(void)\"");
        }
        else {
            str += "      default: return \"invalid ( \" + VULKAN_HPP_NAMESPACE::toHexString( static_cast<uint32_t>( value ) ) + \" )\";";
            output += format(R"(
  {INLINE} std::string to_string({0} value) {
    switch (value) {
{1}
    }
  }
)",         data.name, str);
        }
    });
}

std::string Generator::generateEnums() {
    std::string output;
    output += format(R"(
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

    for (const auto &e : enums) {
        //output += "  // " + e.first + ", " + e.second.name.original + "\n";
        output += generateEnum(e.second);
    }
    return output;
}

std::string Generator::genFlagTraits(const EnumData &data, std::string name) {
    std::string output;

    std::string flags = "";
    std::string str;

    for (size_t i = 0; i < data.members.size(); ++i) {
        const auto &m = data.members[i];
        if (m.isAlias) {
            continue;
        }
        withProtect(flags, m.protect, [&](std::string &output) {
            output += "        | VkFlags(" + data.name + "::" + m.value + ")\n";
        });
        withProtect(str, m.protect, [&](std::string &output) {
            std::string value = m.value;
            strStripPrefix(value, "e");
            output += format(R"(
    if (value & {0}::{1})
      result += "{2} | ";
)",         data.name, m.value, value);
        });
    }
    strStripPrefix(flags, "        | ");
    if (flags.empty()) {
        flags += "0\n";
    }

    output += format(R"(
  using {0} = Flags<{1}>;
)", data.name, name, flags, str);

    for (const auto &a : data.aliases) {
        output += "  using " + a + " = " + data.name + ";\n";
    }

    output += format(R"(

  template <>
  struct FlagTraits<{1}> {
    enum : VkFlags {
      allFlags = {2}
    };
  };

  {INLINE} {CONSTEXPR} {0} operator|({1} bit0, {1} bit1) {NOEXCEPT} {
    return {0}( bit0 ) | bit1;
  }

  {INLINE} {CONSTEXPR} {0} operator&({1} bit0, {1} bit1) {NOEXCEPT} {
    return {0}( bit0 ) & bit1;
  }

  {INLINE} {CONSTEXPR} {0} operator^({1} bit0, {1} bit1) {NOEXCEPT} {
    return {0}( bit0 ) ^ bit1;
  }

  {INLINE} {CONSTEXPR} {0} operator~({1} bits) {NOEXCEPT} {
    return ~( {0}( bits ) );
  }


)", data.name, name, flags);

    if (str.empty()) {
        output += format(R"(
  {INLINE} std::string to_string({0} value) {
    return "{}";
  }
)",     data.name, name, str);
    }
    else {
        output += format(R"(
  {INLINE} std::string to_string({0} value) {
    if ( !value )
      return "{}";

    std::string result;
    {2}

    return "{ " + result.substr( 0, result.size() - 3 ) + " }";
  }
)",     data.name, name, str);

    }
    return output;
}

/*
void Generator::genFlags() {

    std::cout << "Generating enum flags" << std::endl;

    for (auto &e : flags) {
        std::string l = toCppStyle(e.second.name, true);
        std::string r = toCppStyle(e.first, true);

        r = std::regex_replace(r, std::regex("Flags"), "FlagBits");

        output += genOptional(e.second.name, [&](std::string &output) {
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
*/
std::string Generator::generateDispatch() {
    std::string output;
    output += generateDispatchLoaderBase();
    output += generateDispatchLoaderStatic();
    output += format(R"(
  class DispatchLoaderDynamic;
#if !defined( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC )
#  if defined( VK_NO_PROTOTYPES )
#    define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#  else
#    define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 0
#  endif
#endif

#if !defined( VULKAN_HPP_STORAGE_API )
#  if defined( VULKAN_HPP_STORAGE_SHARED )
#    if defined( _MSC_VER )
#      if defined( VULKAN_HPP_STORAGE_SHARED_EXPORT )
#        define VULKAN_HPP_STORAGE_API __declspec( dllexport )
#      else
#        define VULKAN_HPP_STORAGE_API __declspec( dllimport )
#      endif
#    elif defined( __clang__ ) || defined( __GNUC__ )
#      if defined( VULKAN_HPP_STORAGE_SHARED_EXPORT )
#        define VULKAN_HPP_STORAGE_API __attribute__( ( visibility( "default" ) ) )
#      else
#        define VULKAN_HPP_STORAGE_API
#      endif
#    else
#      define VULKAN_HPP_STORAGE_API
#      pragma warning Unknown import / export semantics
#    endif
#  else
#    define VULKAN_HPP_STORAGE_API
#  endif
#endif

#if !defined( VULKAN_HPP_DEFAULT_DISPATCHER )
#  if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
#    define VULKAN_HPP_DEFAULT_DISPATCHER ::VULKAN_HPP_NAMESPACE::defaultDispatchLoaderDynamic
#    define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE                     \
      namespace VULKAN_HPP_NAMESPACE                                               \
      {                                                                            \
        VULKAN_HPP_STORAGE_API DispatchLoaderDynamic defaultDispatchLoaderDynamic; \
      }
  extern VULKAN_HPP_STORAGE_API DispatchLoaderDynamic defaultDispatchLoaderDynamic;
#  else
  static inline ::VULKAN_HPP_NAMESPACE::DispatchLoaderStatic & getDispatchLoaderStatic()
  {
    static ::VULKAN_HPP_NAMESPACE::DispatchLoaderStatic dls;
    return dls;
  }
#    define VULKAN_HPP_DEFAULT_DISPATCHER ::VULKAN_HPP_NAMESPACE::getDispatchLoaderStatic()
#    define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#  endif
#endif

#if !defined( VULKAN_HPP_DEFAULT_DISPATCHER_TYPE )
#  if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
#    define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::VULKAN_HPP_NAMESPACE::DispatchLoaderDynamic
#  else
#    define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::VULKAN_HPP_NAMESPACE::DispatchLoaderStatic
#  endif
#endif

#if defined( VULKAN_HPP_NO_DEFAULT_DISPATCHER )
#  define VULKAN_HPP_DEFAULT_ARGUMENT_ASSIGNMENT
#  define VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT
#  define VULKAN_HPP_DEFAULT_DISPATCHER_ASSIGNMENT
#else
#  define VULKAN_HPP_DEFAULT_ARGUMENT_ASSIGNMENT         = {}
#  define VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT = nullptr
#  define VULKAN_HPP_DEFAULT_DISPATCHER_ASSIGNMENT       = VULKAN_HPP_DEFAULT_DISPATCHER
#endif
)");
    return output;
}

std::string Generator::generateErrorClasses() {
    std::string output;
//    output += "#ifndef VULKAN_HPP_NO_EXCEPTIONS\n";
//    output += beginNamespace(Namespace::STD);
//    output += format(R"(
//  template <>
//  struct is_error_code_enum<VULKAN_HPP_NAMESPACE::Result> : public true_type
//  {
//  };
//)");
//    output += endNamespace(Namespace::STD);
//    output += "#endif\n";

    output += beginNamespace(Namespace::VK);
    output += format(RES_ERRORS);

    std::string str;

    for (const auto &e : errorClasses) {
        std::string name = e.first;
        strStripPrefix(name, "e");

        withProtect(output, e.second, [&](std::string &output){

            output += this->format(R"(
  class {0} : public SystemError
  {
  public:
    {0}( std::string const & message ) : SystemError( make_error_code( Result::{1} ), message ) {}
    {0}( char const * message ) : SystemError( make_error_code( Result::{1} ), message ) {}
  };
)",         name, e.first);
        });

        withProtect(str, e.second, [&](std::string &output){
            output += "        case Result::" + e.first + ": throw " + name + "(message);\n";
        });
    }

    output += format(R"(
  namespace {
    [[noreturn]] void throwResultException({NAMESPACE}::Result result, char const *message) {
      switch (result) {
{0}
        default: throw SystemError( make_error_code( result ) );
      }
    }
  }  // namespace
)", str);

    return output;
}

std::string Generator::generateDispatchLoaderBase() {
    std::string output;
    output += format(R"(
  class DispatchLoaderBase
  {
  public:
    DispatchLoaderBase() = default;
    DispatchLoaderBase( std::nullptr_t )
#if !defined( NDEBUG )
      : m_valid( false )
#endif
    {
    }

#if !defined( NDEBUG )
    size_t getVkHeaderVersion() const
    {
      VULKAN_HPP_ASSERT( m_valid );
      return vkHeaderVersion;
    }

  private:
    size_t vkHeaderVersion = VK_HEADER_VERSION;
    bool   m_valid         = true;
#endif
  };

)", "");
    dispatchLoaderBaseGenerated = true;
    return output;
}

std::string Generator::generateDispatchLoaderStatic() {
    std::string output;
    output += "//#if !defined( VK_NO_PROTOTYPES )\n";
    output += "  class DispatchLoaderStatic : public DispatchLoaderBase {\n";
    output += "  public:\n";

    HandleData empty{""};
    for (const CommandData &c : commands) {
        output += genOptional(c.name.original, [&](std::string &output){
            ClassCommandData d{*this, &empty, c};
            MemberContext ctx{d, Namespace::VK};

            std::string protoArgs = ctx.createProtoArguments(true);
            std::string args = ctx.createPassArguments();
            std::string name = ctx.name.original;
            std::string proto = ctx.type + " " + name + "(" + protoArgs + ")";
            std::string call;
            if (d.pfnReturn != PFNReturnCategory::VOID) {
                call += "return ";
            }
            call += "::" + name + "(" + args + ");";

            output += format(R"(
    {0} const {NOEXCEPT} {
      {1}
    }
)", proto, call);

        });
    }

    output += "  };\n";
    output += "//#endif\n";
    return output;
}

void Generator::parseTypes(XMLNode *node) {
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
                String n{name, true};
                const char *alias = type->Attribute("alias");
                if (alias) {
                    auto it = enums.find(alias);
                    if (it == enums.end()) {                        
                        std::cerr << "parse alias enum: can't find target" << std::endl;
                    } else {
                        it->second.aliases.push_back(n);
                    }
                } else {
                    enums.emplace(name, EnumData{name});
                }
            }
        } else if (strcmp(cat, "bitmask") == 0) {

            const char *name = type->Attribute("name");
            const char *aliasAttrib = type->Attribute("alias");
            // const char *reqAttrib = type->Attribute("requires");
            //const char *bitvalues = type->Attribute("bitvalues");
            if (!aliasAttrib) {
                XMLElement *nameElem = type->FirstChildElement("name");
                if (!nameElem) {
                    std::cerr << "Error: bitmap has no name" << std::endl;
                    continue;
                }
                name = nameElem->GetText();
            }
            if (!name) {
                std::cerr << "Error: bitmap alias has no name" << std::endl;
                continue;
            }


            EnumData data{name};
            if (aliasAttrib) {
                auto it = enums.find(aliasAttrib);
                if (it == enums.end()) {
                    std::cerr << "Error: parse alias enum: can't find target" << aliasAttrib << " (" << name << ")" << std::endl;
                } else {
                    it->second.aliases.push_back(data.name);
                }
            }
            else {
                data.isFlag = true;
                enums.emplace(name, data);

                std::string bits = std::regex_replace(name, std::regex("Flag"), "FlagBit");
                auto it = enums.find(bits);
                if (it == enums.end()) {
                    EnumData data{bits};
                    data.name.original = name;
                    data.isBitmask = true;
                    enums.emplace(bits, data);
                }

            }

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
        // initialize superclass information
        auto superclass = getHandleSuperclass(h.second);
        h.second.superclass = superclass;
        std::cout << "Handle: " << h.second.name << ", super: " << superclass << std::endl;
        if (h.second.isSubclass()) {
            h.second.ownerhandle = "m_" + strFirstLower(superclass);
        }
    }

    for (auto &h : handles) {
        if (h.first != h.second.name.original) {
            std::cerr << "Error: handle intergity check. " << h.first << " vs " << h.second.name.original << std::endl;
        }
    }

    std::cout << "Parsing declarations done" << std::endl;
}

void Generator::parseEnums(XMLNode *node) {

    const char *typeptr = node->ToElement()->Attribute("type");
    if (!typeptr) {
        return;
    }

    std::string_view type{typeptr};

    bool isBitmask = type == "bitmask";

    if (isBitmask || type == "enum") {

        const char *name = node->ToElement()->Attribute("name");
        if (!name) {
            std::cerr << "Can't get name of enum" << std::endl;
            return;
        }

        auto it = enums.find(name);
        if (it == enums.end()) {
            std::cerr << "cant find " << name << "in enums" << std::endl;
            return;
        }

        auto &en = it->second;

        bool dbg = false;
        if (en.name.starts_with("VideoBeginCoding"))
            dbg = true;

        en.isBitmask = isBitmask;
        if (isBitmask) {
            en.name.original = std::regex_replace(en.name.original, std::regex("FlagBit"), "Flag");
        }

        std::vector<std::string> aliased;

        for (XMLElement *e : Elements(node) | ValueFilter("enum")) {
            const char *value = e->Attribute("name");
            if (value) {
                const char *alias = e->Attribute("alias");
                if (alias) {
                    const char *comment = e->Attribute("comment");
                    if (!comment) {
                        aliased.push_back(value);
                    }
                    continue;
                }
                std::string cpp = enumConvertCamel(en.name, value, isBitmask);
                String v{cpp};
                v.original = value;

                en.members.push_back(EnumValue{v});
            }
        }

        for (const auto &a : aliased) {
            std::string cpp = enumConvertCamel(en.name, a, isBitmask);
            if (!en.containsValue(cpp)) {
                String v{cpp};
                v.original = a;
                en.members.push_back(EnumValue{v, true});
            }
        }

    }
}

std::string Generator::generateStructDecl(const std::string &name,
                                   const StructData &d) {
    return genOptional(name, [&](std::string &output) {
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

std::string Generator::generateClassDecl(const HandleData &data, bool allowUnique) {
    return genOptional(data.name.original, [&](std::string &output) {
        output += "  class " + data.name + ";\n";
        if (allowUnique && data.uniqueVariant()) {            
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

std::string Generator::generateHandles() {
    std::string output;
    for (auto &e : handles) {
        output += generateClassDecl(e.second, true);
    }
    for (auto &e : structs) {
        output += generateStructDecl(e.first, e.second);
    }

    HandleData empty{""};
    GenOutputClass out;
    for (auto &c : staticCommands) {
        ClassCommandData d{*this, &empty, c};
        MemberContext ctx{d, Namespace::VK};
        ctx.isStatic = true;
        generateClassMember(ctx, out, outputFuncs);
    }
    output += out.sPublic;

    for (auto &e : handles) {
        e.second.calculate(*this, loader.name);
    }

    for (auto &e : handles) {
        output += generateClass(e.first.data(), e.second, outputFuncs);
    }
    return output;
}

std::string Generator::generateStructs() {
    std::string output;
    for (auto &e : structs) {
        output += parseStruct(e.second.node->ToElement(), e.first,
                    e.second.type == StructData::VK_STRUCT);
    }

    for (auto it = genStructStack.begin(); it != genStructStack.end(); ++it) {
        std::string structType{}, structTypeValue{}; // placeholders
        Variables members =
            parseStructMembers(it->node, structType, structTypeValue);
        output += generateStruct(it->name, structType, structTypeValue, members,
                       it->typeList, it->structOrUnion);
    }
    return output;
}

std::string Generator::generateRAII() {
    std::string output;

    output += genNamespaceMacro(cfg.macro.mNamespaceRAII);
    output += beginNamespace(Namespace::RAII);
    output += "  using namespace " + cfg.macro.mNamespace.get() + ";\n";
    output += format(RES_RAII);

    //outputRAII += "  class " + loader.name + ";\n";
    for (auto &e : handles) {
        output += generateClassDecl(e.second, false);
    }
    output += generateLoader();

    for (auto &e : handles) {
        //if (e.second.totalMembers() > 1) {
        output += generateClassRAII(e.first.data(), e.second, outputFuncsRAII);
        //}
    }

    output += endNamespace(Namespace::RAII);
    output += "#include \"vulkan20_raii_funcs.hpp\"\n";
    return output;
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
    if (name.starts_with("acquire")) {
        return MemberNameCategory::ACQUIRE;
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
//    bool dbg = false;
//    if (ctx.name.original == "vkDestroySurfaceKHR") {
//        dbg = true;
//    }

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
    secondary.rbegin()->get()->dbgtag = "overload";
}

void Generator::generateClassMember(MemberContext &ctx,
                                       GenOutputClass &out, std::string &funcs) {
    if (ctx.ns == Namespace::VK && ctx.isRaiiOnly()) {
        return;
    }
    std::vector<std::unique_ptr<MemberResolver>> secondary;

    bool createPassOverload = true; // ctx.ns == Namespace::VK;
    const auto getPrimaryResolver = [&]() {
        std::unique_ptr<MemberResolver> resolver;
        const auto &last = ctx.getLastVar(); // last argument
        bool uniqueVariant = false;
        if (isHandle(last->original.type())) {
            const auto &handle = findHandle(last->original.type());
            uniqueVariant = handle.uniqueVariant();

            if (last->isArrayOut() && handle.vectorVariant && ctx.ns == Namespace::RAII) {

                MemberContext c{ctx};
                const auto &parent = c.params.begin()->get();
                if (parent->isHandle()) {
                    const auto &handle = findHandle(parent->original.type());
                    if (handle.isSubclass()) {
                        const auto &superclass = handle.superclass;
                        if (/*parent->original.type() != superclass.original && */
                            superclass.original != ctx.cls->superclass.original)
                        {
                            std::shared_ptr<VariableData> var =
                            std::make_shared<VariableData>(*this, superclass);
                            var->setConst(true);
                            c.params.insert(c.params.begin(), var);
                        }
                    }
                }

                for (const auto &p : c.params) {
                    if (p->isHandle()) {
                        p->toRAII();
                    }
                }

                resolver = std::make_unique<MemberResolverVectorRAII>(c);
                return resolver;
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
                auto &&var = ctx.getLastPointerVar();
                if (var->isHandle()) {
                    createPassOverload = true;
                }
                resolver = std::make_unique<MemberResolverGet>(ctx);
                return resolver;
            }
            break;
        }
        case MemberNameCategory::ALLOCATE:
        case MemberNameCategory::CREATE:
            createPassOverload = true;
            if (ctx.ns == Namespace::VK) {
                if (uniqueVariant && !last->isArray()) {
                    secondary.push_back(std::make_unique<MemberResolverCreateUnique>(ctx));
                }
            }
            resolver = std::make_unique<MemberResolverCreate>(ctx);
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

        if (last->isPointer() && !last->isConst() && !last->isArrayIn()) {
            resolver = std::make_unique<MemberResolverGet>(ctx);
            resolver->dbgtag = "get (default)";
            return resolver;
        }
        return std::make_unique<MemberResolver>(ctx);
    };

    const auto &resolver = getPrimaryResolver();

    if (createPassOverload) {
        MemberResolverPass passResolver{ctx};
        passResolver.dbgtag = "pass";        

        bool same = resolver->compareSignature(passResolver);
        if (same) {
            out.sPublic += "/*\n";
            funcs += "/*\n";
        }
        passResolver.generate(out.sPublic, funcs);
        if (same) {
            out.sPublic += "*/\n";
            funcs += "*/\n";
        }
    }

    resolver->generate(out.sPublic, funcs);

    for (const auto &resolver : secondary) {
        resolver->generate(out.sPublic, funcs);
    }
}

void Generator::generateClassMembers(HandleData &data,
                                     GenOutputClass &out, std::string &funcs, Namespace ns) {
    std::string output;
    if (ns == Namespace::RAII) {
        const auto &className = data.name;
        const auto &handle = data.vkhandle;
        const std::string &ldr = loader.name;

        const auto &superclass = findHandle("Vk" + data.superclass);
        VariableData super(*this, superclass.name);
        super.setConst(true);

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

        if (data.hasPFNs()) {

            // PFN function pointers
            for (const ClassCommandData &m : data.members) {
                out.sProtected +=
                    genOptional(m.name.original, [&](std::string &output) {
                        const std::string &name = m.name.original;
                        output += format("    PFN_{0} m_{0} = {};\n", name);
                    });
            }

            std::string declParams = super.toString();
            std::string defParams = declParams;

            if (super.type() == ldr) {
                super.setAssignment("libLoader");
                declParams = super.toStringWithAssignment();
            }

            out.sProtected += "  void loadPFNs(" + declParams + ");\n";

            output +=
                "  void " + className + "::loadPFNs(" + defParams + ") {\n";

            if (data.getAddrCmd.has_value()) {
                const std::string &getAddr = data.getAddrCmd->name.original;
                output += "    m_" + getAddr + " = " + super.identifier();
                if (super.type() == ldr) {
                    output += ".getInstanceProcAddr();\n";
                } else {
                    output +=
                        ".getProcAddr<PFN_" + getAddr + ">(\"" + getAddr + "\");\n";
                }
            }

            std::string loadSrc;
            if (data.getAddrCmd->name.empty()) {
                loadSrc = super.identifier() + ".";
            }
            // function pointers initialization
            for (const ClassCommandData &m : data.members) {
                output +=
                    genOptional(m.name.original, [&](std::string &output) {
                        const std::string &name = m.name.original;
                        output += format(
                            "    m_{0} = {1}getProcAddr<PFN_{0}>(\"{0}\");\n", name,
                            loadSrc);
                    });
            }
            output += "  }\n";
        }

        std::string call;
        if (!data.dtorCmds.empty()) {
            std::string n = data.dtorCmds[0]->name;
            ClassCommandData d(*this, &data, *data.dtorCmds[0]);
            MemberContext ctx{d, ns};
            call = "if (" + handle + ") {\n";
            auto parent = ctx.params.begin()->get();
            if (parent->isHandle() && parent->original.type() != data.name.original) {
                parent->setIgnorePFN(true);
            }
            auto alloc = ctx.params.rbegin()->get();
            if (alloc->original.type() == "VkAllocationCallbacks") {
                alloc->setAltPFN("nullptr");
            }
            if (data.ownerhandle.empty()) {
                call += "      m_" + data.dtorCmds[0]->name.original + "(" + ctx.createPFNArguments() + ");\n";
            }
            else {
                call += "      " + data.ownerhandle + "->" + data.dtorCmds[0]->name + "(" + ctx.createPassArguments() + ");\n";
            }
            call += "    }\n    ";
        }

        if (data.ownerhandle.empty()) {
            output += format(R"(
  void {0}::clear() {NOEXCEPT} {
    {2}{1} = nullptr;
  }

  void {0}::swap({NAMESPACE_RAII}::{0} &rhs) {NOEXCEPT} {
    std::swap({1}, rhs.{1});
  }
)",         className, handle, call);
        }
        else {
            output += format(R"(
  void {0}::clear() {NOEXCEPT} {
    {3}{2} = nullptr;
    {1} = nullptr;
  }

  void {0}::swap({NAMESPACE_RAII}::{0} &rhs) {NOEXCEPT} {
    std::swap({2}, rhs.{2});
    std::swap({1}, rhs.{1});
  }
)",         className, handle, data.ownerhandle, call);
        }
    }

    if (!output.empty()) {
        funcs += genOptional(data.name.original, [&](std::string &out){
            out += output;
        });
    }

    // wrapper functions
    for (const ClassCommandData &m : data.members) {
        MemberContext ctx{m, ns};
        generateClassMember(ctx, out, funcs);
    }
}

void Generator::generateClassConstructors(const HandleData &data,
                                          GenOutputClass &out, std::string &funcs) {
    const std::string &superclass = data.superclass;

    out.sPublic += format(R"(
    {CONSTEXPR} {0}() = default;
    {CONSTEXPR} {0}(std::nullptr_t) {NOEXCEPT} {}

    {EXPLICIT} {0}(Vk{0} {1}) {NOEXCEPT}  : {2}({1}) {}
)",
                   data.name, strFirstLower(data.name), data.vkhandle);
/*
    for (auto &m : data.ctorCmds) {
        const auto &parent = m.params.begin()->get()->type();

        MemberContext ctx{m};
        MemberResolverInit resolver{ctx};
        if (!resolver.checkMethod()) {
            // std::cout << ">> constructor skipped: " <<  parent << "." <<
            // resolver.getName() << std::endl;
        } else {
            resolver.generate(out.sPublic, funcs);
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
                resolver.generate(out.sPublic, funcs);
            } else {
                // std::cout << ">> alt constructor skipped: " << parent << "."
                // << resolver.getName() << std::endl;
            }
        }
    }
*/
}

void Generator::generateClassConstructorsRAII(const HandleData &data,
                                          GenOutputClass &out, std::string &funcs) {
    static constexpr Namespace ns = Namespace::RAII;

    const auto &superclass = data.superclass;    
    const std::string &owner = data.ownerhandle;

    const auto genCtor = [&](MemberContext &ctx, auto &parent) {
        MemberResolverCtor resolver{ctx};
        if (!resolver.checkMethod()) {
            std::cout << "ctor skipped: class " << data.name << ", p: " << parent->type() << ", s: " << superclass << std::endl;
        } else {
            resolver.generate(out.sPublic, funcs);
        }
    };

    for (auto &m : data.ctorCmds) {
        if (m.isAlias()) {
            continue;
        }
        const auto &parent = m.params.begin()->get();        

        if (parent->original.type() != superclass.original) {
            MemberContext ctx{m, ns, true};

            std::shared_ptr<VariableData> var =
                std::make_shared<VariableData>(*this, superclass);
            var->setConst(true);
            ctx.params.insert(ctx.params.begin(), var);
            std::cout << "ctor conflict: class " << data.name << ", p: " << parent->type() << ", s: " << superclass << std::endl;

            genCtor(ctx, parent);

            if (parent->isHandle()) {
                const auto &handle = findHandle(parent->original.type());
                if (handle.superclass.original != superclass.original) {
                    std::cerr << "ctor: impossible combination" << std::endl;
                    continue;
                }
            }
        }      

        MemberContext ctx{m, ns, true};
        genCtor(ctx, parent);

    }

    const auto createInit = [&](std::string indent) {
        std::string out = "\n";
        out += indent + ": ";
        out += format("{0}({NAMESPACE_RAII}::exchange(rhs.{0}, {}))\n", data.vkhandle);
        if (!data.ownerhandle.empty()) {
            out += indent + ", ";
            out += format("{0}({NAMESPACE_RAII}::exchange(rhs.{0}, {}))\n", data.ownerhandle);
        }
        return out;
    };


     const auto createAssign = [&](std::string indent) {
        std::string out = "\n";
        out += indent;
        out += format("{0} = {NAMESPACE_RAII}::exchange(rhs.{0}, {});\n", data.vkhandle);
        if (!data.ownerhandle.empty()) {
            out += indent;
            out += format("{0} = {NAMESPACE_RAII}::exchange(rhs.{0}, {});\n", data.ownerhandle);
        }
        return out;
    };

    std::string init = createInit("        ");
    std::string assign = createAssign("          ");

    out.sPublic += format(R"(
    {0}(std::nullptr_t) {NOEXCEPT} {}
    ~{0}() {
        //clear(); TODO
    }

    {0}() = delete;
    {0}({0} const&) = delete;
    {0}({0}&& rhs) {NOEXCEPT}{1}
    {
    }
    {0}& operator=({0} const &) = delete;
    {0}& operator=({0}&& rhs) {
        if ( this != &rhs ) {
          // clear(); TODO{2}
        }
        return *this;
    }
    )",
                          data.name, init, assign);

    std::string loadCall;
    if (data.hasPFNs()) {
        loadCall = "\n      //loadPFNs(" + owner + ");\n    ";
    }

    if (!owner.empty()) {
        out.sPublic += format(R"(
    {EXPLICIT} {0}(Vk{0} {1}, const {2} &{3}) {NOEXCEPT} : {4}({1}), {5}(&{3}) {{6}}
)",
                              data.name, strFirstLower(data.name),
                              superclass, strFirstLower(superclass),
                              data.vkhandle, owner, loadCall);
    } else {
        out.sPublic += format(R"(
    {EXPLICIT} {0}(Vk{0} {1}, const {2} &{3}) {NOEXCEPT} : {4}({1}) {{5}}
)",
                              data.name, strFirstLower(data.name),
                              superclass, strFirstLower(superclass),
                              data.vkhandle, loadCall);
    }
}

std::string Generator::generateUniqueClass(const HandleData &data, std::string &funcs) {
    std::string output;
    GenOutputClass out;
    const std::string &base = data.name;
    const std::string &className = "Unique" + base;
    const std::string &handle = data.vkhandle;
    const std::string &superclass = data.superclass;

    out.inherits = "public " + base;

    bool isSubclass = data.isSubclass();    
    if (isSubclass) {
        out.sPrivate += "    const " + superclass + " *m_owner;\n";
    }
    out.sPrivate += "    const VULKAN_HPP_DEFAULT_DISPATCHER_TYPE *m_dispatch;\n";


    std::string method = data.creationCat == HandleCreationCategory::CREATE ? "destroy" : "free";

    std::string destroyCall;// = "std::cout << \"dbg: destroy " + base + "\" << std::endl;\n      ";
    if (isSubclass) {
        destroyCall +=

            "m_owner->" + method + "(this->get(), /*allocationCallbacks=*/nullptr);";
    } else {
        destroyCall += base + "::" + method + "(/*allocationCallbacks=*/nullptr);";
    }

    destroyCall = "//" + destroyCall;

    out.sPublic += format(isSubclass ? R"(
    {0}() : m_owner(), {1}() {}

    {EXPLICIT} {0}({1} const &value, const {2} &owner);

    {0}({0} && other) {NOEXCEPT};

    const {2}* getOwner() const {
      return m_owner;
    }
)"
                                 : R"(
    {0}() : {1}() {}

    {EXPLICIT} {0}({1} const &value) : {1}(value) {}

    {0}({0} && other) {NOEXCEPT};
)",
                          className, base, superclass, handle); 

    out.sPublic += format(R"(
    {0}({0} const &) = delete;

    ~{0}() {NOEXCEPT};

    void destroy();

    {0}& operator=({0} const&) = delete;
)",
                          className, handle);

    out.sPublic += format(isSubclass ? R"(
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

    output += genOptional(data.name.original, [&](std::string &output) {

        output += generateClassString(className, out);

        output += format(R"(  
  {INLINE} void swap({0} &lhs, {0} &rhs) {NOEXCEPT} {
    lhs.swap(rhs);
  }

)",
                         className);
    });

    funcs += genOptional(data.name.original, [&](std::string &output) {

        output += format(isSubclass ? R"(

  {0}::{0}({1} const &value, const {2} &owner) : m_owner(&owner), {1}(value) {
#ifdef VK20DBG
    std::cout << "{0}: " << value;// << ", \t owner: " << m_owner;
    std::cout << std::endl;
#endif
  }

  {0}::{0}({0} && other) {NOEXCEPT}
    : m_owner(std::move(other.m_owner)), {1}(other)
  {
    other.{3} = nullptr;
  }

  {0}::~{0}() {NOEXCEPT} {
#ifdef VK20DBG
      if (!{3})
        std::cout << "~{0}: null" << std::endl;
#endif
      if ({3}) {
        this->destroy();
      }
    }
)"
                                 : R"(

    {0}::{0}({0} && other) {NOEXCEPT}
      : {1}(other)
    {
      other.{3} = nullptr;
    }

    {0}::~{0}() {NOEXCEPT} {
#ifdef VK20DBG
      if (!{3})
        std::cout << "~{0}: null" << std::endl;
#endif
      if ({3}) {
        this->destroy();
      }
    }
)",
                          className, base, superclass, handle);

        output += format(R"(
  void {0}::destroy() {
#ifdef VK20DBG
    std::cout << "~{0}: " << {2} << std::endl;
#endif
    {3}
    {2} = nullptr;
  }
)",
                         className, base, handle, destroyCall);

        output += format(R"(
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


    return output;
}

String Generator::getHandleSuperclass(const HandleData &data) {
    if (!data.parent) {
        return loader.name;
    }
    if (data.name.original == "VkSwapchainKHR") {
        return findHandle("VkDevice").name;
    }
    auto *it = data.parent;
    while (it->second.parent) {
        if (it->first == "VkInstance" || it->first == "VkDevice") {
            break;
        }
        it = it->second.parent;
    }
    return it->second.name;
}

std::string Generator::generateClass(const std::string &name, HandleData data, std::string &funcs) {
    std::string output;
    output += genOptional(name, [&](std::string &output) {
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

        generateClassConstructors(data, out, funcs);

        out.sProtected += "    " + name + " " + handle + " = {};\n";

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

        generateClassMembers(data, out, funcs, Namespace::VK);

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

        if (data.uniqueVariant()) {
            output += generateUniqueClass(data, funcs);
        }
    });
    return output;
}

std::string Generator::generateClassRAII(const std::string &name, HandleData data, std::string &funcs) {
    std::string output;
    output += genOptional(name, [&](std::string &output) {
        GenOutputClass out;

        const std::string &className = data.name;
        const std::string &classNameLower = strFirstLower(className);
        const std::string &handle = data.vkhandle;
        const auto &superclass = data.superclass;
        const std::string &owner = data.ownerhandle;

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

        generateClassConstructorsRAII(data, out, funcs);


        if (!owner.empty()) {
            out.sPrivate += format("    {0} const * {1} = {};\n", superclass, owner);

            out.sPublic += format(R"(
    {0} const * get{0}() const {
      return {1};
    }
)",
                                  superclass, owner);
        }
        out.sPrivate += format("    {NAMESPACE}::{0} {1} = {};\n", className, handle);

        out.sPublic += format(R"(    

    {NAMESPACE}::{0} const &operator*() const {NOEXCEPT} {
        return {1};
    }

    void clear() {NOEXCEPT};
    void swap({NAMESPACE_RAII}::{0} &) {NOEXCEPT};
)",
                              className, handle);

        generateClassMembers(data, out, funcs, Namespace::RAII);

        output += generateClassString(className, out);

        if (!data.vectorCmds.empty()) {

            GenOutputClass out;
            std::string name = className + "s";

            out.inherits += format("public std::vector<{NAMESPACE_RAII}::{0}>", className);

            HandleData cls = data;
            cls.name = name;

            for (auto &m : data.vectorCmds) {

                MemberContext ctx{m, Namespace::RAII, true};
                ctx.cls = &cls;
                const auto &parent = m.params.begin()->get();

                if (parent->original.type() != superclass.original) {

                    std::shared_ptr<VariableData> var =
                        std::make_shared<VariableData>(*this, superclass);
                    var->setConst(true);
                    ctx.params.insert(ctx.params.begin(), var);
                }

                MemberResolverVectorCtor resolver{ctx};
                if (!resolver.checkMethod()) {
                    std::cout << "vector ctor skipped: class " << data.name
                    //<< ", p: " << parent->type() << ", s: " << superclass
                    << std::endl;
                } else {
                    resolver.generate(out.sPublic, funcs);
                }

            }

            out.sPublic += format(R"(
    {0}( std::nullptr_t ) {}

    {0}()                          = delete;
    {0}( {0} const & ) = delete;
    {0}( {0} && rhs )  = default;
    {0} & operator=( {0} const & ) = delete;
    {0} & operator=( {0} && rhs ) = default;
)", name);

            output += generateClassString(name, out);

        }

    });
    return output;
}

void Generator::parseCommands(XMLNode *node) {
    std::cout << "Parsing commands" << std::endl;

//    struct Level {
//        std::string
//    };

    std::vector<std::string_view> deviceObjects;
    std::vector<std::string_view> instanceObjects;

    std::vector<CommandData> elementsUnassigned;    

    for (auto &h : handles) {
        if (h.first == "VkDevice" || h.second.superclass == "Device") {
            deviceObjects.push_back(h.first);
        } else if (h.first == "VkInstance" ||
                   h.second.superclass == "Instance") {
            instanceObjects.push_back(h.first);
        }
    }

    std::map<std::string_view, std::vector<std::string_view>> aliased;

    const auto addCommand = [&](const std::string_view &type,
                                const std::string_view &level,
                                const CommandData &command)
    {
        auto &handle = findHandle(level);
        handle.addCommand(*this, command);
        //std::cout << "=> added direct to: " << handle.name << ", " << command.name << std::endl;
        if (command.isIndirectCandidate(type) && type != level) {
            auto &handle = findHandle(type);
            CommandData data{command};
            data.setFlagBit(CommandFlags::INDIRECT, true);
            handle.addCommand(*this, data);
            // std::cout << "=> added indirect to: " << handle.name << ", " << command.name << std::endl;
        }
        if (command.params.size() >= 2) {
            std::string_view second = command.params.at(1)->original.type();
            if (command.isIndirectCandidate(second) && isHandle(second)) {
                auto &handle = findHandle(second);
                if (handle.superclass.original == type) {
                    CommandData data{command};
                    data.setFlagBit(CommandFlags::INDIRECT, true);
                    data.setFlagBit(CommandFlags::RAII_ONLY, true);
                    handle.addCommand(*this, data);
                    //std::cout << "=> added raii indirect to: " << handle.name << ", s: " << handle.superclass << ", " << command.name << std::endl;
                }
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

        const auto findCommand = [&](const std::string &name) -> CommandData* {
            for (auto &c : commands) {
                if (c.name.original == name) {
                    return &c;
                }
            }
            return nullptr;
        };

        if (command.isAlias()) {
            return;
        }

        switch (command.nameCat) {
            case MemberNameCategory::GET:
            case MemberNameCategory::ENUMERATE:
            case MemberNameCategory::CREATE:
            case MemberNameCategory::ALLOCATE:
                break;
            default:
                return;
        }

        const auto &last = command.params.rbegin()->get();
        std::string type = last->original.type();
        if (!last->isPointer() || !isHandle(type)) {
            return;
        }
        try {
            auto &handle = findHandle(type);
            auto superclass = handle.superclass;
            std::string name = handle.name;
            strStripPrefix(name, superclass);

            ClassCommandData data {*this, &handle, command};
            if (command.params.rbegin()->get()->isArray()) {
//                std::cout << ">> VECTOR " << handle.name << ": " << command.name << std::endl;
                handle.vectorVariant = true;
                handle.vectorCmds.push_back(data);
            }
            else {
                handle.ctorCmds.push_back(data);
//                std::cout << ">> " << handle.name << ": " << command.name << std::endl;
            }

            bool destroyExists = false;
            if (command.nameCat == MemberNameCategory::CREATE) {
                const auto c = findCommand("vkDestroy" + name);
                if (c) {
                    destroyExists = true;
                    handle.creationCat = HandleCreationCategory::CREATE;
                    handle.dtorCmds.push_back(c);
                }
            } else if (command.nameCat == MemberNameCategory::ALLOCATE) {                
                const auto c = findCommand("vkFree" + name);
                if (c) {
                    destroyExists = true;
                    handle.creationCat = HandleCreationCategory::ALLOCATE;
                    handle.dtorCmds.push_back(c);
                }
            }            
        } catch (std::runtime_error e) {
            std::cerr << "warning: can't assign constructor: " << type
                      << " (from " << command.name << "): " << e.what()
                      << std::endl;
        }
    };

    const auto assignCommand = [&](const CommandData &command) {
        return assignGetProc("Instance", command) ||
               assignGetProc("Device", command);
    };

    int c = 0;
    std::vector<XMLElement*> unaliased;
    for (XMLElement *commandElement : Elements(node) | ValueFilter("command")) {
        const char *alias = commandElement->Attribute("alias");

        c++;

        if (alias) {
            const char *name = commandElement->Attribute("name");
            if (!name) {
                std::cerr << "Error: Command has no name" << std::endl;
                continue;
            }
            auto it = aliased.find(alias);
            if (it == aliased.end()) {
                aliased.emplace(alias, std::vector<std::string_view>{{name}});
            } else {
                it->second.push_back(name);
            }
        }
        else {
            unaliased.push_back(commandElement);
        }
    }

    for (XMLElement *element : unaliased) {

        CommandData command = parseClassMember(element, "");
        commands.push_back(command);

        auto alias = aliased.find(command.name.original);
        if (alias != aliased.end()) {
            for (auto &a : alias->second) {
                CommandData data = command;
                data.setFlagBit(CommandFlags::ALIAS, true);
                data.setName(*this, std::string(a));
                commands.push_back(data);
            }
        }
    }


    for (const auto &command : commands) {

        if (assignCommand(command)) {
            continue;
        }

        assignConstruct(command);

        std::string_view first;
        if (!command.params.empty()) {
            // type of first argument
            first = command.params.at(0)->original.type();
        }

        if (!isHandle(first)) {
            staticCommands.push_back(command);
            loader.addCommand(*this, command);
            continue;
        }

        if (isInContainter(deviceObjects,
                           first)) { // command is for device
            addCommand(first, "VkDevice", command);
        } else if (isInContainter(instanceObjects,
                                  first)) { // command is for instance
            addCommand(first, "VkInstance", command);
        } else {
            std::cerr << "warning: can't assign command: " << command.name << std::endl;
        }

    }

    std::cout << "Parsing commands done" << std::endl;

    for (auto &h : handles) {
        if (h.first != h.second.name.original) {
            std::cerr << "Error: handle intergity check. " << h.first << " vs " << h.second.name.original << std::endl;
            abort();
        }
    }

    if (!elementsUnassigned.empty()) {
        std::cerr << "Unassigned commands: " << elementsUnassigned.size()
                  << std::endl;
        for (auto &c : elementsUnassigned) {
            std::cerr << "  " << c.name << std::endl;
        }
    }
    std::cout << "Static commands: " << staticCommands.size() << " { " << std::endl;
    for (auto &c : staticCommands) {
        std::cout << "  " << c.name << std::endl;
    }
    std::cout << "}" << std::endl;

    bool verbose = false;
    for (const auto &h : handles) {
        std::cout << "obj: " << h.second.name << " (" << h.first << ")";
        if (h.second.uniqueVariant()) {
            std::cout << " unique ";
        }
        if (h.second.vectorVariant) {
            std::cout << " vector ";
        }
        std::cout << std::endl;
        std::cout << "  super: " << h.second.superclass;
        if (h.second.parent) {
            std::cout << ", parent: " << h.second.parent->second.name;
        }
        std::cout << std::endl;
        for (const auto &c : h.second.dtorCmds) {
            std::cout << "  DTOR: " << c->name << " (" << c->name.original << ")" << std::endl;
        }
        if (!h.second.members.empty()) {
            std::cout << "  commands: " << h.second.members.size();
            if (verbose) {               
                std::cout << " {" << std::endl;
                for (const auto &c : h.second.members) {
                    std::cout << "  " << c.name.original << " ";
                    if (c.isRaiiOnly()) {
                        std::cout << "R";
                    }
                    if (c.isIndirect()) {
                        std::cout << "I";
                    }
                    std::cout << std::endl;
                }
                std::cout << "}";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

std::string Generator::generatePFNs(const HandleData &data, GenOutputClass &out) {
    std::string load;
    std::string loadSrc;
    if (data.getAddrCmd->name.empty()) {
        loadSrc = strFirstLower(data.superclass) + ".";
    }

    for (const ClassCommandData &m : data.members) {
        const std::string &name = m.name.original;

        // PFN pointers declaration
        out.sProtected +=
            genOptional(name, [&](std::string &output) {
                output += format("    PFN_{0} m_{0} = {};\n", name);
            });

        // PFN pointers initialization
        load += genOptional(name, [&](std::string &output) {
                output += format(
                            "      m_{0} = {1}getProcAddr<PFN_{0}>(\"{0}\");\n", name,
                            loadSrc);
            });
    }

    return load;
}

std::string Generator::generateLoader() {
    GenOutputClass out;
    std::string output;

    out.sProtected += R"(
    LIBHANDLE lib = {};
    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr = {};
)";
    std::string load = generatePFNs(loader, out);

    out.sPublic += format(R"(
#ifdef _WIN32
    static constexpr char const* defaultName = "vulkan-1.dll";
#else
    static constexpr char const* defaultName = "libvulkan.so.1";
#endif

    {0}() = default;

    {0}(const std::string &name) {
      load(name);
    }

    template<typename T>
    {INLINE} T getProcAddr(const char *name) const {
      return std::bit_cast<T>(m_vkGetInstanceProcAddr(nullptr, name));
    }

    void load(const std::string &name = defaultName) {

#ifdef _WIN32
      lib = LoadLibraryA(name.c_str());
#else
      lib = dlopen(name.c_str(), RTLD_NOW);
#endif
      if (!lib) {
        throw std::runtime_error("Cant load library: " + name);
      }

#ifdef _WIN32
      m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
#else
      m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
#endif
      if (!m_vkGetInstanceProcAddr) {
        throw std::runtime_error("Cant load vkGetInstanceProcAddr");
      }
{1}
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
      }
    }

    ~LibraryLoader() {
      unload();
    }

    PFN_vkGetInstanceProcAddr getInstanceProcAddr() const {
      return m_vkGetInstanceProcAddr;
    }
)",
                          loader.name, load);

    for (const auto &m : loader.members) {
        MemberContext ctx{m, Namespace::RAII};
        if (ctx.nameCat == MemberNameCategory::CREATE) {
            std::shared_ptr<VariableData> var =
            std::make_shared<VariableData>(*this, loader.name);
            var->setConst(true);
            var->setIgnorePFN(true);
            ctx.params.insert(ctx.params.begin(), var);
        }
        generateClassMember(ctx, out, outputFuncsRAII);
    }

   output += R"(
#ifdef _WIN32
#  define LIBHANDLE HINSTANCE
#else
#  define LIBHANDLE void*
#endif
)";
    output += generateClassString(loader.name, out);
    output += "  static " + loader.name + " libLoader;\n";

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

void Generator::initLoaderName() {
    loader.name.convert("Vk" + cfg.loaderClassName, true);
}

std::string Generator::beginNamespace(Namespace ns) const {
    return "namespace " + getNamespace(ns, false) + " {\n";
}

std::string Generator::endNamespace(Namespace ns) const {
    return "}  // namespace " + getNamespace(ns, false) + "\n";
}

Generator::Generator() : loader(HandleData{""}) {    
    unload();
    resetConfig();

    namespaces.insert_or_assign(Namespace::VK, &cfg.macro.mNamespace);
    namespaces.insert_or_assign(Namespace::RAII, &cfg.macro.mNamespaceRAII);
    namespaces.insert_or_assign(Namespace::STD, &cfg.macro.mNamespaceSTD);
}

void Generator::resetConfig() {
    cfg.gen.structNoinit = false;
    cfg.gen.structProxy = false;
    cfg.gen.objectParents = true;
    cfg.gen.dispatchLoaderStatic = true;
    cfg.gen.useStaticCommands = false;

    cfg.dbg.methodTags = false;

    cfg.loaderClassName = "LibraryLoader";

    const auto setMacro = [](Macro &m, std::string define, std::string value,
                             bool uses) {
        m.define = define;
        m.value = value;
        m.usesDefine = uses;
    };

    setMacro(cfg.macro.mNamespace, "VULKAN_HPP_NAMESPACE", "vk20", true);
    setMacro(cfg.macro.mNamespaceRAII, "VULKAN_HPP_NAMESPACE_RAII", "vk20r", true);
    setMacro(cfg.macro.mConstexpr, "VULKAN_HPP_CONSTEXPR", "constexpr", true);
    setMacro(cfg.macro.mInline, "VULKAN_HPP_INLINE", "inline", true);
    setMacro(cfg.macro.mNoexcept, "VULKAN_HPP_NOEXCEPT", "noexcept", true);
    setMacro(cfg.macro.mExplicit, "VULKAN_HPP_TYPESAFE_EXPLICIT", "explicit", true);
    setMacro(cfg.macro.mDispatch, "VULKAN_HPP_DEFAULT_DISPATCHER_ASSIGNMENT", "", true);
    setMacro(cfg.macro.mDispatchType, "VULKAN_HPP_DEFAULT_DISPATCHER_TYPE", "", true);

    // fixed
    setMacro(cfg.macro.mNamespaceSTD, "", "std", false);

    // temporary override
    cfg.dbg.methodTags = true;

    //cfg.mcrNamespace.usesDefine = false;
    cfg.macro.mConstexpr.usesDefine = false;
    cfg.macro.mInline.usesDefine = false;
    cfg.macro.mNoexcept.usesDefine = false;
    cfg.macro.mExplicit.usesDefine = false;
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
        {"types", std::bind(&Generator::parseTypes, this,
                            std::placeholders::_1)},
        {"enums", std::bind(&Generator::parseEnums, this,
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

    auto it = enums.find("VkResult");
    if (it == enums.end()) {
        throw std::runtime_error("Missing VkResult in xml registry");
    }
    for (const auto &m : it->second.members) {
        if (!m.isAlias && m.value.starts_with("eError")) {
            errorClasses.emplace(m.value, m.protect);
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
    commands.clear();
    staticCommands.clear();
    errorClasses.clear();    
    loader = HandleData{""};
    initLoaderName();
}

void Generator::generate() {

    std::cout << "generating" << std::endl;

    if (cfg.loaderClassName.empty()) {
        throw std::runtime_error{"Loader class name is empty"};
    }

    std::string p = outputFilePath;
    std::replace(p.begin(), p.end(), '\\', '/');
    if (!p.ends_with('/')) {
        p += '/';
    }
    std::filesystem::path path = std::filesystem::absolute(p);
    std::cout << "path: " << path << std::endl;
    if (!std::filesystem::exists(path)) {
        if (!std::filesystem::create_directory(path)) {
            throw std::runtime_error{"Can't create directory"};
        }
    }
    if (!std::filesystem::is_directory(path)) {
        throw std::runtime_error{"Output path is not a directory"};
    }


    // TODO check existing files

    initLoaderName();
    loader.calculate(*this, loader.name);

    outputFuncs.clear();    
    outputFuncsRAII.clear();
    generatedStructs.clear();
    genStructStack.clear();
    dispatchLoaderBaseGenerated = false;    

    for (auto &h : handles) {
        if (!h.second.generated.empty()) {
            std::cerr << "Warning: handle has uncleaned generated members"
                      << std::endl;
        }
    }

    generateFiles(path);
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

std::string Generator::getNamespace(Namespace ns, bool colons) const {
    if (ns == Namespace::NONE) {
        return "";
    }
    try {
        auto it = namespaces.at(ns);
        return it->get() + (colons? "::" : "");
    }
    catch (std::exception) {
        throw std::runtime_error("getNamespace(): namespace does not exist.");
    }
}

template <class... Args>
std::string Generator::format(const std::string &format,
                              const Args... args) const {

    std::vector<std::string> list;
    static const std::unordered_map<std::string, const Macro &> macros = {
        {"NAMESPACE", cfg.macro.mNamespace},
        {"NAMESPACE_RAII", cfg.macro.mNamespaceRAII},
        {"CONSTEXPR", cfg.macro.mConstexpr},
        {"NOEXCEPT", cfg.macro.mNoexcept},
        {"INLINE", cfg.macro.mInline},
        {"EXPLICIT", cfg.macro.mExplicit}
    };

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
    std::string str = regex_replace(format, rgx, [&](std::smatch const &match) {
        if (match.size() < 2) {
            return std::string{};
        }
        const std::string &str = match[1];
        if (str.empty()) {
            return std::string{};
        }

        const auto &m = macros.find(str);
        matched = true;
        if (m != macros.end()) {
            return m->second.get();
        }

        char *end;
        long index = strtol(str.c_str(), &end, 10);
        if (end && *end != '\0') {
            return std::string{};
        }
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
        std::string s = gen.genOptional(m.name.original,
                        [&](std::string &output) { effectiveMembers++; });
    }
}

void Generator::HandleData::addCommand(const Generator &gen,
                                       const CommandData &cmd) {
    // std::cout << "Added to " << name << ", " << cmd.name << std::endl;
    members.push_back(ClassCommandData{gen, this, cmd});
}

bool Generator::EnumData::containsValue(const std::string &value) const {
    for (const auto &m : members) {
        if (m.value == value) {
            return true;
        }
    }
    return false;
}

std::string Generator::MemberResolverBase::getDbgtag() {
    if (!ctx.gen.getConfig().dbg.methodTags) {
        return "";
    }
    std::string out = "// <" + dbgtag + ">";
    if (ctx.isRaiiOnly()) {
        out += " <RAII indirect>";
    }
    else if (ctx.isIndirect()) {
        out += " <indirect>";
    }
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
