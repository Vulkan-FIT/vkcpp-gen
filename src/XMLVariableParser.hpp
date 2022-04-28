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
#include <string>

#include <iostream>
#include <memory>
#include <stdexcept>

#include "StringUtils.hpp"

enum State { // State used for indexing and parsing FSM
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

};

// holds variable information broken into 4 sections
struct VariableData : public VariableFields {

    std::string altPFN;
    std::string optionalAmp;
  public:
    enum Type {
        TYPE_INVALID,
        TYPE_DEFAULT,
        TYPE_RETURN,
        TYPE_ARRAY_PROXY,
        TYPE_SIZE_AND_ARRAY_PROXY,
        // TYPE_TEMPLATE,
        // TYPE_TEMPLATE_WITH_SIZE,
        TYPE_VECTOR
    };

    VariableFields original;

    VariableData();

    VariableData(Type type) {
        arrayLengthFound = false;
        specialType = type;
        if (specialType == TYPE_INVALID) {
            ignoreFlag = true;
        }
    }

    void setAltPFN(const std::string &str) { altPFN = str; }

    void setSpecialType(Type type) { specialType = type; }

    bool hasLenAttrib() const { return _hasLenAttrib; }

    std::string lenAttrib() { return _lenAttrib; }

    std::string lenAttribVarName();

    // arrayLength flag getter
    bool hasArrayLength() const { return arrayLengthFound; }
    // arrayLength getter
    const std::string &arrayLength() const { return arrayLengthStr; }

    bool isDefault() const { return specialType == TYPE_DEFAULT; }

    void setIgnoreFlag(bool value) { ignoreFlag = value; }

    bool getIgnoreFlag() const { return ignoreFlag; }

    void setIgnorePFN(bool value) { ignorePFN = value; }

    bool getIgnorePFN() const { return ignorePFN; }

    bool isInvalid() const { return specialType == TYPE_INVALID; }

    bool isReturn() const { return specialType == TYPE_RETURN; }

    void convertToCpp();

    void convertToArrayProxy(bool bindSizeArgument = true);

    void bindLengthAttrib(std::shared_ptr<VariableData> var) {
        //  std::cout << "Obj[" << this << "] " << "Bind to lenght attrib var: "
        //  << var->identifier() << std::endl;
        _lenAttribVar = var;
    }

    bool hasLengthAttribVar() { return _lenAttribVar.get() != nullptr; }

    std::shared_ptr<VariableData> lengthAttribVar();

    void convertToTemplate();

    void convertToReturn();

    void convertToStdVector();

    bool removeLastAsterisk();

    std::string toArgument() const;

    // full type getter
    std::string fullType() const;

    // full type getter
    std::string originalFullType() const {
        return original.get(PREFIX) + original.get(TYPE) + original.get(SUFFIX);
    }

    // getter for all fields combined
    std::string toString() const;

    std::string toStringWithAssignment() const;

    // getter for all fields combined for C version
    std::string originalToString() const;

    void setAssignment(const std::string &str) { _assignment = str; }

    void setReferenceFlag(bool enabled) {
        optionalAmp = enabled? "&" : "";
    }

    std::string assignment() const { return _assignment; }

  protected:
    Type specialType;
    bool ignoreFlag;
    bool arrayLengthFound;
    std::string arrayLengthStr;
    bool _hasLenAttrib;
    std::string _lenAttrib;
    std::shared_ptr<VariableData> _lenAttribVar;
    std::string _assignment;
    bool ignorePFN;

  private:
    std::string optionalArraySuffix() const;

    std::string
    toArgumentSizeAndArrayProxy() const { // in reality it's two arguments
        return get(IDENTIFIER) + ".size()" + ", " + toArgumentArrayProxy();
    }

    std::string toArgumentArrayProxy() const {
        return "std::bit_cast<" + originalFullType() + ">(" + get(IDENTIFIER) +
               ".data())";
    }

    std::string createCast(std::string from) const;


    //    std::string toArgumentTemplateWithSize() const {
    //        return "sizeof(" + get(TYPE) + "), " + toArgumentDefault();
    //    }

    std::string toArgumentDefault() const;

    std::string identifierAsArgument() const;
};

// parses XMLElement's contents into variable data
/*  <member>     <type>   </type>    <name>    </name></member>
 *          ^prefix    ^type     ^suffix    ^identifier
 */
class XMLVariableParser : public VariableData, protected tinyxml2::XMLVisitor {

    State state{PREFIX}; // FSM state

  public:
    XMLVariableParser() = default;

    XMLVariableParser(tinyxml2::XMLElement *element);

    // Entry point, reset state to initial, parse XMLElement and trim
    void parse(tinyxml2::XMLElement *element);

    // next Visit call appends according to state until set to DONE
    virtual bool Visit(const tinyxml2::XMLText &text) override;

    // leaves one space in suffix
    void trim();
};

#endif // XMLVARIABLEPARSER_H
