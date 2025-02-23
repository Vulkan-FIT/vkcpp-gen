// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//                                                           copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef XMLVARIABLEPARSER_H
#define XMLVARIABLEPARSER_H

#include "Enums.hpp"
#include "Utils.hpp"
#include "tinyxml2.h"

#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace vkgen
{

    class Registry;
    class Generator;

    enum State
    {  // State used for indexing and FSM
        PREFIX     = 0,
        TYPE       = 1,
        SUFFIX     = 2,
        IDENTIFIER = 3,
        ARRAY_LENGTH,
        BRACKET_LEFT,
        DONE
    };

    struct VariableFields
    {
        static constexpr size_t N = 4;

        // prefix getter
        const std::string &prefix() const {
            return fields[PREFIX];
        }

        // suffix getter
        const std::string &suffix() const {
            return fields[SUFFIX];
        }

        // type getter
        const std::string &type() const {
            return fields[TYPE];
        }

        // name getter
        const std::string &identifier() const {
            return fields[IDENTIFIER];
        }

        void setPrefix(const std::string &prefix) {
            fields[PREFIX] = prefix;
        }

        void setType(const std::string &type) {
            fields[TYPE] = type;
        }

        void setSuffix(const std::string &suffix) {
            fields[SUFFIX] = suffix;
        }

        void setIdentifier(const std::string &identifier) {
            fields[IDENTIFIER] = identifier;
        }

        void setFullType(const std::string &prefix, const std::string &type, const std::string &suffix) {
            fields[PREFIX] = prefix;
            fields[TYPE]   = type;
            fields[SUFFIX] = suffix;
        }

        bool isPointer() const {
            return fields[SUFFIX].find("*") != std::string::npos;
        }

        bool isConst() const {
            return fields[PREFIX].find("const") != std::string::npos;
        }

        bool isConstSuffix() const {
            return fields[SUFFIX].find("const") != std::string::npos;
        }

      protected:
        void set(size_t index, const std::string &str);

        std::array<std::string, N> fields;
    };

    struct VariableDataInfo;

    struct VariableBase : public VariableFields
    {
        enum Type
        {
            TYPE_INVALID,
            TYPE_DEFAULT,
            TYPE_RETURN,
            TYPE_ARRAY_PROXY,
            TYPE_ARRAY_PROXY_NO_TEMPORARIES,
            TYPE_VECTOR,
            TYPE_TEMPL_VECTOR,
            TYPE_ARRAY,
            TYPE_VK_VECTOR,
            TYPE_EXP_ARRAY,
            TYPE_OPTIONAL,
            TYPE_DISPATCH,
            TYPE_STD_ALLOCATOR,
            TYPE_PLACEHOLDER
        };

        enum ArraySize
        {
            NONE,
            DIM_1D,
            DIM_2D
        };

        enum class Flags : int
        {
            NONE = 0,
            // HANDLE = 1,
            ARRAY            = 2,
            ARRAY_IN         = 4,
            ARRAY_OUT        = 8,
            CLASS_VAR_VK     = 16,
            CLASS_VAR_UNIQUE = 32,
            CLASS_VAR_RAII   = 64,
            OUT              = 128
        };

        Type      specialType = Type::TYPE_INVALID;
        Flags     flags       = Flags::NONE;
        Namespace ns          = Namespace::NONE;
        bool      optional    = false;
    };

    class VariableBase2 : public VariableBase
    {
      protected:
        ArraySize   arrayAttrib{};
        std::string arraySizes[2];
        std::string lenAttribStr;
        std::string altlenAttribStr;

      public:
        VariableFields           original;
        std::vector<std::string> lenExpressions;
    };

    // holds variable information broken into 4 sections
    struct VariableData
      : public VariableBase2
      , public MetaType
    {
        VariableBase saved;

      public:

        struct Template {
            std::string prefix;
            std::string type;
            std::string assignment;
            std::string pass;

            void clear() {
              type.clear();
              assignment.clear();
              pass.clear();
            }
        };

        VariableData(Registry &reg, xml::Element);

        VariableData(const String &type, const std::string &id);

        explicit VariableData(Type type = TYPE_DEFAULT);

        explicit VariableData(const VariableDataInfo &info);

        explicit VariableData(const String &type);

        // copy
        VariableData(const VariableData &o) : VariableBase2(o) {
            // std::cout << "VariableData copy ctor " << this << '\n';
            _assignment = o._assignment;
            lenghtVar = o.lenghtVar;
            dataTemplate = o.dataTemplate;
            sizeTemplate = o.sizeTemplate;
            allocatorTemplate = o.allocatorTemplate;
            stdAllocatorIdentifier = o.stdAllocatorIdentifier;
            setMetaType(o.metaType());
        }

        // VariableData(const VariableData &o) = default;

        // assignment
        VariableData &operator=(VariableData &) = delete;

        ~VariableData() {
            // std::cout << "~var dtor " << this << " " << fullType() << ", s: " << (int) specialType << '\n';
        }

        bool isTemplated() const noexcept {
            return !dataTemplate.type.empty() || !sizeTemplate.type.empty() || !allocatorTemplate.type.empty();
        }

        void updateMetaType(Registry &reg);

        bool check(const Generator &gen, VariableData &ref, std::stringstream &s) const {
            std::stringstream ss;

            const auto cmp = [&](const std::string &msg, auto &rhs, auto &lhs) {
                if (rhs != lhs) {
                    ss << "  " << msg << ": " << rhs << " != " << lhs << '\n';
                }
            };
            cmp("prefix", prefix(), ref.prefix());
            cmp("type", type(), ref.type());
            cmp("suffix", suffix(), ref.suffix());
            cmp("id", identifier(), ref.identifier());
            if (specialType != ref.specialType) {
                ss << "  specialType: " << int(specialType) << " != " << int(ref.specialType) << '\n';
            }

            const auto &str = ss.str();
            if (!str.empty()) {
                s << " comp var: " << fullType(gen) << '\n';
                s << str << '\n';
                return false;
            }
            return true;
        }

        void save() {
            saved = *reinterpret_cast<VariableBase *>(this);
        }

        void restore() {
            // std::cerr << "  restore() " << this << '\n';
            VariableBase::operator=(saved);

            ignoreFlag  = false;
            ignorePFN   = false;
            ignoreProto = false;
            ignorePass  = false;
            localVar    = false;
            structChain = false;

            altPFN.clear();
            _assignment.clear();
            dataTemplate.clear();
            sizeTemplate.clear();
            allocatorTemplate.clear();
            stdAllocatorIdentifier.clear();
            dbgTag.clear();
        }

        std::string argdbg() const {
            std::string dbg = "// ";
            //        std::stringstream s;
            //        s << std::hex << this;
            // dbg += "<" + s.str() + ">  ";
            dbg += "  ";
            if (getIgnoreFlag()) {
                dbg += "I ";
            }
            if (getIgnorePFN()) {
                dbg += "F ";
            }
            if (getIgnoreProto()) {
                dbg += "A ";
            }
            if (getIgnorePass()) {
                dbg += "P ";
            }
            dbg += getDbgTag();
            dbg += '[';
            if (original.type().empty()) {
                dbg += '?';
            } else {
                dbg += original.type();
            }
            dbg += ']';
            if (isOptional()) {
                dbg += " O";
            }
            if (getNamespace() == Namespace::RAII) {
                dbg += " :R";
            }
            if (isLocalVar()) {
                dbg += " :Loc";
            }
            // dbg += "*/ ";
            return dbg;
        }

        void setAltPFN(const std::string &str) {
            altPFN = str;
        }

        void setSpecialType(Type type) {
            specialType = type;
        }

        void setNameSuffix(std::string &str) {
            nameSuffix = str;
        }

        bool hasNameSuffix() const {
            return !nameSuffix.empty();
        }

        Type getSpecialType() const {
            return specialType;
        }

        std::string getNameSuffix() const {
            return nameSuffix;
        }

        std::string getLenAttrib() const {
            return lenAttribStr;
        }

        std::string getAltlenAttrib() const {
            return altlenAttribStr;
        }

        std::string getLenAttribIdentifier() const;

        std::string getLenAttribRhs() const;

        std::string namespaceString(const Generator &gen, bool forceNamespace = false) const;

        bool isLenAttribIndirect() const;

        std::string getDbgTag() const {
            return dbgTag;
        }

        void setDbgTag(const std::string &tag) {
            dbgTag = tag;
        }

        void appendDbg(const std::string &tag) {
            dbgTag += tag;
        }

        bool isOptional() const {
            return optional;
        }

        void setOptional(bool value) {
            optional = value;
        }

        void overrideOptional(bool value) {
            optional = value;
            saved.optional = value;
        }

        bool hasArrayLength() const {
            return arrayAttrib != ArraySize::NONE;
        }

        const std::string &arrayLength(int index = 0) const {
            return arraySizes[index];
        }

        bool isDefault() const {
            return specialType == TYPE_DEFAULT;
        }

        void setIgnoreFlag(bool value) {
            ignoreFlag = value;
        }

        bool getIgnoreFlag() const {
            return ignoreFlag;
        }

        void setIgnoreProto(bool value) {
            // std::cerr << "  set ignore: " << value << "  " << this << '\n';
            ignoreProto = value;
        }

        bool getIgnoreProto() const {
            return ignoreProto;
        }

        bool isStructChain() const {
            return structChain;
        }

        void setIgnorePass(bool value) {
            ignorePass = value;
        }

        bool getIgnorePass() const {
            return ignorePass;
        }

        bool isLocalVar() const {
            return localVar;
        }

        void setNamespace(Namespace);

        void toRAII();

        Namespace getNamespace() const;

        void setIgnorePFN(bool value) {
            ignorePFN = value;
        }

        bool getIgnorePFN() const {
            return ignorePFN;
        }

        bool isInvalid() const {
            return specialType == TYPE_INVALID;
        }

        bool isReturn() const {
            return specialType == TYPE_RETURN;
        }

        bool isNullTerminated() const;

        void convertToArrayProxy();

        void convertToConstReference();

        void convertToStructChain();

        void bindLengthVar(VariableData &var, bool noArray = false);

        void bindArrayVar(VariableData *var);

        bool hasLengthVar() const {
            return lenghtVar != nullptr;
        }

        VariableData *getLengthVar() const;

        const std::vector<VariableData *> &getArrayVars() const;

        std::vector<VariableData *> &getArrayVars() {
            return arrayVars;
        };

        std::string getReturnType(const Generator &gen, bool forceNamespace = false) const;

        void createLocalReferenceVar(const Generator &gen, const std::string &indent, const std::string &assignment, std::string &output);

        void createLocalVar(const Generator &gen, const std::string &indent, const std::string &dbg, std::string &output, const std::string &assignment = "");

        std::string generateVectorReserve(const Generator &gen, const std::string &indent);

        std::string getLocalInit() const {
            std::string output;
            if (isArray() && specialType != VariableData::TYPE_EXP_ARRAY) {
                const auto &var = getLengthVar();

                VariableData *arrayVar = {};
                for (const auto &v : var->getArrayVars()) {
                    if (v != this && v->isArrayIn()) {
                        arrayVar = v;
                        break;
                    }
                }

                if (arrayVar) {
                    // output += "/*Lia*/;
                    output += arrayVar->toArrayProxySize();
                } else {
                    if (!var->getIgnoreProto()) {
                        // output += "/*Li*/";
                        output += var->identifier();
                        std::string rhs;
                        size_t      pos = lenAttribStr.find_first_of("->");
                        if (pos != std::string::npos) {
                            rhs = lenAttribStr.substr(pos + 2);
                        }
                        if (!rhs.empty()) {
                            if (var->isPointer()) {
                                output += "->";
                            } else {
                                output += ".";
                            }
                            output += rhs;
                        }
                    }
                }

                const auto &templ = dataTemplate.type;
                if (!output.empty() && !templ.empty()) {
                    output += " / sizeof( " + templ + " )";
                }
            }
            if (!stdAllocatorIdentifier.empty()) {
                if (!output.empty()) {
                    output += ", ";
                }
                output += stdAllocatorIdentifier;
            }
            return output;
        }

        void setStdAllocator(const std::string &id) {
            stdAllocatorIdentifier = id;
        }

//        std::string getAllocatorType() const {
//            return optionalAllocator;
//        }

        void convertToReturn();

        void convertToReference();

        void convertToPointer();

        void convertToOptionalWrapper();

        void convertToStdVector(const Generator &gen);

        bool removeLastAsterisk();

        void setConst(bool enabled);

        Flags getFlags() const;

        void setFlag(Flags flag, bool enabled);

        // bool isHandle() const;
        bool isArray() const;
        bool isArrayIn() const;
        bool isArrayOut() const;
        bool isOutParam() const;

        std::string getAltPNF() const {
            return altPFN;
        }

        std::string isClean() const {
            std::string s;
            if (ignoreFlag != false) {
                s += "I";
            }
            if (ignorePFN != false) {
                s += "P";
            }
            if (ignoreProto != false) {
                s += "A";
            }
            if (ignorePass != false) {
                s += "p";
            }
            if (!s.empty()) {
                s = "{" + s + "}";
            }

            if (!altPFN.empty()) {
                s += " alt: " + altPFN;
            }
            if (!_assignment.empty()) {
                s += " as: " + _assignment;
            }
//            if (!optionalTemplate.empty()) {
//                s += " temp: " + optionalTemplate;
//            }
//            if (!optionalTemplateAssignment.empty()) {
//                s += " tmpa: " + optionalTemplateAssignment;
//            }
//            if (!stdAllocatorIdentifier.empty()) {
//                s += " std: " + optionalTemplateAssignment;
//            }
            return s;
        }

        std::string flagstr() const;

        std::string dbgstr() const {
            std::stringstream s;
            s << "  //";
            s << "<" << std::hex << this << std::dec << ">";
            s << "P[" << prefix() << "]";
            s << "T[" << type() << "]";
            s << "S[" << suffix() << "]";
            s << "I[" << identifier() << "]";
            s << " type: " << std::to_string(specialType);
            std::string c = isClean();
            if (c.empty()) {
                s << ", clean";
            } else {
                s << c;
            }
            s << ", f: " + flagstr();

#ifndef NDEBUG
            if (!bound) {
                s << " not bound!";
            }
#endif
            s << "\n";

            const auto &l = getLengthVar();
            if (l) {
                s << "    //L: ";
                s << "<" << std::hex << l << std::dec << ">\n";
            }
            for (const auto &v : getArrayVars()) {
                s << "    //A: ";
                s << "<" << std::hex << v << std::dec << ">\n";
            }

            return s.str();
        }

        std::string toClassVar(const Generator &gen) const;

        std::string toArgument(const Generator &gen, bool useOriginal = false) const;

        std::string toVariable(const Generator &gen, const VariableData &dst, bool useOriginal = false) const;

        std::string toStructArgumentWithAssignment(const Generator &gen) const;

        // full type getter
        std::string fullType(const Generator &gen, bool forceNamespace = false) const;

        // full type getter
        std::string originalFullType() const {
            return original.prefix() + original.type() + original.suffix();
        }

        // getter for all fields combined
        std::string toString(const Generator &gen) const;

        std::string toStructString(const Generator &gen, bool cstyle = false) const;

        std::string declaration(const Generator &gen) const;

        std::string toStringWithAssignment(const Generator &gen) const;

        std::string toStructStringWithAssignment(const Generator &gen, bool cstyle = false) const;

        // getter for all fields combined for C version
        std::string originalToString() const;

        void setAssignment(const std::string &str) {
            _assignment = str;
        }

        void setReference(bool enabled) {
            auto &suf = fields[SUFFIX];
            auto  pos = suf.find_last_of('&');
            if (enabled && pos == std::string::npos) {
                suf += '&';
            } else if (!enabled && pos != std::string::npos) {
                suf.erase(pos, 1);
            }
        }

        bool isReference() const {
            auto pos = suffix().find_last_of('&');
            return pos != std::string::npos;
        }

        std::string getAssignment() const {
            return _assignment;
        }

        // void setTemplate(const std::string &str);
        // void setTemplateAssignment(const std::string &str);
        // void addTemplate(const std::string &type, const std::string &assignment = "");

        // const Templates& getTemplates() const;

        // void setTemplateDataType(const std::string &str);

        // const std::string &getTemplateDataType() const;

        // std::string getTemplateAssignment() const;

        std::string                         toArrayProxySize() const;
        std::string                         toArrayProxyData() const;
        std::pair<std::string, std::string> toArrayProxyRhs() const;

        std::string toArgumentArrayProxy(const Generator &gen) const;

        std::string optionalArraySuffix() const;

        void evalFlags();


        Template   dataTemplate;
        Template   sizeTemplate;
        Template   allocatorTemplate;

      protected:
        // const Generator &gen;
        VariableData *lenghtVar = {};
        std::string   _assignment;
        std::string   altPFN;
        std::string   nameSuffix;

        // Templates templates;

        // std::string   templateDataTypeStr;
        std::string   stdAllocatorIdentifier;
        std::string   dbgTag;
        bool          ignoreFlag  = false;
        bool          ignorePFN   = false;
        bool          ignoreProto = false;
        bool          ignorePass  = false;
        bool          localVar    = false;
        bool          structChain = false;

        bool nullTerminated = false;

        void addArrayLength(const std::string &length) {
            switch (arrayAttrib) {
                case ArraySize::NONE:
                    arraySizes[0] = length;
                    arrayAttrib   = ArraySize::DIM_1D;
                    break;
                case ArraySize::DIM_1D:
                    arraySizes[1] = length;
                    arrayAttrib   = ArraySize::DIM_2D;
                    break;
                case ArraySize::DIM_2D: throw std::runtime_error("xml registry: unsupported array dimension"); break;
            }
        };

        std::vector<VariableData *> arrayVars = {};

        std::string createCast(const Generator &gen, std::string from) const;

        std::string toArgumentDefault(const Generator &gen, bool useOriginal = false) const;

        std::string identifierAsArgument(const Generator &gen) const;

        std::string createVectorType(const std::string_view vectorType, const std::string &type) const;

        void convertToCpp();

        // leaves one space in suffix
        void trim();

        friend class XMLVariableParser;
#ifndef NDEBUG
      public:
        bool bound = false;
#endif
    };

    struct VariableDataInfo
    {
        std::string         prefix;
        std::string         vktype;
        std::string         stdtype;
        std::string         suffix;
        std::string         identifier;
        std::string         assigment;
        Namespace           ns          = {};
        VariableData::Flags flag        = {};
        VariableData::Type  specialType = VariableData::TYPE_DEFAULT;
        MetaType::Value     metaType    = {};
    };

    // parses XMLElement's contents into variable data
    /*  <member>     <type>   </type>    <name>    </name></member>
     *          ^prefix    ^type     ^suffix    ^identifier
     */
    class XMLVariableParser : protected tinyxml2::XMLVisitor
    {
        VariableData &data;
        State         state{ PREFIX };  // FSM state

      public:
        // Entry point, reset state to initial, parse XMLElement and trim
        XMLVariableParser(VariableData &data, tinyxml2::XMLElement *element);

        // next Visit call appends according to state until set to DONE
        virtual bool Visit(const tinyxml2::XMLText &text) override;
    };

    inline VariableData::Flags operator|(const VariableData::Flags &a, const VariableData::Flags &b) {
        return static_cast<VariableData::Flags>(static_cast<int>(a) | static_cast<int>(b));
    }

    inline VariableData::Flags operator&(const VariableData::Flags &a, const VariableData::Flags &b) {
        return static_cast<VariableData::Flags>(static_cast<int>(a) & static_cast<int>(b));
    }

    inline VariableData::Flags &operator|=(VariableData::Flags &a, const VariableData::Flags &b) {
        return a = a | b;
    }

    inline VariableData::Flags &operator&=(VariableData::Flags &a, const VariableData::Flags &b) {
        return a = a & b;
    }

    inline bool operator!=(const VariableData::Flags &a, const VariableData::Flags &b) {
        return static_cast<int>(a) != static_cast<int>(b);
    }

    inline bool hasFlag(const VariableData::Flags &a, const VariableData::Flags &b) {
        return static_cast<int>(a) & static_cast<int>(b);
    }

    class XMLDefineParser : protected tinyxml2::XMLVisitor
    {
        enum State
        {  // State used for parsing
            DEFINE = 0,
            NAME   = 1,
            VALUE  = 2,
            DONE
        };

        State state;  // FSM state

      public:
        std::string name;
        std::string value;

        XMLDefineParser(tinyxml2::XMLElement *element);

        // Entry point, reset state to initial, parse XMLElement and trim
        void parse(tinyxml2::XMLElement *element);

        void trim();

        virtual bool Visit(const tinyxml2::XMLText &text) override;
    };

    class XMLTextParser : protected tinyxml2::XMLVisitor
    {
        const tinyxml2::XMLNode *root = {};
        const tinyxml2::XMLNode *prev = {};
        std::unordered_map<std::string, std::string> fields;
      public:
        std::string text;

        std::string& operator[](const std::string &field);

        XMLTextParser(xml::Element element);

        virtual bool Visit(const tinyxml2::XMLText &text) override;
    };

    /*
    class XMLBaseTypeParser : protected tinyxml2::XMLVisitor
    {
      public:
        std::string name;
        std::string text;

        XMLBaseTypeParser(tinyxml2::XMLElement *element);

        virtual bool Visit(const tinyxml2::XMLText &text) override;
    };
    */

}  // namespace vkgen

#endif  // XMLVARIABLEPARSER_H
