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

static const std::string NAMESPACE {"vk20"};
static const std::string FILEPROTECT {"VULKAN_20_HPP"};

static FileHandle file; //main output file
static std::string sourceDir; //additional source files directory

static std::unordered_set<std::string> structNames; //list of names from <types>
static std::unordered_set<std::string> tags; //list of tags from <tags>

static std::map<std::string, std::string> platforms; //maps platform name to protect (#if defined PROTECT)
static std::map<std::string, std::string*> extensions; //maps extension to protect as reference

//main parse functions
static void parsePlatforms(XMLNode*);
static void parseExtensions(XMLNode*);
static void parseTags(XMLNode*);
static void parseTypes(XMLNode*);
static void parseCommands(XMLNode*);

//specifies order of parsing vk.xml refistry,
static const std::vector<std::pair<std::string, std::function<void(XMLNode*)>>> rootParseOrder {
    {"platforms", parsePlatforms},
    {"extensions", parseExtensions},
    {"tags", parseTags},
    {"types", parseTypes},
    {"commands", parseCommands}
};

//holds information about class member (function)
struct ClassMemberData {
    std::string name; //identifier
    std::string type; //return type
    std::vector<VariableData> params; //list of arguments

    //creates function arguments signature
    std::string createProtoArguments(const std::string &className) const {
        std::string out;
        for (size_t i = 0; i < params.size(); ++i) {
            if (params[i].type() == "Vk" + className) {
                continue;
            }
            out += params[i].proto();
            if (i != params.size() - 1) {
                out += ", ";
            }
        }
        return out;
    }

    //arguments for calling vulkan API functions
    std::string createPFNArguments(const std::string &className, const std::string &handle) const {
        std::string out;
        for (size_t i = 0; i < params.size(); ++i) {
            if (params[i].type() == "Vk" + className) {
                out += handle;
            }
            else {
                out += params[i].identifier();
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

static bool caseInsensitivePredicate(char a, char b) {
  return std::toupper(a) == std::toupper(b);
}

//case insensitve search for substring
static bool strContains(const std::string &string, const std::string &substring) {
    auto it = std::search(string.begin(), string.end(),
                          substring.begin(), substring.end(),
                          caseInsensitivePredicate);
    return it != string.end();
}

//tries to match str in extensions, if found returns pointer to protect, otherwise nullptr
static std::string* findExtensionProtect(const std::string &str) {
    auto it = extensions.find(str);
    if (it != extensions.end()) {
        return it->second;
    }
    return nullptr;
}

//#if defined encapsulation
static void writeWithProtect(const std::string &name, std::function<void()> function) {
    const std::string* protect = findExtensionProtect(name);
    if (protect) {
        file.get() << "#if defined(" << *protect << ")" << ENDL;
    }

    function();

    if (protect) {
        file.get() << "#endif //" << *protect << ENDL;
    }
    //file.get() << ENDL;
}

static void strStripPrefix(std::string &str, const std::string_view &prefix) {
    if (prefix.size() > str.size()) {
        return;
    }
    if (str.substr(0, prefix.size()) == prefix) {
        str.erase(0, prefix.size());
    }
}

static void strStripSuffix(std::string &str, const std::string_view &suffix) {
    if (suffix.size() > str.size()) {
        return;
    }
    if (str.substr(str.size() - suffix.size(), std::string::npos) == suffix) {
        str.erase(str.size() - suffix.size(), std::string::npos);
    }
}

static void strStripVk(std::string &str) {
    strStripPrefix(str, "Vk");
}

static std::string strStripVk(const std::string &str) {
    std::string out = str;
    strStripVk(out);
    return out;
}


static std::string camelToSnake(const std::string &str) {
    std::string out;
    for (char c : str) {
        if (std::isupper(c) && !out.empty()) {
            out += '_';
        }        
        out += std::toupper(c);
    }
    return out;
}

static std::string convertSnakeToCamel(const std::string &str) {
    std::string out;
    bool flag = false;
    for (char c : str) {
        if (c == '_') {
            flag = true;
            continue;
        }
        out += flag? std::toupper(c) : std::tolower(c);
        flag = false;
    }
    return out;
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
    return suffix;
}

static std::pair<std::string, std::string> snakeToCamelPair(std::string str) {
    std::string suffix = strRemoveTag(str);
    std::string out = convertSnakeToCamel(str);

    //'bit' to 'Bit' workaround
    size_t pos = out.find("bit");
    if (pos != std::string::npos &&
        pos > 0 &&
        std::isdigit(out[pos - 1]))
    {
            out[pos] = 'B';        
    }

    return std::make_pair(out, suffix);
}

static std::string snakeToCamel(const std::string &str) {
    const auto &p = snakeToCamelPair(str);
    return p.first + p.second;
}

static std::string enumConvertCamel(const std::string &enumName, std::string value) {
    strStripPrefix(value, "VK_" + camelToSnake(enumName));
    return "e" + snakeToCamel(value);
}

static void parseXML(XMLElement *root) {

    //maps all root XMLNodes to their tag identifier
    std::map<std::string, XMLNode*> rootTable;
    for (XMLNode &node : Nodes(root)) {
        rootTable.emplace(node.Value(), &node);
    }

    //call each function in rootParseOrder with corresponding XMLNode
    for (auto &key : rootParseOrder) {
        auto it = rootTable.find(key.first);//find tag id
        if (it != rootTable.end()) {
            key.second(it->second); //call function(node*)
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
        //help option block
        if (helpOption.set) {
            std::cout << HELP_TEXT;
            return 0;
        }
        //argument check
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
    //iterate contents of <type>, filter only <member> children
    for (XMLElement *member : Elements(node) | ValueFilter("member")) {
            std::string out;

            XMLVariableParser parser{member}; //parse <member>

            std::string type = strStripVk(parser.type());
            std::string name = parser.identifier();

            out += parser.prefix();
            if (isStruct(type)) {
                out += NAMESPACE + "::";
            }
            out += type;
            out += parser.suffix();
            out += name;
            if (parser.hasArrayLength()) {
                out += "[" + parser.arrayLength() + "]";
            }

            if (const char *values = member->ToElement()->Attribute("values")) {
                std::string value = enumConvertCamel(type, values);
                out += (" = " + type + "::" + value);
                if (name == "sType") { //save sType information for structType
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

        std::string structType{}, structTypeValue{}; //placeholders
        std::vector<std::string> members = parseStructMembers(node, structType, structTypeValue);

        file.writeLine("struct " + name);
        file.writeLine("{");

        file.pushIndent();
        if (!structType.empty() && !structTypeValue.empty()) { //structType
            file.writeLine("static VULKAN_HPP_CONST_OR_CONSTEXPR " + structType + " structureType = "
                           + structType + "::" + structTypeValue + ";");
            file.get() << ENDL;
        }
        //structure members
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
    //iterate contents of <platforms>, filter only <platform> children
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
    //iterate contents of <extensions>, filter only <extension> children
    for (XMLElement *extension : Elements(node) | ValueFilter("extension")) {
        const char *platform = extension->Attribute("platform");
        if (platform) {
            auto it = platforms.find(platform);
            if (it != platforms.end()) {
                //iterate contents of <extension>, filter only <require> children
                for (XMLElement *require : Elements(extension) | ValueFilter("require")) {
                    //iterate contents of <require>
                    for (XMLElement &entry : Elements(require)) {
                        const char *name = entry.Attribute("name");
                        if (name) { //pair extension name with platform protect
                            extensions.emplace(name, &it->second);
                        }
                    }
                }
            }
        }
    }
    std::cout << "Parsing extensions done" << ENDL;
}

void parseTags(XMLNode *node) {
    std::cout << "Parsing tags" << ENDL;
    //iterate contents of <tags>, filter only <tag> children
    for (XMLElement *tag : Elements(node) | ValueFilter("tag")) {
        const char *name = tag->Attribute("name");
        if (name) {
            tags.emplace(name);
        }
    }
    std::cout << "Parsing tags done" << ENDL;
}

void parseTypes(XMLNode *node) {
    std::cout << "Parsing types" << std::endl;
    //iterate contents of <types>, filter only <type> children
    for (XMLElement *type : Elements(node) | ValueFilter("type")) {
        const char *cat = type->Attribute("category");
        const char *name = type->Attribute("name");
        if (cat && name) {
            if (strcmp(cat, "struct") == 0) {
                parseStruct(type, name);
            }
        }
    }
    std::cout << "Parsing types done" << ENDL;
}

ClassMemberData parseClassMember(XMLElement *command) {
    ClassMemberData m;
    //iterate contents of <command>
    for (XMLElement &child : Elements(command) ) {
        //<proto> section
        if (std::string_view(child.Value()) == "proto") {
            //get <name> field in proto
            XMLElement *nameElement = child.FirstChildElement("name");
            if (nameElement) {
                m.name = nameElement->GetText();
            }
            //get <type> field in proto
            XMLElement *typeElement = child.FirstChildElement("type");
            if (typeElement) {
                m.type = typeElement->GetText();
            }
        }
        //<param> section
        else if (std::string_view(child.Value()) == "param") {
            //parse inside of param
            XMLVariableParser parser {&child};
            //add proto data to list
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

static const std::string debugName = ""; // debug
//allocateCommandBuffers
//getPipelineCacheData

std::string generateClassMemberCStyle(const std::string &className, const std::string &handle, std::string &protoName, ClassMemberData& m) {

    std::string protoArgs = m.createProtoArguments(className);
    std::string innerArgs = m.createPFNArguments(className, handle);
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

enum class MEMBER_RETURN_CATEGORY {
    VOID,
    RESULT,
    STRUCT,        
    ARRAY_IN,
    ARRAY_OUT,
    ARRAY_OUT_ALLOC
};

std::string toString(MEMBER_RETURN_CATEGORY category) {
    switch (category) {
        case MEMBER_RETURN_CATEGORY::VOID:            return "void";
        case MEMBER_RETURN_CATEGORY::RESULT:          return "result";
        case MEMBER_RETURN_CATEGORY::STRUCT:          return "struct";
        case MEMBER_RETURN_CATEGORY::ARRAY_IN:        return "array_input";
        case MEMBER_RETURN_CATEGORY::ARRAY_OUT:       return "array_output";
        case MEMBER_RETURN_CATEGORY::ARRAY_OUT_ALLOC: return "array_alloc";
    }
    return "";
}

enum class ARRAY_SIZE_ARGUMENT {
    INVALID,
    COUNT,
    SIZE
};

ARRAY_SIZE_ARGUMENT evalArraySizeArgument(const VariableData &m) {
    if (strContains(m.identifier(), "count")  &&
        strContains(m.type(), "int"))
    {
        return ARRAY_SIZE_ARGUMENT::COUNT;
    }
    if (strContains(m.identifier(), "size")  &&
        strContains(m.type(), "size_t"))
        //strContains(m.suffix(), "*"))
    {
        return ARRAY_SIZE_ARGUMENT::SIZE;
    }
    return ARRAY_SIZE_ARGUMENT::INVALID;
}
void evalMemberReturnCategory(const std::string& name, const VariableData& last, MEMBER_RETURN_CATEGORY& category) {
    static constexpr char const* keywordAllocate = "allocate";

    if (strContains(name, "create") ||
        (strContains(name, "get") && strContains(last.suffix(), "*"))) {
        category = MEMBER_RETURN_CATEGORY::STRUCT;
    }
    else if (name.starts_with(keywordAllocate)) {
        std::string allocName = name.substr(sizeof(keywordAllocate));
        strRemoveTag(allocName);
        if (allocName.ends_with("s")) {
            category = MEMBER_RETURN_CATEGORY::ARRAY_OUT_ALLOC;
        }
        else {
            category = MEMBER_RETURN_CATEGORY::STRUCT;
        }
    }
}

class ArgumentList : public std::vector<std::string> {
public:
    void add(const std::string &str) {
        push_back(str);
    }

    std::string string() {
        std::string out;
        if (empty()) {
            return "";
        }
        std::vector<std::string>::iterator it;
        for (it = begin(); it != std::prev(end()); ++it) {
            out += *it;
            out += ", ";
        }
        out += *it;
        return out;
    }
};

void generateClassMemberCpp(const std::string &className, const std::string &handle, std::string &protoName, ClassMemberData& m, const std::string &signatureC) {

    std::string dbgProtoComment;

    if (m.params.empty()) {
        std::cerr << "Unhandled: no params" << m.name << ENDL;
        return;
    }
    bool hasResult = m.type == "VkResult";

    VariableData last = *m.params.rbegin();
    std::string returnType = m.type;
    MEMBER_RETURN_CATEGORY returnCat = MEMBER_RETURN_CATEGORY::VOID;

    if (m.type == "VkResult") {
        returnCat = MEMBER_RETURN_CATEGORY::RESULT;
    }

    bool hasTemplate = false;

    std::pair<VariableData, VariableData> paramVector;
    VariableData allocInfo;
    ArgumentList vectorCall, call;
    bool addedLast = false;

    evalMemberReturnCategory(protoName, last, returnCat);

    std::vector<VariableData>::iterator it = m.params.begin();

    const auto specialParseMemeberArguments = [&]() {        

        ARRAY_SIZE_ARGUMENT arrayArg = evalArraySizeArgument(*it);
        if (arrayArg == ARRAY_SIZE_ARGUMENT::INVALID) {
            return false;
        }

        const VariableData nextObj = *std::next(it);

        if (!strContains(nextObj.suffix(), "*") || (arrayArg == ARRAY_SIZE_ARGUMENT::SIZE && nextObj.type() != "void")) {
            return false;
        }

        paramVector.first = *it;
        paramVector.second = nextObj;

        if (strContains(nextObj.prefix(), "const") || nextObj.type() == "void") {
            returnCat = MEMBER_RETURN_CATEGORY::ARRAY_IN;
            it = m.params.erase(it); //erase one element at it

            if (arrayArg == ARRAY_SIZE_ARGUMENT::SIZE) {
                it->setType("", "ArrayProxy<T>", " &");
                hasTemplate = true;
            }
            else {
                it->setType("", "ArrayProxy<const " + it->type() + ">", " const &");
            }
            call.add(it->identifier() + ".size()");
            call.add(it->identifier() + ".data()");
            it++;
        }
        else {
            returnCat = MEMBER_RETURN_CATEGORY::ARRAY_OUT;

            vectorCall.add("&" + it->identifier());
            vectorCall.add("nullptr");

            call.add("&" + it->identifier());
            call.add("reinterpret_cast<" + last.type() + "*>(" + nextObj.identifier() + ".data())");

            it = m.params.erase(m.params.erase(it)); // erase two elements at it
        }
        return true;
    };

    std::vector<VariableData>::iterator it_last = std::prev(m.params.end());
    while (it != m.params.end() && it != it_last) {

        if (returnCat == MEMBER_RETURN_CATEGORY::ARRAY_OUT_ALLOC) {
            if (it->type() == last.type() + "AllocateInfo") {
                allocInfo = *it;
            }
        }

        if (!specialParseMemeberArguments()) {
            if (it->type() == "Vk" + className) {
                vectorCall.add(handle);
                call.add(handle);
            }
            else {
                vectorCall.add(it->identifier());
                call.add(it->identifier());
            }
            it++;
        }
        if (it == m.params.end()) {
            addedLast = true;
        }
    }

    if (returnCat == MEMBER_RETURN_CATEGORY::ARRAY_OUT_ALLOC) {
        std::cout << "Array alloc pop: " << m.params.rbegin()->identifier() << ENDL;
        m.params.pop_back();
        addedLast = true;
    }
    else if (returnCat == MEMBER_RETURN_CATEGORY::STRUCT) {
        m.params.pop_back();
        addedLast = true;

    }
    if (last.type() == "Vk" + className) {
        returnCat = MEMBER_RETURN_CATEGORY::VOID;
        std::cout << m.name << " returns class" << ENDL;
    }

    strStripVk(returnType);

    if (returnCat == MEMBER_RETURN_CATEGORY::ARRAY_OUT)
    {
        returnType = "std::vector<" + last.type() + ">";
    }    
    else if (returnCat == MEMBER_RETURN_CATEGORY::STRUCT) {
        call.add("reinterpret_cast<" + last.type() + "*>(&" + last.identifier() +")");
        returnType = last.type();
        addedLast = true;
    }
    else if (returnCat == MEMBER_RETURN_CATEGORY::ARRAY_OUT_ALLOC) {
        call.add("reinterpret_cast<" + last.type() + "*>(" + last.identifier() + ".data())");
        returnType = "std::vector<" + last.type() + ">";
    }
    else if (returnCat == MEMBER_RETURN_CATEGORY::ARRAY_IN) {
        if (hasResult) {
            returnType = "VkResult";
            returnCat = MEMBER_RETURN_CATEGORY::RESULT;
        }
        else {
            returnType = "void";
            returnCat = MEMBER_RETURN_CATEGORY::VOID;
        }
    }

    if (!addedLast) {
        call.add(last.identifier());
    }

    std::string protoArgs = m.createProtoArguments(className);
    std::string proto = "inline " + returnType + " " + protoName + "(" + protoArgs + ") {"
                        + " // [" + toString(returnCat) + dbgProtoComment + "]";

    if (signatureC == protoArgs) {
        file.writeLine("// overload of " + proto);
        return;
    }

   if (hasTemplate) {
       file.writeLine("template <typename T>");
   }
   file.writeLine(proto);
   file.pushIndent();

   if (hasResult && returnCat != MEMBER_RETURN_CATEGORY::RESULT) {
       file.writeLine("VkResult r;");
   }
   bool flagNestIf = false;

   if (returnCat == MEMBER_RETURN_CATEGORY::STRUCT) {
       file.writeLine(returnType + " " + last.identifier() + "; // to &");
   }
   else if (returnCat == MEMBER_RETURN_CATEGORY::ARRAY_OUT) {
       file.writeLine(returnType + " " + last.identifier() + ";");
       file.writeLine(paramVector.first.type() + " " + paramVector.first.identifier() + ";");
       if (hasResult) {
           file.writeLine("r = m_" + m.name + "(" + vectorCall.string() + ");");
           file.writeLine("if (r == VK_SUCCESS) {");
           file.pushIndent();
           flagNestIf = true;
       }
       else {
           file.writeLine("m_" + m.name + "(" + vectorCall.string() + ");");
       }
       file.writeLine(last.identifier() + ".resize(" + paramVector.first.identifier() + ");");
   }
   else if (returnCat == MEMBER_RETURN_CATEGORY::ARRAY_OUT_ALLOC) {
       file.writeLine(returnType + " " + last.identifier() + ";");
       std::string allocStructMember = last.type() + "Count";
       // refactor
       strStripVk(allocStructMember);
       if (allocStructMember.size() > 0) { //convert first letter to lowercase
           allocStructMember[0] = std::tolower(allocStructMember[0]);
       }

       file.writeLine(last.identifier() + ".resize(" + allocInfo.identifier() + "->" + allocStructMember + ");");
   }

   if (returnCat == MEMBER_RETURN_CATEGORY::RESULT) {
       file.writeLine("return m_" + m.name + "(" + call.string() + ");");
       returnCat = MEMBER_RETURN_CATEGORY::VOID;
   }
   else if (hasResult) {
       file.writeLine("r = m_" + m.name + "(" + call.string() + ");");
   }
   else {
       file.writeLine("m_" + m.name + "(" + call.string() + ");");
   }

   if (flagNestIf) {
       file.popIndent();
       file.writeLine("}");
   }
   if (returnCat == MEMBER_RETURN_CATEGORY::RESULT) {
       file.writeLine("return r;");
   }
   else if (returnCat != MEMBER_RETURN_CATEGORY::VOID) {
       if (hasResult) {
           std::string message = "\"" + className + "::" + protoName + "\"";
           file.writeLine("return vk::createResultValue<" + returnType + ">(static_cast<Result>(r), " + last.identifier() + ", " + message + ");");
       }
       else {
           file.writeLine("return " + last.identifier() + ";");
       }
   }
   file.popIndent();
   file.writeLine("}");
}

void generateClassUniversal(const std::string &className,
                            const std::string &handle,
                            std::vector<ClassMemberData> &members,
                            const std::string &sourceFile = {},
                            const std::vector<std::string> memberBlacklist = {})
{
    //extract member data from XMLElements
    std::string memberProcAddr = "vkGet" + className + "ProcAddr";
    std::string memberCreate = "vkCreate" + className;

    file.writeLine("class " + className);
    file.writeLine("{");

    file.writeLine("protected:");
    file.pushIndent();
    file.writeLine("Vk" + className + " " + handle + ";");
    file.writeLine("uint32_t _version;");
    //PFN function pointers
    for (const ClassMemberData &m : members) {
        writeWithProtect(m.name, [&]{
            file.writeLine("PFN_" + m.name + " " + "m_" + m.name + ";");
        });
    }

    file.popIndent();
    file.writeLine("public:");
    file.pushIndent();
    //getProcAddr member
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
    //wrapper functions
    for (ClassMemberData &m : members) {
        if (m.name == memberProcAddr || m.name == memberCreate || isInContainter(memberBlacklist, m.name)) {
            continue;
        }

        //debug
        if (!debugName.empty() && !strContains(m.name, debugName)) continue;

        writeWithProtect(m.name, [&]{
            std::string protoName{m.name}; //prototype name (without vk)
            strStripPrefix(protoName, "vk");
            if (protoName.size() > 0) { //convert first letter to lowercase
                protoName[0] = std::tolower(protoName[0]);
            }         

            std::string signature = generateClassMemberCStyle(className, handle, protoName, m);
            generateClassMemberCpp(className, handle, protoName, m, signature);

        });
    }

    file.popIndent();

    file.get() << ENDL;

    file.pushIndent();
    file.writeLine("void loadTable()");
    file.writeLine("{");
    file.pushIndent();
    //function pointers initialization
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

    static const std::vector<std::string> deviceObjects = {"VkDevice", "VkQueue", "VkCommandBuffer"};

    //command data is stored in XMLElement*
    std::vector<ClassMemberData> elementsDevice;
    std::vector<ClassMemberData> elementsInstance;
    std::vector<ClassMemberData> elementsOther;

    //iterate contents of <commands>, filter only <command> children
    for (XMLElement *commandElement : Elements(node) | ValueFilter("command")) {

        //default destination is elementsOther
        std::vector<ClassMemberData> *target = &elementsOther;

        ClassMemberData command = parseClassMember(commandElement);

        if (command.params.size() > 0) {
            std::string first = command.params.at(0).type(); // first argument of a command
            if (isInContainter(deviceObjects, first) ||
                command.name == "vkCreateDevice")
            { //command is for device
                target = &elementsDevice;
            }
            else if (first == "VkInstance" ||
                     command.name == "vkCreateInstance")
            { //command is for instance
                target = &elementsInstance;
            }
        }

        target->push_back(command);
    }

    genInstanceClass(elementsInstance);
    genDeviceClass(elementsDevice);

    std::cout << "Parsing commands done" << ENDL;
}
