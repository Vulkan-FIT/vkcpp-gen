/*
MIT License

Copyright (c) 2021 guritchi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <unordered_set>
#include <stdexcept>
#include <algorithm>
#include <memory>
#include <regex>
#include <functional>

#include "ArgumentsParser.hpp"
#include "XMLVariableParser.hpp"
#include "XMLUtils.hpp"
#include "FileHandle.hpp"

#include "tinyxml2.h"
using namespace tinyxml2;

static constexpr std::string_view HELP_TEXT {
    R"(Usage:
    -r, --reg       path to source registry file
    -s, --source    path to source directory
    -d, --dest      path to destination file)"
};

struct EnumExtendsValue {
    std::string bitpos;
    std::string name;
};

struct EnumDeclaration {
    std::string attribRequires;
    std::map<std::string, std::vector<EnumExtendsValue>> extendValues;
};

static const std::string NAMESPACE {"vk20"};
static const std::string FILEPROTECT {"VULKAN_20_HPP"};

static FileHandle file; // main output file
static std::string sourceDir; // additional source files directory

static std::unordered_set<std::string> structNames; // list of names from <types>
static std::unordered_set<std::string> tags; // list of tags from <tags>

static std::map<std::string, std::string> platforms; // maps platform name to protect (#if defined PROTECT)
static std::map<std::string, std::string*> extensions; // maps extension to protect as reference

static std::unordered_map<std::string, EnumDeclaration> enums;

// main parse functions
static void parsePlatforms(XMLNode*);
static void parseTypeDeclarations(XMLNode*);
static void parseExtensions(XMLNode*);
static void parseTags(XMLNode*);
static void parseEnums(XMLNode*);
static void parseTypes(XMLNode*);
static void parseCommands(XMLNode*);

// specifies order of parsing vk.xml refistry,
static const std::vector<std::pair<std::string, std::function<void(XMLNode*)>>> rootParseOrder {
{"platforms", parsePlatforms},
{"types", parseTypeDeclarations},
{"extensions", parseExtensions},
{"tags", parseTags},
{"types", parseTypes},
{"enums", parseEnums},
//{"commands", parseCommands}
                                                                            };

static VariableData invalidVar(VariableData::TYPE_INVALID);

static std::string stripVkPrefix(const std::string &str);

using VariableArray = std::vector<std::shared_ptr<VariableData>>;

// holds information about class member (function)
struct ClassMemberData {
    std::string name; // identifier
    std::string type; // return type
    VariableArray params; // list of arguments

    // creates function arguments signature
    std::string createProtoArguments(const std::string &className, bool useOriginal = false) const {
        std::string out;
        for (const auto &data : params) {
            if (data->original.type() == "Vk" + className || data->getIgnoreFlag()) {
                continue;
            }
            out += useOriginal? data->originalToString() : data->toString();
            out += ", ";
        }
        strStripSuffix(out, ", ");
        return out;
    }

    // arguments for calling vulkan API functions
    std::string createPFNArguments(const std::string &className, const std::string &handle, bool useOriginal = false) const {
        std::string out;
        for (size_t i = 0; i < params.size(); ++i) {
            if (params[i]->original.type() == "Vk" + className) {
                out += handle;
            }
            else {
                out += useOriginal? params[i]->original.identifier() : params[i]->toArgument();
            }
            if (i != params.size() - 1) {
                out += ", ";
            }
        }
        return out;
    }

};

template<template<class...> class TContainer, class T, class A>
static bool isInContainter(const TContainer<T, A> &array, T entry) {
    return std::find(std::begin(array), std::end(array), entry) != std::end(array);
}

static bool isStruct(const std::string &name) {
    return isInContainter(structNames, name);
}

// tries to match str in extensions, if found returns pointer to protect, otherwise nullptr
static std::string* findExtensionProtect(const std::string &str) {
    auto it = extensions.find(str);
    if (it != extensions.end()) {
        return it->second;
    }
    return nullptr;
}

// #if defined encapsulation
static void writeWithProtect(const std::string &name, std::function<void()> function) {
    const std::string* protect = findExtensionProtect(name);
    if (protect) {
        file.get() << "#if defined(" << *protect << ")" << ENDL;
    }

    function();

    if (protect) {
        file.get() << "#endif //" << *protect << ENDL;
    }
}

static std::string strRemoveTag(std::string &str) {
    std::string suffix;
    auto it = str.rfind('_');
    if (it != std::string::npos) {
        suffix = str.substr(it + 1);
        if (tags.find(suffix) != tags.end()) {
            str.erase(it);
        }
        else {
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

static std::string strWithoutTag(const std::string &str) {
    std::string out = str;
    for (const std::string &tag : tags) {
        if (out.ends_with(tag)) {
            out.erase(out.size() - tag.size());
            break;
        }
    }
    //if (out != str)
       // std::cout << "Returning without tag: " << out << "( " << str << " ) " << std::endl;
    return out;
}

static std::pair<std::string, std::string> snakeToCamelPair(std::string str) {
    std::string suffix = strRemoveTag(str);
    std::string out = convertSnakeToCamel(str);

    out = std::regex_replace(out, std::regex("bit"), "Bit");
    out = std::regex_replace(out, std::regex("Rgba10x6"), "Rgba10X6");

    return std::make_pair(out, suffix);
}

static std::string snakeToCamel(const std::string &str) {
    const auto &p = snakeToCamelPair(str);
    return p.first + p.second;
}

static std::string enumConvertCamel(const std::string &enumName, std::string value) {
    //std::cout << value << " to enum" << std::endl;
    //std::cout << "  strip: " << "VK_" + camelToSnake(enumName) << std::endl;
    strStripPrefix(value, "VK_" + camelToSnake(enumName));
    strStripPrefix(value, "VK_");
    //std::cout << "  after: " << value << std::endl;
    std::string out = snakeToCamel(value);
    if (!out.empty()) {
        out[0] = std::toupper(out[0]);
    }
    return "e" + out;
}

static std::string toCppStyle(const std::string &str) {
    std::string out = stripVkPrefix(str);
    if (!out.empty()) {
        out[0] = std::tolower(out[0]);
    }
    return out;
}

static void parseXML(XMLElement *root) {

    // maps all root XMLNodes to their tag identifier
    std::vector<std::pair<std::string, XMLNode*>> rootTable;
    for (XMLNode &node : Nodes(root)) {        
        rootTable.push_back(std::make_pair(node.Value(), &node));
    }

    // call each function in rootParseOrder with corresponding XMLNode
    for (auto &key : rootParseOrder) {
        for (auto &n : rootTable) {
            // find tag id

            if (n.first == key.first) {
                key.second(n.second); //call function(node*)
            }
        }
    }
}

static void generateReadFromFile(const std::string &path) {
    std::ifstream inputFile;
    inputFile.open(path);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Can't open file: " + path);
    }

    for(std::string line; getline(inputFile, line);) {
        file.writeLine(line);
    }

    inputFile.close();
}

static void generateFile(XMLElement *root) {
    file.writeLine("#ifndef " + FILEPROTECT);
    file.writeLine("#define " + FILEPROTECT);

    file.writeLine("#include <vulkan/vulkan_core.h>");
    file.writeLine("#include <vulkan/vulkan.hpp>");
    file.writeLine("#include <bit>");

    file.writeLine("#ifdef _WIN32");
    file.writeLine("# define WIN32_LEAN_AND_MEAN");
    file.writeLine("# include <windows.h>");
    file.writeLine("#else");
    file.writeLine("# include <dlfcn.h>");
    file.writeLine("#endif");

    file.writeLine("// Windows defines MemoryBarrier which is deprecated and collides");
    file.writeLine("// with the VULKAN_HPP_NAMESPACE::MemoryBarrier struct.");
    file.writeLine("#if defined( MemoryBarrier )");
    file.writeLine("#  undef MemoryBarrier");
    file.writeLine("#endif");

    file.writeLine("namespace " + NAMESPACE);
    file.writeLine("{");

    file.pushIndent();
    file.writeLine("using namespace vk;");

    generateReadFromFile(sourceDir + "/source_libraryloader.hpp");
    parseXML(root);

    file.popIndent();

    file.writeLine("}");
    file.writeLine("#endif //" + FILEPROTECT);
}

int main(int argc, char** argv) {
    try {
        ArgOption helpOption{"-h", "--help"};
        ArgOption xmlOption{"-r", "--reg", true};
        ArgOption sourceOption{"-s", "--source", true};
        ArgOption destOpton{"-d", "--dest", true};
        ArgParser p({&helpOption,
                     &xmlOption,
                     &sourceOption,
                     &destOpton
                    });
        p.parse(argc, argv);
        // help option block
        if (helpOption.set) {
            std::cout << HELP_TEXT;
            return 0;
        }
        // argument check
        if (!destOpton.set || !xmlOption.set || !sourceOption.set) {
            throw std::runtime_error("Missing arguments. See usage.");
        }

        sourceDir = sourceOption.value;

        file.open(destOpton.value);

        XMLDocument doc;
        if (XMLError e = doc.LoadFile(xmlOption.value.c_str()); e != XML_SUCCESS) {
            throw std::runtime_error("XML load failed: " + std::to_string(e) + " (file: " + xmlOption.value + ")");
        }

        XMLElement *root = doc.RootElement();
        if (!root) {
            throw std::runtime_error("XML file is empty");
        }

        generateFile(root);

        std::cout << "Parsing done" << ENDL;

    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    file.close();
    return 0;
}

std::vector<std::string> parseStructMembers(XMLElement *node, std::string &structType, std::string &structTypeValue) {
    std::vector<std::string> members;
    // iterate contents of <type>, filter only <member> children
    for (XMLElement *member : Elements(node) | ValueFilter("member")) {
        std::string out;

        XMLVariableParser parser{member}; // parse <member>

        std::string type = parser.type();
        std::string name = parser.identifier();
        if (isStruct(type)) {
            parser.set(0, parser.prefix() + NAMESPACE + "::"); // temporary solution!
        }
        out = parser.toString();

        if (const char *values = member->ToElement()->Attribute("values")) {
            std::string value = enumConvertCamel(type, values);
            out += (" = " + type + "::" + value);
            if (name == "sType") { // save sType information for structType
                structType = type;
                structTypeValue = value;
            }
        }
        else {
            if (name == "sType") {
                out += " = StructureType::eApplicationInfo";
            }
            else {
                out += " = {}";
            }
        }

        out += ";";
        members.push_back(out);

    }
    return members;
}


void parseStruct(XMLElement *node, std::string name) {
    writeWithProtect(name, [&]{


        strStripVk(name);
        structNames.emplace(name);

        if (const char *aliasAttrib = node->Attribute("alias")) {
            file.writeLine("using " + name + " = " + strStripVk(std::string(aliasAttrib)) +";");
            return;
        }

        std::string structType{}, structTypeValue{}; // placeholders
        std::vector<std::string> members = parseStructMembers(node, structType, structTypeValue);

        file.writeLine("struct " + name);
        file.writeLine("{");

        file.pushIndent();
        if (!structType.empty() && !structTypeValue.empty()) { // structType
            file.writeLine("static VULKAN_HPP_CONST_OR_CONSTEXPR " + structType + " structureType = "
                           + structType + "::" + structTypeValue + ";");
            file.get() << ENDL;
        }
        // structure members
        for (const std::string &str : members) {
            file.writeLine(str);
        }
        file.get() << ENDL;

        file.writeLine("operator "+ NAMESPACE + "::" + name + "*() { return this; }");
        file.writeLine("operator vk::" + name + "&() { return *reinterpret_cast<vk::" + name + "*>(this); }");

        file.writeLine("operator Vk" + name + " const&() const { return *reinterpret_cast<const Vk" + name + "*>(this); }");
        file.writeLine("operator Vk" + name + " &() { return *reinterpret_cast<Vk" + name + "*>(this); }");

        file.popIndent();

        file.writeLine("};");
    });
}

void parsePlatforms(XMLNode *node) {
    std::cout << "Parsing platforms" << ENDL;
    // iterate contents of <platforms>, filter only <platform> children
    for (XMLElement *platform : Elements(node) | ValueFilter("platform")) {
        const char *name = platform->Attribute("name");
        const char *protect = platform->Attribute("protect");
        if (name && protect) {
            platforms.emplace(name, protect);
        }
    }
    std::cout << "Parsing platforms done" << ENDL;
}

void parseExtensions(XMLNode *node) {
    std::cout << "Parsing extensions" << ENDL;
    // iterate contents of <extensions>, filter only <extension> children
    for (XMLElement *extension : Elements(node) | ValueFilter("extension")) {
        const char *platform = extension->Attribute("platform");
        if (platform) {
            auto it = platforms.find(platform);
            if (it != platforms.end()) {
                // iterate contents of <extension>, filter only <require> children
                for (XMLElement *require : Elements(extension) | ValueFilter("require")) {
                    // iterate contents of <require>
                    for (XMLElement &entry : Elements(require)) {                        
                        const std::string_view value = entry.Value();
                        if (value == "command") {
                            const char *name = entry.Attribute("name");
                            if (name) { // pair extension name with platform protect
                                //std::cout << "protect pair: " << name << ", " << it->second << std::endl;
                                extensions.emplace(name, &it->second);
                            }
                        }
                        else if (value == "enum") {
                            const char* extends = entry.Attribute("extends");
                            const char* bitpos = entry.Attribute("bitpos");
                            const char* name = entry.Attribute("name");
                            const char* protect = entry.Attribute("protect");
                            if (extends && name && bitpos && protect) {
                                auto it = enums.find(extends);
                                if (it != enums.end()) {
                                   // std::cout << "extends " << extends << " : " << protect << std::endl;
                                    auto values = it->second.extendValues.find(protect);
                                    if (values == it->second.extendValues.end()) {
                                        it->second.extendValues.emplace(protect, std::vector<EnumExtendsValue>{});
                                        values = it->second.extendValues.find(protect);
                                    }
                                    values->second.push_back(EnumExtendsValue{
                                        .bitpos = bitpos,
                                        .name = name
                                    });
                                    //std::cout << "pushed to " << it->first << std::endl;
                                    for (auto &a : values->second) {
                                        //std::cout << "  " << a.name << std::endl;
                                    }

                                    {
                                    auto it = enums.find(extends);
                                        if (it != enums.end()) {
                                            for (auto &t : it->second.extendValues) {
                                              //  std::cout << "test iter: " << t.first << std::endl;
                                            }
                                        }

                                    }
                                }
                            }

                        }
                    }
                }
            }
            else {
                std::cerr << "Unknown platform in extensions: " << platform << std::endl;
            }
        }
    }
    std::cout << "Parsing extensions done" << ENDL;
}

void parseTags(XMLNode *node) {
    std::cout << "Parsing tags" << ENDL;
    // iterate contents of <tags>, filter only <tag> children
    for (XMLElement *tag : Elements(node) | ValueFilter("tag")) {
        const char *name = tag->Attribute("name");
        if (name) {
            tags.emplace(name);
        }
    }
    std::cout << "Parsing tags done" << ENDL;
}


void generateEnum(std::string name, XMLNode *node) {
    return;

    auto it = enums.find(name);
    if (it == enums.end()) {
        std::cerr << "cant find " << name << "in enums" << std::endl;
        return;
    }
    writeWithProtect(name, [&]{

        name = toCppStyle(name);
        name[0] = std::toupper(name[0]);

        std::string cname = name;
        std::string tag = strRemoveTag(cname);

        //std::string cname = camelToSnake(name);
        //strAddPrefix(cname, "VK_");
        std::vector<std::string> values;

        // iterate contents of <enums>, filter only <enum> children
        for (XMLElement *e : Elements(node) | ValueFilter("enum")) {
            const char *name = e->Attribute("name");
            if (name) {
                values.push_back(name);
            }
        }

        file.writeLine("enum class " + name + " {" + " // " + cname + " - " + tag);
        file.pushIndent();

        for (size_t i = 0; i < values.size(); ++i) {
            std::string c = values[i];
            //writeWithProtect(c, [&]{
                std::string cpp;
                //if (strContains(cpp, cname)) {
                cpp = enumConvertCamel(cname, c);
                //}
                if (cpp.ends_with(tag)) {
                    cpp.erase(cpp.size() - tag.size());
                }
                std::string comma = "";
                if (i != values.size() - 1) {
                  comma = ",";
                }
                file.writeLine(cpp + " = " + c + comma);
            //});

        }

        std::cout << "gen enum: " << name << std::endl;
        for (auto &v : it->second.extendValues) {
            std::cout << "extend protect: " << v.first << std::endl;
            for (auto &e : v.second) {
                std::cout << name << " : " << "extend: " << e.name;
            }
        }


        file.popIndent();
        file.writeLine("};\n");
    });

}

void parseEnums(XMLNode *node) {

    const char *name = node->ToElement()->Attribute("name");
    if (!name) {
        std::cerr << "Can't get name of enum" << std::endl;
        return;
    }
   // std::cout << "Parsing enum: " << name << std::endl;

    const char *type = node->ToElement()->Attribute("type");
    if (!type) {
        return;
    }
    if (std::string_view(type) == "enum" || std::string_view(type) == "bitmask") {
        generateEnum(name, node);
    }
}

void parseTypeDeclarations(XMLNode *node) {
    std::cout << "Parsing declarations" << std::endl;
    std::vector<XMLElement*> types;
    // iterate contents of <types>, filter only <type> children
    for (XMLElement *type : Elements(node) | ValueFilter("type")) {
        types.push_back(type);
    }
    for (auto &e : types) {
        const char *cat = e->Attribute("category");
        const char *name = e->Attribute("name");
        if (cat && name) {
            if (strcmp(cat, "enum") == 0) {
                //std::cout << "decl enum: " << name << std::endl;
                enums.emplace(name, EnumDeclaration{
                    .attribRequires = ""
                });
            }
        }

    }
    for (auto &e : types) {
        const char *cat = e->Attribute("category");
        //const char *name = e->Attribute("name");
        if (cat && strcmp(cat, "bitmask") == 0) {
            const char *req = e->Attribute("requires");
            if (req) {
                XMLElement *nameElement = e->FirstChildElement("name");
                if (nameElement) {
                    const char *name = nameElement->GetText();
                    if (name) {
                        //std::cout << "requires: " << name << " -> " << req << std::endl;
                        auto it = enums.find(req);
                        if (it != enums.end()) {
                            it->second.attribRequires = name;
                        }
                        else {
                            std::cerr << "Can't find enum in require bitmask: " << req << std::endl;;
                        }
                    }
                }
            }
        }

    }

    std::cout << "Parsing types done" << ENDL;
}

void parseTypes(XMLNode *node) {
    std::cout << "Parsing types" << std::endl;
    // iterate contents of <types>, filter only <type> children
    for (XMLElement *type : Elements(node) | ValueFilter("type")) {
        const char *cat = type->Attribute("category");
        const char *name = type->Attribute("name");
        if (cat && name) {
            if (strcmp(cat, "struct") == 0) {
                parseStruct(type, name);
            }
            else if (strcmp(cat, "bitmask") == 0) {

            }
        }
    }
    std::cout << "Parsing types done" << ENDL;
}

ClassMemberData parseClassMember(XMLElement *command) {
    ClassMemberData m;
    // iterate contents of <command>
    for (XMLElement &child : Elements(command) ) {
        // <proto> section
        if (std::string_view(child.Value()) == "proto") {
            // get <name> field in proto
            XMLElement *nameElement = child.FirstChildElement("name");
            if (nameElement) {
                m.name = nameElement->GetText();
            }
            // get <type> field in proto
            XMLElement *typeElement = child.FirstChildElement("type");
            if (typeElement) {
                m.type = typeElement->GetText();
            }
        }
        // <param> section
        else if (std::string_view(child.Value()) == "param") {
            // parse inside of param
            std::shared_ptr<XMLVariableParser> parser = std::make_shared<XMLVariableParser>(&child);
            // add proto data to list
            m.params.push_back(parser);
        }
    }

    return m;
}

std::vector<ClassMemberData> parseClassMembers(const std::vector<XMLElement*> &elements) {
    std::vector<ClassMemberData> list;
    for (XMLElement *command : elements) {
        list.push_back(parseClassMember(command));
    }
    return list;
}

std::string generateClassMemberCStyle(const std::string &className, const std::string &handle, const std::string &protoName, ClassMemberData& m) {

    std::string protoArgs = m.createProtoArguments(className, true);
    std::string innerArgs = m.createPFNArguments(className, handle, true);
    file.writeLine("inline " + m.type + " " + protoName + "(" + protoArgs + ") { // C");

    file.pushIndent();
    std::string cmdCall;
    if (m.type != "void") {
        cmdCall += "return ";
    }
    cmdCall += ("m_" + m.name + "(" + innerArgs + ");");
    file.writeLine(cmdCall);
    file.popIndent();

    file.writeLine("}");
    return protoArgs;
}

enum class PFNReturnCategory {
    OTHER,
    VOID,
    VK_RESULT,
    VK_OBJECT
};

// debug
//static std::string toString(MEMBER_RETURN category) {
//    switch (category) {
//    case MEMBER_RETURN::INVALID:         return "N/A";
//    case MEMBER_RETURN::VOID:            return "void";
//    case MEMBER_RETURN::RESULT:          return "result";
//    case MEMBER_RETURN::STRUCT:          return "struct";
//    case MEMBER_RETURN::ARRAY_IN:        return "array_input";
//    case MEMBER_RETURN::ARRAY_OUT:       return "array_output";
//    case MEMBER_RETURN::ARRAY_OUT_ALLOC: return "array_alloc";
//    }
//    return "";
//}

enum class ArraySizeArgument {
    INVALID,
    COUNT,
    SIZE,
    CONST_COUNT
};

enum class MemberNameCategory {
    UNKNOWN,
    GET,
    GET_ARRAY,
    ALLOCATE,
    ALLOCATE_ARRAY,
    CREATE,
    CREATE_ARRAY,
    ENUMERATE
};

struct MemberContext {
    const std::string &className;
    const std::string &handle;
    const std::string &protoName;
    const PFNReturnCategory &pfnReturn;
    ClassMemberData mdata;

//    MemberContext(const MemberContext &other) :
//        className(other.className),
//        handle(other.handle),
//        protoName(other.protoName)
//    {
//        std::cout << "Copy constructor" << ENDL;

//    }
};

bool containsCountVariable(const VariableArray &params) {
    for (const auto &v : params) {
        if (v->identifier().ends_with("Count")) {
            return true;
        }
    }
    return false;
}

bool containsLengthAttrib(const VariableArray &params) {
    for (const auto &v : params) {
        if (v->hasLenAttrib()) {
            return true;
        }
    }
    return false;
}

MemberNameCategory evalMemberNameCategory(MemberContext &ctx) {
    const std::string& name = ctx.protoName;
   // std::cout << "  evalMemberNameCategory: " << name << std::endl;
    bool endsWithS = strWithoutTag(name).ends_with("s");
    bool containsCountVar = containsLengthAttrib(ctx.mdata.params);
    bool arrayFlag = containsCountVar;
    std::cout << "endswithS: " << endsWithS << ", length attrib: " << containsCountVar << ENDL;

    if (name.starts_with("get")) {
        return arrayFlag ? MemberNameCategory::GET_ARRAY
                         : MemberNameCategory::GET;
    }
    if (name.starts_with("allocate")) {
        if (arrayFlag) {
            return MemberNameCategory::ALLOCATE_ARRAY;
        }
        else {
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

static bool isTypePointer(const VariableData &m) {
    return strContains(m.suffix(), "*");
}

ArraySizeArgument evalArraySizeArgument(const VariableData &m) {
    std::cout << "evalArraySizeArgument: " << m.identifier() << ENDL;
    if (m.identifier().ends_with("Count")) {
        return isTypePointer(m) ? ArraySizeArgument::COUNT
                                : ArraySizeArgument::CONST_COUNT;
    }
    if (m.identifier().ends_with("Size")) {
        return ArraySizeArgument::SIZE;
    }
//            strContains(m.type(), "int"))
//    {
//        return ArraySizeArgument::COUNT;
//    }
//    if (strContains(m.identifier(), "size")  &&
//            strContains(m.type(), "size_t"))
//    {
//        return ArraySizeArgument::SIZE;
//    }
    return ArraySizeArgument::INVALID;
}

class MemberResolver {

private:


    void transformMemberArguments() {
        //return;
        auto &params = ctx.mdata.params;


        {
           for (VariableArray::iterator it = params.begin(); it != params.end(); ++it) {
                std::cout << "  Param: " << it->get()->identifier() << " " << it->get()->hasLenAttrib() << ENDL;
            }
        }

        //VariableArray::iterator it_last = std::prev(params.end());
        VariableArray::iterator it = params.begin();

        const auto findSizeVar = [&](std::string identifier) {           
           for (const auto &it : params) {
                if (it->identifier() == identifier) {
                    return it;
                    break;
                }
           }
           return std::make_shared<VariableData>(VariableData::TYPE_INVALID);
        };

        const auto transformArgument = [&]() {
            if (it->get()->hasLenAttrib()) {
                std::string lenAttrib = it->get()->lenAttrib();
                if (lenAttrib == "null-terminated") {
                    //std::cout << "(in: " << ctx.protoName << ")" << ">>>> NULL TERM" << std::endl;
                    //it->set
                    return;
                }
                std::string len = it->get()->lenAttribVarName();
                const std::shared_ptr<VariableData> &sizeVar = findSizeVar(len);

                std::cout << "Length attrib  " << len << "  on: " << it->get()->identifier() << ENDL;
                std::cout << "Size var find: " << sizeVar->identifier() << ENDL;
                if (sizeVar->isInvalid()) {
                    // length param not found
                    std::cerr << "(in: " << ctx.protoName << ")" << " Length param not found: " << len << ENDL;
                    return;
                }
                if (!strContains(lenAttrib, "->")) {
                    if (!sizeVar->getIgnoreFlag()) {
                        //std::cout << "Setting ignore on sizevar: " << sizeVar->identifier() << ENDL;
                        sizeVar->setIgnoreFlag(true);
                        std::string alt = it->get()->identifier() + ".size()";
                        if (it->get()->type() == "void") {
                            alt += " * sizeof(T)";
                        }
                        sizeVar->setAltPFN(alt);
                    }
                }
                it->get()->bindLengthAttrib(sizeVar);
                if (it->get()->type() == "void") {
                    templates.push_back("typename T");
                    it->get()->setFullType("", "T", "");
                }
                it->get()->convertToArrayProxy(false);
            }

        };

        while (it != params.end()) {

            transformArgument();
            if (it == params.end()) {
                break;
            }
            it++;
        }
    }

    std::string declareReturnVar(const std::string assignment) {
        returnVar.setIdentifier("result");
        returnVar.setFullType("", "Result", "");
        returnVar.setType(VariableData::TYPE_DEFAULT);
        if (!assignment.empty()) {
            return returnVar.toString() + " = " + assignment + ";";
        }
        else {
            return returnVar.toString() + ";";
        }
    }

protected:

    MemberContext ctx;
    std::string returnType;
    std::vector<std::string> templates;

    VariableData returnVar;

    virtual void generateMemberBody() {
         file.writeLine(generatePFNcall());
         if (returnType == "Result") {
            file.writeLine("return " + returnVar.identifier() + ";");
         }
    }

    std::string generatePFNcall() {
        std::string arguments = ctx.mdata.createPFNArguments(ctx.className, ctx.handle);
        std::string call = "m_" + ctx.mdata.name + "(" + arguments + ")";
        switch (ctx.pfnReturn) {
            case PFNReturnCategory::VK_RESULT:
                return assignToResult("static_cast<Result>(" + call + ")");
                break;
            case PFNReturnCategory::VK_OBJECT:
                return "return static_cast<" + returnType + ">(" + call + ");";
                break;
            case PFNReturnCategory::OTHER:
                return "return " + call + ";";
                break;
            case PFNReturnCategory::VOID:
            default:
                return call + ";";
                break;
        }
    }

    std::string assignToResult(const std::string &assignment) {
        //std::cout << "   " << "Assign: " << assignment << ENDL;
        if (returnVar.isInvalid()) {
          //  std::string temp = declareReturnVar(assignment);
            //std::cout << "   " << "Decl: " << temp << ENDL;
            return declareReturnVar(assignment);
        }
        else {
            return returnVar.identifier() + " = " + assignment + ";";
        }
    }

public:
    std::string dbgtag;

    MemberResolver(MemberContext &refCtx) : ctx(refCtx), returnVar(VariableData::TYPE_INVALID) {

        returnType = ctx.mdata.type;
        returnType = strStripVk(returnType.data());
        //std::cout << "Member resolver created. (Ret: " << returnType << ")" << ENDL;

        transformMemberArguments();
        dbgtag = "default";
    }

    virtual ~MemberResolver() {
        //std::cout << "Member resolver destroyed." << ENDL;
    }

    void generate() {       


        if (!templates.empty()) {
            std::string temp;
            for (const std::string &str : templates) {
                temp += str;
                temp += ", ";
            }
            strStripSuffix(temp, ", ");
            file.writeLine("template <" + temp + ">");
        }

        std::string protoArgs = ctx.mdata.createProtoArguments(ctx.className);
        std::string proto = "inline " + returnType + " " + ctx.protoName + "(" + protoArgs + ") {"
                            + " // [" + dbgtag + "]";


        file.writeLine(proto);
        file.pushIndent();
        file.writeLine("// " + dbgtag);

        generateMemberBody();

        file.popIndent();
        file.writeLine("}");
    }

};

class MemberResolverStruct : public MemberResolver {

protected:
    std::shared_ptr<VariableData> lastVar;

public:
    MemberResolverStruct(MemberContext &refCtx) : MemberResolver(refCtx) {
        lastVar = ctx.mdata.params[ctx.mdata.params.size() - 1];

        lastVar->convertToReturn();

        returnType = lastVar->fullType();

        dbgtag = "single return";
    }

    void generateMemberBody() override {        
        file.writeLine(returnType + " " + lastVar->identifier() + ";");

        file.writeLine(generatePFNcall());

        file.writeLine("return " + lastVar->identifier() + ";");
    }

};

class MemberResolverGet : public MemberResolver {

protected:
    std::shared_ptr<VariableData> last;
    std::shared_ptr<VariableData> lenVar;
    std::string initSize;

    std::shared_ptr<VariableData> getReturnVar() {

        std::string name = ctx.protoName;
        strRemoveTag(name);
        strStripPrefix(name, "get");

        std::cout << "Get return var. " << name << std::endl;

        VariableArray::reverse_iterator it = ctx.mdata.params.rbegin();
        while (it != ctx.mdata.params.rend()) {
            std::cout << it->get()->type() << " vs " << name << std::endl;
            if (strContains(it->get()->type(), name)) {
                std::cout << "name match: " << it->get()->identifier() << std::endl;
                return *it;
            }
            if (it->get()->hasLengthAttribVar()) {
                return *it;
            }
            it++;
        }
        std::cerr << "Can't get return var" << std::endl;
        return std::make_shared<VariableData>(VariableData::TYPE_INVALID);
    }

    bool returnsArray;


    void generateArray() {
      file.writeLine(last->toString() + initSize + ";");
        std::string size = last->lenAttrib();
        if (lenVar->getIgnoreFlag()) {
            file.writeLine(lenVar->type() + " " + lenVar->identifier() + ";");
        }

        if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
            file.writeLine(assignToResult(""));
            std::string call = generatePFNcall();
            file.writeLine("do {");
            file.pushIndent();

            file.writeLine(call);
            file.writeLine("if (( result == Result::eSuccess ) && " + lenVar->identifier() + ") {");
            file.pushIndent();

            file.writeLine(last->identifier() + ".resize(" + size + ");");
            file.writeLine(call);

            file.writeLine("//VULKAN_HPP_ASSERT( " + lenVar->identifier() + " <= " + last->identifier() + ".size() );");

            file.popIndent();
            file.writeLine("}");

            file.popIndent();
            file.writeLine("} while ( result == Result::eIncomplete );");

            file.writeLine("if ( ( result == Result::eSuccess ) && ( " + lenVar->identifier() + "< " + last->identifier() + ".size() ) ) {");
            file.pushIndent();

            file.writeLine(last->identifier() + ".resize(" + size + ");");

            file.popIndent();
            file.writeLine("}");

            file.writeLine("return " + last->identifier() + ";");
        }
        else {
            std::string call = generatePFNcall();

            file.writeLine(call);
            file.writeLine(last->identifier() + ".resize(" + size + ");");

            file.writeLine(call);
            file.writeLine("//VULKAN_HPP_ASSERT( " + lenVar->identifier() + " <= " + last->identifier() + ".size() );");

            file.writeLine("return " + last->identifier() + ";");
        }
    }

public:
    MemberResolverGet(MemberContext &refCtx) : MemberResolver(refCtx) {

        dbgtag = "get array";

        last = getReturnVar();


        returnsArray = last->hasLengthAttribVar();

        if (returnsArray) {
            lenVar = last->lengthAttribVar();
            std::cout << "has lenVar: " << lenVar->identifier() << std::endl;

            if (last->type() == "void") {
                //std::cerr << "(in " << ctx.protoName << ") " << "void vector" << std::endl;
                last->setFullType("", "T", "");
                templates.push_back("typename T");
                lenVar->setIgnoreFlag(false);
                initSize = "(" + lenVar->identifier() + " / sizeof(T))";
                std::cout << "initsize:" << initSize << std::endl;
                lenVar->setAltPFN(lenVar->identifier() + " * sizeof(T)");

            }
            else {
                if (!strContains(last->lenAttrib(), "->")) {
                    std::string ref = strContains(lenVar->original.suffix(), "*")? "&" : "";
                    lenVar->setAltPFN(ref + lenVar->identifier());
                }
            }

            last->convertToStdVector();
            last->setIgnoreFlag(true);

        }
        else {
            last->convertToReturn();
        }

        returnType = last->fullType();
    }

    void generateMemberBody() override {

        if (returnsArray) {
            generateArray();
        }
        else {
            file.writeLine(returnType + " " + last->identifier() + ";");

            file.writeLine(generatePFNcall());

            file.writeLine("return " + last->identifier() + ";");
        }
    }

};

class MemberResolverCreateArray : public MemberResolver {

protected:
    std::shared_ptr<VariableData> infoVar;
    std::shared_ptr<VariableData> lastVar;

public:
    MemberResolverCreateArray(MemberContext &refCtx) :
        MemberResolver(refCtx)
    {
        for (auto & var : ctx.mdata.params) {
            //if (var.identifier() == "pAllocateInfo") {
            if (strContains(var->type(), "CreateInfo")) {
                infoVar = var;
               // std::cout << "Got info var. " << infoVar.fullType() << ENDL;
                break;
            }
        }
        if (!infoVar.get()) {
            std::cerr << "Error: MemberResolverAllocateArray: pCreateInfo not found." << std::endl;
        }

        lastVar = ctx.mdata.params[ctx.mdata.params.size() - 1];

        lastVar->convertToStdVector();
        lastVar->setIgnoreFlag(true);
        returnType = lastVar->fullType();

        dbgtag = "create array";
    }

  void generateMemberBody() override {
        std::string vectorSize = infoVar->identifier() + ".size()";
        file.writeLine(lastVar->toString() + "(" + vectorSize + ");");

        file.writeLine(generatePFNcall());

        file.writeLine("return " + lastVar->identifier() + ";");
    }

};

class MemberResolverAllocateArray : public MemberResolver {

protected:
    //std::shared_ptr<VariableData> infoVar;
    std::shared_ptr<VariableData> lastVar;

public:

    MemberResolverAllocateArray(MemberContext &refCtx) :
        MemberResolver(refCtx)
    {

//        for (auto & var : ctx.mdata.params) {
//            //if (var.identifier() == "pAllocateInfo") {
//            if (strContains(var->type(), "AllocateInfo")) {
//                infoVar = var;
//               // std::cout << "Got info var. " << infoVar.fullType() << ENDL;
//                break;
//            }
//        }

//        if (!infoVar.get()) {
//            std::cerr << "Error: MemberResolverAllocateArray: pAllocateInfo not found." << std::endl;
//        }

        lastVar = ctx.mdata.params[ctx.mdata.params.size() - 1];

        lastVar->convertToStdVector();
        lastVar->setIgnoreFlag(true);
        returnType = lastVar->fullType();
    }

    void generateMemberBody() override {
        //std::string allocStructMember = toCppStyle(lastVar->type()) + "Count";
        std::string vectorSize = lastVar->lenAttrib();

        file.writeLine(lastVar->toString() + "(" + vectorSize + ");");

        file.writeLine(generatePFNcall());

        file.writeLine("return " + lastVar->identifier() + ";");
    }

};

class MemberResolverReturnProxy : public MemberResolver {

protected:

    std::shared_ptr<VariableData> last;
    std::shared_ptr<VariableData> sizeVar;

public:
    MemberResolverReturnProxy(MemberContext &refCtx) : MemberResolver(refCtx) {
        last = *ctx.mdata.params.rbegin();
        sizeVar = *std::next(ctx.mdata.params.rbegin());

        last->convertToTemplate();
        sizeVar->setAltPFN("sizeof(T)");

        sizeVar->setIgnoreFlag(true);
        last->setIgnoreFlag(true);

        returnType = last->type();
        templates.push_back("typename T");

        dbgtag = "Return template proxy";
    }

    void generateMemberBody() override {
        file.writeLine("T " + last->identifier() + ";");

        file.writeLine(generatePFNcall()); // TODO missing &

        file.writeLine("return " + last->identifier() + ";");
    }

};

class MemberResolverReturnVectorOfProxy : public MemberResolver {

protected:
    std::shared_ptr<VariableData> last;
    std::shared_ptr<VariableData> sizeVar;


public:
    MemberResolverReturnVectorOfProxy(MemberContext &refCtx) : MemberResolver(refCtx) {
        last = *ctx.mdata.params.rbegin();
        sizeVar = *std::next(ctx.mdata.params.rbegin());

        last->setFullType("", "T", "");
        last->convertToStdVector();

        sizeVar->setAltPFN(last->identifier() + ".size()" " * " "sizeof(T)");

        last->setIgnoreFlag(true);
        templates.push_back("typename T");

        dbgtag = "Return vector of template proxy";

    }

    void generateMemberBody() override {
        file.writeLine("VULKAN_HPP_ASSERT( " + sizeVar->identifier() + " % sizeof( T ) == 0 );");

        file.writeLine(last->toString() + "( " + sizeVar->identifier() + " / sizeof(T)" " );");
        file.writeLine(generatePFNcall());
        file.writeLine("return " + last->identifier() + ";");
    }

};

class MemberResolverReturnVectorData : public MemberResolver {

protected:
    std::shared_ptr<VariableData> last;
    std::shared_ptr<VariableData> lenVar;

public:
    MemberResolverReturnVectorData(MemberContext &refCtx) : MemberResolver(refCtx) {
        dbgtag = "vector data";
        last = *ctx.mdata.params.rbegin();

        lenVar = last->lengthAttribVar();

        last->convertToStdVector();
        last->setIgnoreFlag(true);

        returnType = last->fullType();
        lenVar->setAltPFN("&" + lenVar->original.identifier());
    }

    void generateMemberBody() override {
        file.writeLine(last->toString() + ";");
        file.writeLine(lenVar->type() + " " + lenVar->identifier() + ";");

        if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
            file.writeLine(assignToResult(""));
            std::string call = generatePFNcall();
            file.writeLine("do {");
            file.pushIndent();

            file.writeLine(call);
            file.writeLine("if (( result == Result::eSuccess ) && " + lenVar->identifier() + ") {");
            file.pushIndent();

            file.writeLine(last->identifier() + ".resize(" + lenVar->identifier() + ");");
            file.writeLine(call);

            file.writeLine("//VULKAN_HPP_ASSERT( " + lenVar->identifier() + " <= " + last->identifier() + ".size() );");

            file.popIndent();
            file.writeLine("}");

            file.popIndent();
            file.writeLine("} while ( result == Result::eIncomplete );");

            file.writeLine("if ( ( result == Result::eSuccess ) && ( " + lenVar->identifier() + "< " + last->identifier() + ".size() ) ) {");
            file.pushIndent();

            file.writeLine(last->identifier() + ".resize(" + lenVar->identifier() + ");");

            file.popIndent();
            file.writeLine("}");

            file.writeLine("return " + last->identifier() + ";");
        }

    }

};

class MemberResolverEnumerate : public MemberResolver {

protected:
    std::shared_ptr<VariableData> last;
    std::shared_ptr<VariableData> lenVar;

public:
    MemberResolverEnumerate(MemberContext &refCtx) : MemberResolver(refCtx) {
        dbgtag = "enumerate";
        last = *ctx.mdata.params.rbegin();

        lenVar = last->lengthAttribVar();

        last->convertToStdVector();
        last->setIgnoreFlag(true);

        returnType = last->fullType();
        lenVar->setAltPFN("&" + lenVar->original.identifier());
    }

    void generateMemberBody() override {
        file.writeLine(last->toString() + ";");
        file.writeLine(lenVar->type() + " " + lenVar->identifier() + ";");

        if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
            file.writeLine(assignToResult(""));
            std::string call = generatePFNcall();
            file.writeLine("do {");
            file.pushIndent();

            file.writeLine(call);
            file.writeLine("if (( result == Result::eSuccess ) && " + lenVar->identifier() + ") {");
            file.pushIndent();

            file.writeLine(last->identifier() + ".resize(" + lenVar->identifier() + ");");

            file.writeLine(call);

            file.writeLine("//VULKAN_HPP_ASSERT( " + lenVar->identifier() + " <= " + last->identifier() + ".size() );");

            file.popIndent();
            file.writeLine("}");

            file.popIndent();
            file.writeLine("} while ( result == Result::eIncomplete );");

            file.writeLine("if ( ( result == Result::eSuccess ) && ( " + lenVar->identifier() + "< " + last->identifier() + ".size() ) ) {");
            file.pushIndent();

            file.writeLine(last->identifier() + ".resize(" + lenVar->identifier() + ");");

            file.popIndent();
            file.writeLine("}");

            file.writeLine("return " + last->identifier() + ";");
        }

    }

};

PFNReturnCategory evaluatePFNReturn(const std::string &type) {
    if (type == "void") {
        return PFNReturnCategory::VOID;
    }
    else if (type == "VkResult") {
        return PFNReturnCategory::VK_RESULT;
    }
    else if (type.starts_with("Vk")) {
        return PFNReturnCategory::VK_OBJECT;
    }
    else {
        //std::cerr << "      Unknown PFN return: " << type << std::endl;
        return PFNReturnCategory::OTHER;
    }
}

bool getLastTwo(MemberContext &ctx, std::pair<VariableArray::reverse_iterator, VariableArray::reverse_iterator> &data) { // rename ?
   VariableArray::reverse_iterator last = ctx.mdata.params.rbegin();
    if (last != ctx.mdata.params.rend()) {
        VariableArray::reverse_iterator prevlast = std::next(last);
        data.first = last;
        data.second = prevlast;
        return true;
    }
    return false;
}

bool isPointerToCArray(const std::string &id ) {
    if (id.size() >= 2) {
        if (id[0] == 'p' && std::isupper(id[1])) {
            return true;
        }
    }
    return false;
}

std::string stripStartingP(const std::string &str) {
    std::string out = str;
    if (isPointerToCArray(str)) {
        out = std::tolower(str[1]);
        out += str.substr(2);
    }
    return out;
}

MemberResolver* createMemberResolver(MemberContext &ctx) {

    if (ctx.mdata.params.empty()) {
        std::cerr << " >>> Unhandled: no params" << ctx.mdata.name << ENDL;
        return nullptr;
    }

    VariableArray::reverse_iterator last = ctx.mdata. params.rbegin(); // last argument
    MemberNameCategory nameCategory = evalMemberNameCategory(ctx);
    std::cout << "MemberNameCategory: " << (int)nameCategory << ENDL;
    MemberResolver *resolver = nullptr;

    if (ctx.pfnReturn == PFNReturnCategory::VK_OBJECT || ctx.pfnReturn == PFNReturnCategory::OTHER) {
        resolver = new MemberResolver(ctx);
        resolver->dbgtag = "PFN return";
        return resolver;
    }

    switch (nameCategory) {
        case MemberNameCategory::GET: {
                std::pair<VariableArray::reverse_iterator, VariableArray::reverse_iterator> data;
                if (getLastTwo(ctx, data)) {

                    if (isPointerToCArray(data.first->get()->identifier()) && data.second->get()->identifier() == stripStartingP(data.first->get()->identifier()) + "Size") { // refactor?

                        MemberResolver *res2 = new MemberResolverReturnProxy(ctx);
                        res2->generate();
                        delete res2;

                        resolver = new MemberResolverReturnVectorOfProxy(ctx);
                        return resolver;
                    }
                    else if (data.second->get()->identifier() == data.first->get()->identifier() + "Size") {
                        if (isTypePointer(*data.second->get())) {
                            resolver = new MemberResolverReturnVectorData(ctx);
                        }
                        else {
                            resolver = new MemberResolverReturnProxy(ctx);
                        }
                    }

                }

                if (!resolver) { // fallback
                    //std::cerr << "Get resolver on this branch 2" << ENDL;
                     if (strContains(last->get()->suffix(), "*") && !strContains(last->get()->prefix(), "const")) {
                        resolver = new MemberResolverStruct(ctx);
                    }
                    else {
                        resolver = new MemberResolver(ctx);
                    }
                }
                resolver->dbgtag = "get";
            }
            break;
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
            std::cout << ">> HERE" << std::endl;
            if (strContains(last->get()->suffix(), "*") && !strContains(last->get()->prefix(), "const")) {
                resolver = new MemberResolverStruct(ctx);
            }
            else {
                resolver = new MemberResolver(ctx);
            }
            break;
    }
    return resolver;
}

void generateClassMemberCpp(MemberContext &ctx) {

    std::string dbgProtoComment; // debug info

    std::cout << "Gen member: " << ctx.protoName << ENDL;

    MemberResolver *resolver = createMemberResolver(ctx);

    if (!resolver) {
        return;
    }

    resolver->generate();
    delete resolver;
}

void generateClassUniversal(const std::string &className,
                            const std::string &handle,
                            std::vector<ClassMemberData> &members,
                            const std::string &sourceFile = {},
                            const std::vector<std::string> memberBlacklist = {})
{
    // extract member data from XMLElements
    std::string memberProcAddr = "vkGet" + className + "ProcAddr";
    std::string memberCreate = "vkCreate" + className;

    file.writeLine("class " + className);
    file.writeLine("{");

    file.writeLine("protected:");
    file.pushIndent();
    file.writeLine("Vk" + className + " " + handle + ";");
    file.writeLine("uint32_t _version;");
    // PFN function pointers
    for (const ClassMemberData &m : members) {
        writeWithProtect(m.name, [&]{
            file.writeLine("PFN_" + m.name + " " + "m_" + m.name + ";");
        });
    }

    file.popIndent();
    file.writeLine("public:");
    file.pushIndent();
    // getProcAddr member
    file.writeLine("template<typename T>");
    file.writeLine("inline T getProcAddr(const std::string_view &name) const");
    file.writeLine("{");
    file.pushIndent();
    file.writeLine("return reinterpret_cast<T>(m_" + memberProcAddr +"(" + handle + ", name.data()));");
    file.popIndent();
    file.writeLine("}");
    // operators
    file.writeLine("operator " "Vk" + className + "() const {");
    file.pushIndent();
    file.writeLine("return " + handle + ";");
    file.popIndent();
    file.writeLine("}");
    // wrapper functions
    for (ClassMemberData &m : members) {
        if (m.name == memberProcAddr || m.name == memberCreate || isInContainter(memberBlacklist, m.name)) {
            continue;
        }

        // debug
        static const std::vector<std::string> debugNames;

        if (!debugNames.empty()) {
            bool contains = false;
            for (const auto &s : debugNames) {
                if (strContains(m.name, s)) {
                    //std::cout << "Pass: " << m.name << ENDL;
                    contains = true;
                    break;
                }
            }
            if (!contains) {
                continue;
            }
        }

        writeWithProtect(m.name, [&]{

            MemberContext ctx {.className = className,
                               .handle = handle,
                               .protoName = toCppStyle(m.name), // prototype name (without vk)
                               .pfnReturn = evaluatePFNReturn(m.type),
                               .mdata = m                               
                               };


            file.writeLine("");
            generateClassMemberCStyle(className, handle, "_" + ctx.protoName, m);

            generateClassMemberCpp(ctx);

        });
    }

    file.popIndent();

    file.get() << ENDL;

    file.pushIndent();
    file.writeLine("void loadTable()");
    file.writeLine("{");
    file.pushIndent();
    // function pointers initialization
    for (const ClassMemberData &m : members) {
        if (m.name == memberProcAddr || m.name == memberCreate) {
            continue;
        }
        writeWithProtect(m.name, [&]{
            file.writeLine("m_" + m.name + " = getProcAddr<" + "PFN_" + m.name + ">(\"" + m.name + "\");");
        });
    }
    file.popIndent();
    file.writeLine("}");

    if (!sourceFile.empty()) {
        file.popIndent();
        generateReadFromFile(sourceFile);
        file.pushIndent();
    }

    file.popIndent();
    file.writeLine("};");
    file.get() << ENDL;
}

void genInstanceClass(std::vector<ClassMemberData> &commands) {
    generateClassUniversal("Instance", "_instance", commands, sourceDir + "/source_instance.hpp", {"vkEnumeratePhysicalDevices"});
}

void genDeviceClass(std::vector<ClassMemberData> &commands) {
    generateClassUniversal("Device", "_device", commands, sourceDir + "/source_device.hpp");
}

void parseCommands(XMLNode *node) {
    std::cout << "Parsing commands" << ENDL;

    //static constexpr std::array<std::string_view, 3> deviceObjects = {"VkDevice", "VkQueue", "VkCommandBuffer"};
    static const std::vector<std::string> deviceObjects = {"VkDevice", "VkQueue", "VkCommandBuffer"};

    // command data is stored in XMLElement*
    std::vector<ClassMemberData> elementsDevice;
    std::vector<ClassMemberData> elementsInstance;
    std::vector<ClassMemberData> elementsOther;

    // iterate contents of <commands>, filter only <command> children
    for (XMLElement *commandElement : Elements(node) | ValueFilter("command")) {

        // default destination is elementsOther
        std::vector<ClassMemberData> *target = &elementsOther;

        ClassMemberData command = parseClassMember(commandElement);

        if (command.params.size() > 0) {
            std::string first = command.params.at(0)->original.type(); // first argument of a command
            if (isInContainter(deviceObjects, first) ||
                    command.name == "vkCreateDevice")
            { // command is for device
                target = &elementsDevice;
            }
            else if (first == "VkInstance" ||
                     command.name == "vkCreateInstance")
            { // command is for instance
                target = &elementsInstance;
            }
        }

        target->push_back(command);
    }

    genInstanceClass(elementsInstance);
    genDeviceClass(elementsDevice);

    std::cout << "Parsing commands done" << ENDL;
}
