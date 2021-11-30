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

#include <string>
#include <array>
#include "tinyxml2.h"

#include <stdexcept>
#include <iostream>
#include <memory>

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

    void set(size_t index, const std::string &str) {
        if (index >= size()) {
            return;
        }
        std::array<std::string, 4>::operator[](index) = str;
    }

    const std::string& get(size_t index) const {
        if (index >= size()) {
            throw std::exception();
            //return "";
        }
        return std::array<std::string, 4>::operator[](index);
    }

    // prefix getter
    const std::string& prefix() const {
        return get(PREFIX);
    }
    // suffix getter
    const std::string& suffix() const {
        return get(SUFFIX);
    }
    // type getter
    const std::string& type() const {
        return get(TYPE);
    }
    // name getter
    const std::string& identifier() const {
        return get(IDENTIFIER);
    }

    void setType(const std::string& type) {
        set(TYPE, type);
    }

    void setIdentifier(const std::string& identifier) {
        set(IDENTIFIER, identifier);
    }

    void setFullType(const std::string& prefix, const std::string& type, const std::string& suffix) {
        set(PREFIX, prefix);
        set(TYPE, type);
        set(SUFFIX, suffix);
    }

//    std::string getProto() const {
//        std::string out;
//        for (size_t i = 0; i < size(); ++i) {
//            out += get(i);
//        }
//        return out;
//    }

};

// holds variable information broken into 4 sections
struct VariableData : public VariableFields {

    std::string altPFN;

public:
    enum Type {
        TYPE_INVALID,
        TYPE_DEFAULT,
        TYPE_RETURN,
        TYPE_ARRAY_PROXY,
        TYPE_SIZE_AND_ARRAY_PROXY,
        //TYPE_TEMPLATE,
        //TYPE_TEMPLATE_WITH_SIZE,
        TYPE_VECTOR
    };

    VariableFields original;

    VariableData() {
        arrayLengthFound = false;
        ignoreFlag = false;
        _hasLenAttrib = false;
        specialType = TYPE_DEFAULT;
    }

    VariableData(Type type) {
        arrayLengthFound = false;
        specialType = type;
        if (specialType == TYPE_INVALID) {
            ignoreFlag = true;
        }
    }

    void setAltPFN(const std::string &str) {
        altPFN = str;
    }

    void setType(Type type) {
        specialType = type;
    }

    bool hasLenAttrib() const {
        return _hasLenAttrib;
    }

    std::string lenAttrib() {
        return _lenAttrib;
    }

    std::string lenAttribVarName() {
        size_t pos = _lenAttrib.find_first_of("->");
        if (pos != std::string::npos) {
            return _lenAttrib.substr(0, pos);
        }
        return _lenAttrib;
    }

    // arrayLength flag getter
    bool hasArrayLength() const {
        return arrayLengthFound;
    }
    // arrayLength getter
    const std::string& arrayLength() const {
        return arrayLengthStr;
    }
//    // getter for all fields combined
//    std::string proto() const {
//        std::string out = getProto();
//        if (arrayLengthFound) {
//            out += "[" + arrayLengthStr + "]";
//        }
//        return out;
//    }

//    // getter for all fields combined for C version
//    std::string originalProto() const {
//        std::string out = original.getProto();
//        if (arrayLengthFound) {
//            out += "[" + arrayLengthStr + "]";
//        }
//        return out;
//    }

    bool isDefault() const {
        return specialType == TYPE_DEFAULT;
    }

    void setIgnoreFlag(bool value) {
        ignoreFlag = value;
    }

    bool getIgnoreFlag() const {
        return ignoreFlag;
    }

    bool isInvalid() const {
        return specialType == TYPE_INVALID;
    }

    void convertToCpp() {
        for (size_t i = 0; i < size(); ++i) {
            original.set(i, get(i));
        }

        set(TYPE, strStripVk(get(TYPE)));
        //std::string temp = get(IDENTIFIER);
        set(IDENTIFIER, strStripVk(get(IDENTIFIER)));
        //std::cout << "Original id: " << temp << " converted: " << get(IDENTIFIER) << std::endl;
    }

    void convertToArrayProxy(bool bindSizeArgument = true) {
        specialType = bindSizeArgument ? TYPE_SIZE_AND_ARRAY_PROXY
                                       : TYPE_ARRAY_PROXY;

        //it->setFullType("", "ArrayProxy<const " + it->type() + ">", " const &");
        removeLastAsterisk();
        //std::cout << get(IDENTIFIER) << " --> to array proxy"  << std::endl;
    }


    void bindLengthAttrib(std::shared_ptr<VariableData> var) {
        std::cout << "Obj[" << this << "] " << "Bind to lenght attrib var: " << var->identifier() << std::endl;
        _lenAttribVar = var;
    }

    bool hasLengthAttribVar() {
        return _lenAttribVar.get() != nullptr;
    }

    std::shared_ptr<VariableData> lengthAttribVar() {
        std::cout << "Obj[" << this << "] " << "len attrib var get:" << _lenAttribVar.get() << std::endl;
        if (!_lenAttribVar.get()) {
            throw std::runtime_error("access to null lengthAttrib");
        }
        return _lenAttribVar;
    }

    void convertToTemplate() {
        //specialType = bindSizeArgument ? TYPE_TEMPLATE_WITH_SIZE
          //                             : TYPE_TEMPLATE;

        setFullType("", "T", "");
        //std::cout << get(IDENTIFIER) << " --> to array proxy"  << std::endl;
    }

    void convertToReturn() {
        specialType = TYPE_RETURN;
        ignoreFlag = true;

        removeLastAsterisk();
    }

    void convertToStdVector() {
        specialType = TYPE_VECTOR;
        removeLastAsterisk();
    }

    bool removeLastAsterisk() {
        std::string &suffix = VariableFields::operator[](SUFFIX);
        if (suffix.ends_with("*")) {
            suffix.erase(suffix.size() - 1); // removes * at end
            return true;
        }
        return false;
    }

    std::string toArgument() const {
        if (!altPFN.empty()) {
            return altPFN;
        }
        //std::cout << "to argument: " << get(TYPE) << " vs " << original.get(TYPE) << std::endl;
        switch (specialType) {
            case TYPE_VECTOR:
            case TYPE_ARRAY_PROXY:
                return toArgumentArrayProxy();
            case TYPE_SIZE_AND_ARRAY_PROXY:
                return toArgumentSizeAndArrayProxy();
            case TYPE_RETURN:
                return toArgumentReturn();
            //case TYPE_TEMPLATE_WITH_SIZE:
              //  return toArgumentTemplateWithSize();
            default:
                return toArgumentDefault();
        }
    }

    // full type getter
    std::string fullType() const {
        std::string type = get(PREFIX) + get(TYPE) + get(SUFFIX);
        switch (specialType) {
            case TYPE_ARRAY_PROXY:
            case TYPE_SIZE_AND_ARRAY_PROXY:
                return "ArrayProxy<" + type + "> const &";
            case TYPE_VECTOR:
                return "std::vector<" + type + ">";
            default:
                return type;
        }
    }

    // full type getter
    std::string originalFullType() const {
        return original.get(PREFIX) +
               original.get(TYPE)   +
               original.get(SUFFIX);
    }

    // getter for all fields combined
    std::string toString() const {
        std::string out = fullType();
        if (!out.ends_with(" ")) {
            out += " ";
        }
        out += get(IDENTIFIER);
        out += optionalArraySuffix();
        return out;
    }

    // getter for all fields combined for C version
    std::string originalToString() const {
        std::string out = originalFullType();
        if (!out.ends_with(" ")) {
            out += " ";
        }
        out += original.get(IDENTIFIER);
        out += optionalArraySuffix();
        return out;
    }


protected:

    Type specialType;
    bool ignoreFlag;
    bool arrayLengthFound;
    std::string arrayLengthStr;
    bool _hasLenAttrib;
    std::string _lenAttrib;
    std::shared_ptr<VariableData> _lenAttribVar;

private:

    std::string optionalArraySuffix() const {
        if (arrayLengthFound) {
            return "[" + arrayLengthStr + "]";
        }
        return "";
    }

    std::string toArgumentSizeAndArrayProxy() const { // in reality it's two arguments
        return get(IDENTIFIER) + ".size()" + ", " + toArgumentArrayProxy();
    }

    std::string toArgumentArrayProxy() const {
        return "reinterpret_cast<" + originalFullType() + ">(" + get(IDENTIFIER) + ".data())";
    }

    std::string createCast(std::string from) const {
        std::string cast = "static_cast";
        if (strContains(original.get(SUFFIX), "*") || arrayLengthFound) {
            cast = "reinterpret_cast";
        }
        return cast + "<" + originalFullType() + (arrayLengthFound? "*" : "") + ">(" + from + ")";

    }

    std::string toArgumentReturn() const {
        if (get(TYPE) == original.get(TYPE)) {
            return "&" + get(IDENTIFIER);
        }
        return createCast("&" + get(IDENTIFIER));
    }

//    std::string toArgumentTemplateWithSize() const {
//        return "sizeof(" + get(TYPE) + "), " + toArgumentDefault();
//    }

    std::string toArgumentDefault() const {
        if (get(TYPE) == original.get(TYPE)) {
            return get(IDENTIFIER);
        }                
        return createCast(get(IDENTIFIER));
    }

};

// parses XMLElement's contents into variable data
/*  <member>     <type>   </type>    <name>    </name></member>
 *          ^prefix    ^type     ^suffix    ^identifier
 */
class XMLVariableParser : public VariableData, protected tinyxml2::XMLVisitor {

    State state{PREFIX}; // FSM state

public:
    XMLVariableParser() = default;

    XMLVariableParser(tinyxml2::XMLElement *element) {
        const char *len = element->Attribute("len");
        if (len) {
            //std::cout << "Parsing has len: " << len << std::endl;
            _lenAttrib = len;
            _hasLenAttrib = true;
        }

        parse(element);

    }

    // Entry point, reset state to initial, parse XMLElement and trim
    void parse(tinyxml2::XMLElement *element) {        
        state = PREFIX;
        arrayLengthFound = false;
        element->Accept(this);
        trim();
        convertToCpp();
    }

    // next Visit call appends according to state until set to DONE
    virtual bool Visit(const tinyxml2::XMLText &text) override {
        std::string_view tag = text.Parent()->Value();
        std::string_view value = text.Value();

        if (tag == "type") { // text is type field
            state = TYPE;
        }
        else if (tag == "name") { // text is name field
            state = IDENTIFIER;
        }
        else if (state == TYPE) { // text after type is suffix (optional)
            state = SUFFIX;
        }
        else if (state == IDENTIFIER) {
            if (value == "[") { // text after name is array size if [
                state = BRACKET_LEFT;
            }
            else if (value.starts_with("[") && value.ends_with("]")) {
                arrayLengthStr = value.substr(1, value.size() - 2);
                arrayLengthFound = true;
                state = DONE;
                return false;
            }
            else {
                state = DONE;
                return false;
            }
        }
        else if (state == BRACKET_LEFT) { // text after [ is <enum>SIZE</enum>
            state = ARRAY_LENGTH;
            arrayLengthStr = value;
        }
        else if (state == ARRAY_LENGTH) {           
            if (value == "]") { //  ] after SIZE confirms arrayLength field
                arrayLengthFound = true;                
            }
            state = DONE;
            return false;
        }


        if (state < size()) { // append if state index is in range
            VariableFields::operator[](state).append(value);
        }
        return true;
    }

    // leaves one space in suffix
    void trim() {
        std::string &suffix = VariableFields::operator[](SUFFIX);
        const auto it = suffix.find_last_not_of(' ');
        if (it != std::string::npos) {
            suffix.erase(it + 1); // removes trailing space
        }
        //suffix += " ";
    }

};

#endif // XMLVARIABLEPARSER_H
