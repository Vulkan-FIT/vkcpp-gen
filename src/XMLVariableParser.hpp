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

#include <iostream>

//holds variable information broken into 4 sections
struct VariableData {
protected:

    enum State { //State used for indexing and parsing FSM
        PREFIX = 0,
        TYPE = 1,
        SUFFIX = 2,
        IDENTIFIER = 3,
        ARRAY_LENGTH = 4,
        BRACKET_LEFT,
        DONE
    };

    bool arrayLengthFound;
    std::array<std::string, 5> fields;

public:
    //prefix getter
    const std::string& prefix() const {
        return fields[PREFIX];
    }
    //suffix getter
    const std::string& suffix() const {
        return fields[SUFFIX];
    }
    //type getter
    const std::string& type() const {
        return fields[TYPE];
    }
    //name getter
    const std::string& identifier() const {
        return fields[IDENTIFIER];
    }
    //arrayLength flag getter
    bool hasArrayLength() const {
        return arrayLengthFound;
    }
    //arrayLength getter
    const std::string& arrayLength() const {
        return fields[ARRAY_LENGTH];
    }
    //getter for first 4 fields combined
    std::string proto() const {
        std::string out;
        for (int i = 0; i < 4; ++i) {
            out += fields[i];
        }
        return out;
    }

};

//parses XMLElement's contents into variable data
/*  <member>     <type>   </type>    <name>    </name></member>
 *          ^prefix    ^type     ^suffix    ^identifier
 */
class XMLVariableParser : public VariableData, protected tinyxml2::XMLVisitor {

    State state{PREFIX}; //FSM state

public:
    XMLVariableParser() = default;

    XMLVariableParser(tinyxml2::XMLElement *element) {
        parse(element);
    }

    //Entry point, reset state to initial, parse XMLElement and trim
    void parse(tinyxml2::XMLElement *element) {        
        state = PREFIX;
        arrayLengthFound = false;
        element->Accept(this);
        trim();
    }

    //next Visit call appends according to state until set to DONE
    virtual bool Visit(const tinyxml2::XMLText &text) override {
        std::string_view tag = text.Parent()->Value();
        std::string_view value = text.Value();

        if (tag == "type") { //text is type field
            state = TYPE;
        }
        else if (tag == "name") { //text is name field
            state = IDENTIFIER;
        }
        else if (state == TYPE) { //text after type is suffix (optional)
            state = SUFFIX;
        }
        else if (state == IDENTIFIER) {
            if (value == "[") { //text after name is array size if [
                state = BRACKET_LEFT;
            }
            else {
                state = DONE;
                return false;
            }
        }
        else if (state == BRACKET_LEFT) { //text after [ is <enum>SIZE</enum>
            state = ARRAY_LENGTH;
        }
        else if (state == ARRAY_LENGTH) {
            if (value == "]") { // ] after SIZE confirms arrayLength field
                arrayLengthFound = true;
            }
            state = DONE;
            return false;
        }

        if (state < fields.size()) { //append if state index is in range
            fields[state].append(value);
        }
        return true;
    }

    //leaves one space in suffix
    void trim() {
        const auto it = fields[SUFFIX].find_last_not_of(' ');
        if (it != std::string::npos) {
            fields[SUFFIX].erase(it + 1); //removes trailing space
        }
        fields[SUFFIX] += " ";
    }

};

#endif // XMLVARIABLEPARSER_H
