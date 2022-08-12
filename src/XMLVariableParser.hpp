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
#ifndef XMLVARIABLEPARSER_H
#define XMLVARIABLEPARSER_H

#include "tinyxml2.h"
#include <array>
#include <unordered_map>
#include <string>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "StringUtils.hpp"

class Generator;

enum class Namespace {
    NONE,
    VK,
    RAII,
    STD
};

enum State { // State used for indexing and FSM
    PREFIX = 0,
    TYPE = 1,
    SUFFIX = 2,
    IDENTIFIER = 3,
    ARRAY_LENGTH,
    BRACKET_LEFT,
    DONE
};

struct VariableFields : public std::array<std::string, 4> {

    void set(size_t index, const std::string &str);

    const std::string &get(size_t index) const;

    // prefix getter
    const std::string &prefix() const { return get(PREFIX); }
    // suffix getter
    const std::string &suffix() const { return get(SUFFIX); }
    // type getter
    const std::string &type() const { return get(TYPE); }
    // name getter
    const std::string &identifier() const { return get(IDENTIFIER); }

    void setType(const std::string &type) { set(TYPE, type); }

    void setIdentifier(const std::string &identifier) {
        set(IDENTIFIER, identifier);
    }

    void setFullType(const std::string &prefix, const std::string &type,
                     const std::string &suffix) {
        set(PREFIX, prefix);
        set(TYPE, type);
        set(SUFFIX, suffix);
    }

    bool isPointer() const {
        std::string s = get(SUFFIX);
        return s.find("*") != std::string::npos;
    }

    bool isConst() const {
        std::string s = get(PREFIX);
        return s.find("const") != std::string::npos;
    }

};

// holds variable information broken into 4 sections
struct VariableData : public VariableFields {

  public:
    enum Type {
        TYPE_INVALID,
        TYPE_DEFAULT,
        TYPE_RETURN,
        TYPE_REFERENCE,
        TYPE_ARRAY_PROXY,
        TYPE_VECTOR,
        TYPE_OPTIONAL
    };

    enum class Flags : int {
        NONE = 0,
        HANDLE = 1,
        ARRAY = 2,
        ARRAY_IN = 4,
        ARRAY_OUT = 8
    };

    VariableFields original;

    VariableData(const Generator &gen, Type type = TYPE_DEFAULT);

    VariableData(const Generator &gen, const String &object);

    VariableData(const Generator &gen, const String &object, const std::string &id);

//    VariableData(const VariableData &o) {
//        gen = o.gen;
//        *this = o;
//    }

    void setAltPFN(const std::string &str) { altPFN = str; }

    void setSpecialType(Type type) { specialType = type; }

    Type getSpecialType() { return specialType; }

    std::string getLenAttrib() const { return lenAttribStr; }

    std::string getLenAttribIdentifier() const;

    std::string getLenAttribRhs() const;

    std::string namespaceString() const;

    bool isLenAttribIndirect() const;

    // arrayLength flag getter
    bool hasArrayLength() const { return arrayLengthFound; }
    // arrayLength getter
    const std::string &arrayLength() const { return arrayLengthStr; }

    bool isDefault() const { return specialType == TYPE_DEFAULT; }

    void setIgnoreFlag(bool value) { ignoreFlag = value; }

    bool getIgnoreFlag() const { return ignoreFlag; }

    void setNamespace(Namespace);

    void toRAII();

    Namespace getNamespace() const;

    void setIgnorePFN(bool value) { ignorePFN = value; }

    bool getIgnorePFN() const { return ignorePFN; }

    bool isInvalid() const { return specialType == TYPE_INVALID; }

    bool isReturn() const { return specialType == TYPE_RETURN; }

    void convertToCpp(const Generator &gen);

    bool isNullTerminated() const;

    void convertToArrayProxy();

    void bindLengthVar(const std::shared_ptr<VariableData> &var);

    void bindArrayVar(const std::shared_ptr<VariableData> &var);

    bool hasLengthVar() const { return lenghtVar.get() != nullptr; }

    bool hasArrayVar() const { return arrayVar.get() != nullptr; }

    const std::shared_ptr<VariableData>& getLengthVar() const;

    const std::shared_ptr<VariableData>& getArrayVar() const;

    // void convertToTemplate();

    void convertToReturn();

    void convertToReference();

    void convertToPointer();

    void convertToOptional();

    void convertToStdVector();

    bool removeLastAsterisk();

    void setConst(bool enabled);

    Flags getFlags() const;

    bool isHandle() const;
    bool isArray() const;
    bool isArrayIn() const;
    bool isArrayOut() const;

    std::string toArgument(bool useOriginal = false) const;

    // full type getter
    std::string fullType() const;

    // full type getter
    std::string originalFullType() const {
        return original.get(PREFIX) + original.get(TYPE) + original.get(SUFFIX);
    }

    // getter for all fields combined
    std::string toString() const;

    std::string declaration() const;

    std::string toStringWithAssignment() const;

    // getter for all fields combined for C version
    std::string originalToString() const;

    void setAssignment(const std::string &str) { _assignment = str; }

    void setReferenceFlag(bool enabled) {
        optionalAmp = enabled? "&" : "";
    }

    std::string assignment() const { return _assignment; }

    void setTemplate(const std::string &str);

    std::string getTemplate() const;

    // static void updateNamespace(Namespace key, const std::string &value);

  protected:
    const Generator &gen;
    std::string altPFN;
    std::string optionalAmp;
    Type specialType;
    Flags flags;
    Namespace ns;
    bool ignoreFlag;
    bool ignorePFN;
    bool arrayLengthFound;
    bool nullTerminated;
    std::string arrayLengthStr;
    std::string lenAttribStr;
    std::string _assignment;
    // std::string optionalNamespace;
    std::string optionalTemplate;

    std::shared_ptr<VariableData> lenghtVar;
    std::shared_ptr<VariableData> arrayVar;

    // static std::unordered_map<Namespace, std::string> namespaces;

    void evalFlags(const Generator &gen);

    std::string optionalArraySuffix() const;

//    std::string
//    toArgumentSizeAndArrayProxy() const { // in reality it's two arguments
//        return get(IDENTIFIER) + ".size()" + ", " + toArgumentArrayProxy();
//    }

    std::string toArgumentArrayProxy() const;

    std::string createCast(std::string from) const;

    //    std::string toArgumentTemplateWithSize() const {
    //        return "sizeof(" + get(TYPE) + "), " + toArgumentDefault();
    //    }

    std::string toArgumentDefault(bool useOriginal = false) const;

    std::string identifierAsArgument() const;
};

// parses XMLElement's contents into variable data
/*  <member>     <type>   </type>    <name>    </name></member>
 *          ^prefix    ^type     ^suffix    ^identifier
 */
class XMLVariableParser : public VariableData, protected tinyxml2::XMLVisitor {

    State state{PREFIX}; // FSM state

  public:
    // XMLVariableParser() = default;

    XMLVariableParser(tinyxml2::XMLElement *element, const Generator &gen);

    // Entry point, reset state to initial, parse XMLElement and trim
    void parse(tinyxml2::XMLElement *element, const Generator &gen);

    // next Visit call appends according to state until set to DONE
    virtual bool Visit(const tinyxml2::XMLText &text) override;

    // leaves one space in suffix
    void trim();
};

inline VariableData::Flags operator|(const VariableData::Flags &a,
                                         const VariableData::Flags &b) {
    return static_cast<VariableData::Flags>(static_cast<int>(a) |
                                                static_cast<int>(b));
}

inline VariableData::Flags operator&(const VariableData::Flags &a,
                                         const VariableData::Flags &b) {
    return static_cast<VariableData::Flags>(static_cast<int>(a) &
                                                static_cast<int>(b));
}

inline VariableData::Flags &operator|=(VariableData::Flags &a,
                                           const VariableData::Flags &b) {
    return a = a | b;
}

inline VariableData::Flags &operator&=(VariableData::Flags &a,
                                           const VariableData::Flags &b) {
    return a = a & b;
}

inline bool operator==(const VariableData::Flags &a,
                       const VariableData::Flags &b) {
    return static_cast<int>(a) == static_cast<int>(b);
}

inline bool operator!=(const VariableData::Flags &a,
                       const VariableData::Flags &b) {
    return static_cast<int>(a) != static_cast<int>(b);
}

inline bool hasFlag(const VariableData::Flags &a,
                    const VariableData::Flags &b) {
    return static_cast<int>(a) & static_cast<int>(b);
}

class XMLDefineParser : protected tinyxml2::XMLVisitor {
    enum State { // State used for parsing
        DEFINE = 0,
        NAME = 1,
        VALUE = 2,
        DONE
    };


    State state; // FSM state

  public:
    std::string name;
    std::string value;

    XMLDefineParser(tinyxml2::XMLElement *element, const Generator &gen);

    // Entry point, reset state to initial, parse XMLElement and trim
    void parse(tinyxml2::XMLElement *element, const Generator &gen);

    void trim();

    virtual bool Visit(const tinyxml2::XMLText &text) override;
};

#endif // XMLVARIABLEPARSER_H
