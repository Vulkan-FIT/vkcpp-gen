// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
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

#include "Variable.hpp"

#include "Format.hpp"
#include "Generator.hpp"

void vkgen::VariableFields::set(size_t index, const std::string &str) {
    if (index >= N) {
        std::cerr << "VariableFields set index out of bounds" << '\n';
        return;
    }
    fields[index] = str;
}

vkgen::VariableData::VariableData(Registry &reg, xml::Element elem) {
    XMLVariableParser p(*this, elem.toElement());

    const auto len      = elem.optional("len");
    const auto altlen   = elem.optional("altlen");
    const auto optional = elem.optional("optional");
    if (len) {
        lenExpressions = split(std::string(len.value()), ",");
        for (const auto &str : lenExpressions) {
            if (str.empty() || std::isdigit(str[0])) {
                continue;
            }
            if (str == "null-terminated") {
                nullTerminated = true;
            } else {
                if (!lenAttribStr.empty()) {
                    std::cout << "Warn: len attrib currently set (is " << lenAttribStr << ", new: " << str << "). xml: " << len.value() << '\n';
                }
                lenAttribStr = str;
            }
        }
    }
    if (altlen) {
        altlenAttribStr = altlen.value();
    }
    if (optional) {
        this->optional = optional == "true";
    }

    trim();
    original = *reinterpret_cast<VariableFields *>(this);

    convertToCpp();

    // updateMetaType(reg);
}

vkgen::VariableData::VariableData(Type type) {
    specialType = type;
    ignoreFlag  = specialType == TYPE_INVALID;
    ignoreProto = specialType == TYPE_INVALID;

    // std::cout << "var ctor1 " << this << '\n';
}

vkgen::VariableData::VariableData(const VariableDataInfo &info) {
    if (!info.stdtype.empty()) {
        original.setType(info.stdtype);
        setFullType(info.prefix, String(info.stdtype, false), info.suffix);
    } else {
        original.setType(info.vktype);
        setFullType(info.prefix, String(info.vktype, true), info.suffix);
    }
    setIdentifier(info.identifier);
    setAssignment(info.assigment);
    setNamespace(info.ns);
    setFlag(info.flag, true);
    setSpecialType(info.specialType);
    setMetaType(info.metaType);
    // std::cout << "var ctor2 " << this << '\n';
    // save();
}

vkgen::VariableData::VariableData(const String &type) : VariableData(type, strFirstLower(type)) {}

vkgen::VariableData::VariableData(const String &type, const std::string &id) : VariableData(TYPE_DEFAULT) {
    setIdentifier(id);
    original.setFullType("", type.original, " *");
    setFullType("", type, " *");
    convertToReference();
}

void vkgen::VariableData::updateMetaType(Registry &reg) {
    auto *t = reg.find(original.type());
    if (t) {
        if (nameSuffix.empty() && !type().starts_with("PFN_")) {
            ns = Namespace::VK;
        } else {
            setType(original.type());
        }
        setMetaType(t->metaType());
    }
}

std::string vkgen::VariableData::getLenAttribIdentifier() const {
    size_t pos = lenAttribStr.find_first_of("->");
    if (pos != std::string::npos) {
        return lenAttribStr.substr(0, pos);
    }
    return lenAttribStr;
}

std::string vkgen::VariableData::getLenAttribRhs() const {
    size_t pos = lenAttribStr.find_first_of("->");
    if (pos != std::string::npos) {
        return lenAttribStr.substr(pos + 2);
    }
    return lenAttribStr;
}

std::string vkgen::VariableData::namespaceString(const Generator &gen, bool forceNamespace) const {
    std::string_view ns_str = gen.getNamespace((forceNamespace && ns == Namespace::RAII) ? Namespace::VK : ns);
    if (ns_str.empty()) {
        return "";
    }
    return std::string{ ns_str } + "::";
}

bool vkgen::VariableData::isLenAttribIndirect() const {
    auto pos      = lenAttribStr.find_first_of("->");
    bool indirect = pos != std::string::npos;
    return indirect;
}

void vkgen::VariableData::setNamespace(Namespace value) {
    ns = value;
}

void vkgen::VariableData::toRAII() {
    ns = Namespace::RAII;
    if (!isOutParam()) {
        convertToReference();
        setConst(true);
    }
}

vkgen::Namespace vkgen::VariableData::getNamespace() const {
    return ns;
}

void vkgen::VariableData::convertToCpp() {
    const auto &t  = original.type();
    const auto &id = original.identifier();
    setType(strStripVk(t));
    setIdentifier(strStripVk(id));
}

void vkgen::VariableData::convertToStructChain() {
    structChain = true;
}

bool vkgen::VariableData::isNullTerminated() const {
    return nullTerminated;
}

void vkgen::VariableData::convertToArrayProxy() {
    specialType = TYPE_ARRAY_PROXY;
    removeLastAsterisk();
    setReference(false);
}

void vkgen::VariableData::convertToConstReference() {
    if (specialType == TYPE_ARRAY_PROXY) {
        specialType = TYPE_DEFAULT;
    } else {
        removeLastAsterisk();
    }
    if (!isConst()) {
        setConst(true);
    }
    setReference(true);
}

void vkgen::VariableData::bindLengthVar(VariableData &var, bool noArray) {
    lenghtVar = &var;
//    if (var.type() == "size_t" && type() == "void") {
//        std::cout << "  " << identifier() << "\n";
//        if (identifier() != "pData") {
//            return;
//        }
//    }
    if (noArray) {
        return;
    }
    flags |= Flags::ARRAY;
    if (isConst()) {
        flags |= Flags::ARRAY_IN;
    } else {
        flags |= Flags::ARRAY_OUT;
    }
}

void vkgen::VariableData::bindArrayVar(VariableData *var) {
    for (const auto &v : arrayVars) {
        if (v == var) {
            // already in array
            std::cout << "Warning: bindArrayVar(): duplicate" << '\n';
            return;
        }
    }
    arrayVars.push_back(var);
}

vkgen::VariableData *vkgen::VariableData::getLengthVar() const {
    return lenghtVar;
}

const std::vector<vkgen::VariableData *> &vkgen::VariableData::getArrayVars() const {
    return arrayVars;
}

void vkgen::VariableData::convertToReturn() {
    specialType = TYPE_RETURN;
    ignoreProto = true;
}

void vkgen::VariableData::convertToReference() {
    int cfrom;
    int cto;
    std::tie(cfrom, cto) = countPointers(original.suffix(), suffix());
    if (cfrom >= cto) {
        removeLastAsterisk();
    }
    setReference(true);
}

void vkgen::VariableData::convertToPointer() {
    if (!isPointer()) {
        fields[SUFFIX] = fields[SUFFIX] + "*";
    }
    setReference(false);
}

void vkgen::VariableData::convertToOptionalWrapper() {
    setReference(false);
    specialType = TYPE_OPTIONAL;
}

void vkgen::VariableData::convertToStdVector(const Generator &gen) {
    specialType = TYPE_VECTOR;
    auto &s     = fields[PREFIX];
    auto  pos   = s.find("const");
    if (pos != std::string::npos) {
        s.erase(pos, 5);
    }

//    if (!optionalTemplate.empty()) {
//        optionalTemplate += ", ";
//    }
    allocatorTemplate.prefix = "typename ";
    allocatorTemplate.type = strFirstUpper(type()) + "Allocator";
    std::string assignment = " = std::allocator<";
    if (ns == Namespace::VK) {
        assignment += gen.m_ns;
        assignment += "::";
    }
    assignment += type();
    assignment += ">";
    allocatorTemplate.assignment = std::move(assignment);
}

bool vkgen::VariableData::removeLastAsterisk() {
    std::string &suffix = fields[SUFFIX];
    if (suffix.ends_with("*")) {
        suffix.erase(suffix.size() - 1);  // removes * at end
        return true;
    }
    return false;
}

void vkgen::VariableData::setConst(bool enabled) {
    if (enabled) {
        if (fields[PREFIX] != "const ") {
            fields[PREFIX] = "const ";
        }
    } else {
        if (fields[PREFIX] == "const ") {
            fields[PREFIX] = "";
        }
    }
}

vkgen::VariableData::Flags vkgen::VariableData::getFlags() const {
    return flags;
}

void vkgen::VariableData::setFlag(Flags flag, bool enabled) {
    if (enabled) {
        flags |= flag;
    } else {
        flags &= ~EnumFlag<Flags>{ flag };
    }
}

// bool vkgen::VariableData::isHandle() const {
//     return hasFlag(flags, Flags::HANDLE);
// }

bool vkgen::VariableData::isArray() const {
    return hasFlag(flags, Flags::ARRAY);
}

bool vkgen::VariableData::isArrayIn() const {
    return hasFlag(flags, Flags::ARRAY_IN);
}

bool vkgen::VariableData::isArrayOut() const {
    return hasFlag(flags, Flags::ARRAY_OUT);
}

bool vkgen::VariableData::isOutParam() const {
    return hasFlag(flags, Flags::OUT);
}

std::string vkgen::VariableData::flagstr() const {
    std::string s = "(" + metaTypeString() + ")|";
    if (hasFlag(flags, Flags::OUT)) {
        s += "OUT|";
    }
    if (hasFlag(flags, Flags::ARRAY)) {
        s += "ARRAY|";
    }
    if (hasFlag(flags, Flags::ARRAY_IN)) {
        s += "ARRAY_IN|";
    }
    if (hasFlag(flags, Flags::ARRAY_OUT)) {
        s += "ARRAY_OUT|";
    }
    if (hasFlag(flags, Flags::CLASS_VAR_VK)) {
        s += "CLASS_VAR_VK|";
    }
    if (hasFlag(flags, Flags::CLASS_VAR_UNIQUE)) {
        s += "CLASS_VAR_UNIQUE|";
    }
    if (hasFlag(flags, Flags::CLASS_VAR_RAII)) {
        s += "CLASS_VAR_RAII|";
    }
    if (!s.empty()) {
        s.erase(s.size() - 1);
    }
    return s;
}

std::string vkgen::VariableData::toClassVar(const Generator &gen) const {
    return fullType(gen) + " " + identifier() + getAssignment() + ";\n";
}

std::string vkgen::VariableData::toArgument(const Generator &gen, bool useOriginal) const {
    if (!altPFN.empty()) {
        return altPFN;
    }
    switch (specialType) {
        case TYPE_VECTOR:
        case TYPE_TEMPL_VECTOR:
        case TYPE_VK_VECTOR:
        case TYPE_EXP_ARRAY:
        case TYPE_ARRAY_PROXY:
        case TYPE_ARRAY_PROXY_NO_TEMPORARIES: return toArgumentArrayProxy(gen);
        default: return toArgumentDefault(gen, useOriginal);
    }
}

std::string vkgen::VariableData::toVariable(const Generator &gen, const VariableData &dst, bool useOriginal) const {
    // TODO check array/vector match
    std::string s = useOriginal ? dst.original.suffix() : dst.suffix();
    std::string t = useOriginal ? dst.original.type() : dst.type();

    std::string id;
    if (specialType == TYPE_OPTIONAL) {
        id = identifierAsArgument(gen);
    } else {
        if (isHandle() && ns == Namespace::RAII && (dst.ns != Namespace::RAII || useOriginal)) {
            id = "*";  // deference raii object
        }
        id += matchTypePointers(suffix(), s) + identifier();
    }
    std::string out;
    if (t != type()) {
        std::string cast = "static_cast";
        if (strContains(s, "*")) {
            cast = gen.m_cast;
        }
        std::string f = useOriginal ? dst.originalFullType() : dst.fullType(gen);

        out = cast + "<" + f + ">(" + id + ")";
    } else {
        out = id;
    }

    return out;  //"/* ${" + fullType() + " -> " + originalFullType() + "} */";
}

std::string vkgen::VariableData::toStructArgumentWithAssignment(const Generator &gen) const {
    std::string out;
    if (hasArrayLength()) {
        std::string atype = "std::array";
        switch (arrayAttrib) {
            case ArraySize::NONE: break;
            case ArraySize::DIM_1D: out += vkgen::format("{0}<{1}, {2}>", atype, fields[TYPE], arraySizes[0]); break;
            case ArraySize::DIM_2D: out += vkgen::format("{0}<{0}<{1}, {2}>, {3}>", atype, fields[TYPE], arraySizes[1], arraySizes[0]); break;
        }
        out += " const &" + fields[IDENTIFIER];
    } else {
        out = toString(gen);
    }
    if (!_assignment.empty()) {
        out += _assignment;
    }
    return out;
}

std::string vkgen::VariableData::createVectorType(const std::string_view vectorType, const std::string &type) const {
    std::string output = std::string(vectorType);
    output += "<";
    output += type;
    if (!sizeTemplate.type.empty()) {
        output += ", ";
        output += sizeTemplate.type;
    }
    if (!allocatorTemplate.type.empty()) {
        output += ", ";
        output += allocatorTemplate.type;
    }
    output += ">";
    return output;
}

std::string vkgen::VariableData::fullType(const Generator &gen, bool forceNamespace) const {
    std::string type = fields[PREFIX];
    if (!fields[TYPE].starts_with("Vk")) {
        type += namespaceString(gen, forceNamespace);
    }
    // type += "/*" + std::to_string((int)ns) + "*/";
    type += fields[TYPE];
    type += fields[SUFFIX];
    switch (specialType) {
        case TYPE_ARRAY:
            {
                std::string out;
                std::string atype = "std::array";
                switch (arrayAttrib) {
                    case ArraySize::NONE: break;
                    case ArraySize::DIM_1D: out += vkgen::format("{}<{}, {}>", atype, fields[TYPE], arraySizes[0]); break;
                    case ArraySize::DIM_2D: out += vkgen::format("{0}<{0}<{1}, {2}>, {3}>", atype, fields[TYPE], arraySizes[1], arraySizes[0]); break;
                }
                out += " const &";
                return out;
            }
        case TYPE_ARRAY_PROXY:
            if (gen.getConfig().gen.proxyPassByCopy) {
                return "const ArrayProxy<" + type + "> ";
            }
            else {
                return "ArrayProxy<" + type + "> const &";
            }
        case TYPE_ARRAY_PROXY_NO_TEMPORARIES: return "ArrayProxyNoTemporaries<" + type + "> const &";
        case TYPE_EXP_ARRAY: return createVectorType("std::array", type);
        case TYPE_VECTOR: return createVectorType("std::vector", type);
        case TYPE_VK_VECTOR: return createVectorType("Vector", type);
        case TYPE_OPTIONAL: return "Optional<" + type + ">";
        default: return type;
    }
    type += nameSuffix;
}

std::string vkgen::VariableData::toString(const Generator &gen) const {
    std::string out = fullType(gen);
    if (!out.ends_with(" ")) {
        out += " ";
    }
    out += fields[IDENTIFIER];
    if (specialType != TYPE_ARRAY) {
        out += optionalArraySuffix();
    }
    out += nameSuffix;
    return out;
}

std::string vkgen::VariableData::toStructString(const Generator &gen,  bool cstyle) const {
    if (cstyle) {
        return toString(gen);
    }
    const auto &id = fields[IDENTIFIER];
    switch (arrayAttrib) {
        case ArraySize::DIM_1D: return vkgen::format("{}::ArrayWrapper1D<{}, {}> {}", gen.m_ns, fields[TYPE], arraySizes[0], id);
        case ArraySize::DIM_2D: return vkgen::format("{}::ArrayWrapper2D<{}, {}, {}> {}", gen.m_ns, fields[TYPE], arraySizes[0], arraySizes[1], id);
        case ArraySize::NONE: return toString(gen);
    }
    return "";
}

std::string vkgen::VariableData::declaration(const Generator &gen) const {
    std::string out = fullType(gen);
    if (!out.ends_with(" ")) {
        out += " ";
    }
    out += fields[IDENTIFIER];
    out += optionalArraySuffix();
    return out;
}



std::string vkgen::VariableData::toStringWithAssignment(const Generator &gen) const {
    std::string out = toString(gen);
    if (!_assignment.empty()) {
        out += _assignment;
    }
    return out;
}

std::string vkgen::VariableData::toStructStringWithAssignment(const Generator &gen, bool cstyle) const {
    std::string out = toStructString(gen, cstyle);
    if (!_assignment.empty()) {
        out += _assignment;
    }
    return out;
}

std::string vkgen::VariableData::originalToString() const {
    std::string out = originalFullType();
    if (!out.ends_with(" ")) {
        out += " ";
    }
    out += original.identifier();
    out += optionalArraySuffix();
    return out;
}

//void vkgen::VariableData::setTemplateDataType(const std::string &str) {
//    templateDataTypeStr = str;
//}
//
//const std::string &vkgen::VariableData::getTemplateDataType() const {
//    return templateDataTypeStr;
//}

void vkgen::VariableData::evalFlags() {
    if (isPointer() && !isConst() && arrayVars.empty()) {
        flags |= Flags::OUT;
    }
}

std::string vkgen::VariableData::optionalArraySuffix() const {
    switch (arrayAttrib) {
        case ArraySize::NONE: return "";
        case ArraySize::DIM_1D: return "[" + arraySizes[0] + "]";
        case ArraySize::DIM_2D: return "[" + arraySizes[0] + "][" + arraySizes[1] + "]";
    }
    return "";
}

std::string vkgen::VariableData::toArgumentArrayProxy(const Generator &gen) const {
    std::string out = fields[IDENTIFIER] + ".data()";
    if (fields[TYPE] == original.type()) {
        return out;
    }
    return gen.m_cast + "<" + originalFullType() + ">(" + out + ")";
}

std::string vkgen::VariableData::createCast(const Generator &gen, std::string from) const {
    std::string cast = "static_cast";
    if ((strContains(original.suffix(), "*") || hasArrayLength())) {
        cast = gen.m_cast;
    }
    return cast + "<" + originalFullType() + (hasArrayLength() ? "*" : "") + ">(" + from + ")";
}

std::string vkgen::VariableData::getReturnType(const Generator &gen, bool forceNamespace) const {
    std::string type = fullType(gen, forceNamespace);
    return type;
};

void vkgen::VariableData::createLocalReferenceVar(const Generator &gen, const std::string &indent, const std::string &assignment, std::string &output) {
    output += indent + getReturnType(gen, true) + " &" + identifier() + " = " + assignment + ";\n";
    localVar = true;
}

std::string vkgen::VariableData::generateVectorReserve(const Generator &gen, const std::string &indent) {
    std::string output;
    if (isArrayOut()) {
        const auto &init = getLocalInit();
        if (!init.empty()) {
            output = indent;
            output += identifier();
            output += ".reserve( ";
            output += init;
            output += " );\n";
        }
    }
    return output;
}

void vkgen::VariableData::createLocalVar(
  const Generator &gen, const std::string &indent, const std::string &dbg, std::string &output, const std::string &assignment) {
    output += indent + getReturnType(gen, true) + " " + identifier();
    if (isArrayOut()) {
        const auto &init = getLocalInit();
        if (!init.empty()) {
            output += "( " + init + " )";
        }
    }
    if (!assignment.empty()) {
        output += " = ";
        output += assignment;
    }
    output += ";" + dbg + "\n";
    localVar = true;
}

std::string vkgen::VariableData::toArgumentDefault(const Generator &gen, bool useOriginal) const {
    if (!arrayVars.empty()) {
        const auto &var = arrayVars[0];
        if (var->isArrayIn() && !var->isLenAttribIndirect() && (var->specialType == TYPE_ARRAY_PROXY || var->specialType == TYPE_ARRAY_PROXY_NO_TEMPORARIES)) {
            std::string size = var->identifier() + ".size()";
            const auto &str = var->dataTemplate.type;
            if (!str.empty()) {
                size += " * sizeof(" + str + ")";
            }
            return size;
        }
    }
    std::string id   = identifierAsArgument(gen);
    bool        same = fields[TYPE] == original.type();
    if ((same && specialType != TYPE_OPTIONAL) || useOriginal) {
        return id;
    }
    return createCast(gen, id);
}

std::string vkgen::VariableData::toArrayProxySize() const {
    std::string s = fields[IDENTIFIER] + ".size()";
    if (original.type() == "void") {
        if (dataTemplate.type.empty()) {
            std::cerr << "Warning: ArrayProxy " << fields[IDENTIFIER] << " has no template set, but is required" << '\n';
        }
        s += " * sizeof(" + dataTemplate.type + ")";
    }
    return s;
}

std::string vkgen::VariableData::toArrayProxyData() const {
    return fields[IDENTIFIER] + ".data()";
}

std::pair<std::string, std::string> vkgen::VariableData::toArrayProxyRhs() const {
    if (specialType != TYPE_ARRAY_PROXY && specialType != TYPE_ARRAY_PROXY_NO_TEMPORARIES) {
        return std::make_pair("", "");
    }
    return std::make_pair(toArrayProxySize(), toArrayProxyData());
}

std::string vkgen::VariableData::identifierAsArgument(const Generator &gen) const {
    const std::string &suf = fields[SUFFIX];
    const std::string &id  = fields[IDENTIFIER];
    if (specialType == TYPE_OPTIONAL) {
        std::string type = fields[PREFIX];
        type += namespaceString(gen);
        type += fields[TYPE] + fields[SUFFIX];
        return "static_cast<" + type + "*>(" + id + ")";
    }

    if (ns == Namespace::RAII) {
        return "*" + id;
    }
    return matchTypePointers(suf, original.suffix()) + id;
}

vkgen::XMLVariableParser::XMLVariableParser(VariableData &data, tinyxml2::XMLElement *element) : data(data) {
    element->Accept(this);
}

bool vkgen::XMLVariableParser::Visit(const tinyxml2::XMLText &text) {
    std::string_view tag = text.Parent()->Value();
    std::string      value;
    if (auto v = text.Value(); v) {
        value = v;
    }
    if (tag == "type") {  // text is type field
        state = TYPE;
    } else if (tag == "name") {  // text is name field
        state = IDENTIFIER;
    } else if (state == TYPE) {  // text after type is suffix (optional)
        state = SUFFIX;
    } else if (state == IDENTIFIER) {
        if (value == "[") {  // text after name is array size if [
            state = BRACKET_LEFT;
        } else if (value.starts_with("[") && value.ends_with("]")) {
            std::regex           rgx("\\[[0-9]+\\]");
            std::sregex_iterator it = std::sregex_iterator(value.begin(), value.end(), rgx);
            std::string          suffix;
            while (it != std::sregex_iterator()) {
                auto        temp  = it;
                std::smatch m     = *temp;
                std::string match = m.str();
                match             = match.substr(1, match.size() - 2);
                // std::cout << "  " << match << " at position " << m.position() << '\n';
                data.addArrayLength(match);
                ++it;
                if (it == std::sregex_iterator()) {
                    suffix = temp->suffix();
                    break;
                }
            }
            if (!suffix.empty()) {
                std::cerr << "[visit] unprocessed suffix: " << suffix << '\n';
            }

            state = DONE;
            return false;
        } else {
            if (value == "][") {  // multi dimensional array
                state = BRACKET_LEFT;
            } else if (!value.empty() && tag != "comment") {
                data.setNameSuffix(value);
            }
            state = DONE;
            return false;
        }
    } else if (state == BRACKET_LEFT) {  // text after [ is <enum>SIZE</enum>
        state = ARRAY_LENGTH;
        data.addArrayLength(value);
    } else if (state == ARRAY_LENGTH) {
        state = DONE;
        return false;
    }

    if (state < 4) {  // append if state index is in range
        data.set(state, value);
    }
    return true;
}

void vkgen::VariableData::trim() {
    std::string &suffix = fields[SUFFIX];
    const auto   it     = suffix.find_last_not_of(' ');
    if (it != std::string::npos) {
        suffix.erase(it + 1);  // removes trailing space
    }
}

vkgen::XMLDefineParser::XMLDefineParser(tinyxml2::XMLElement *element) {
    parse(element);
}

void vkgen::XMLDefineParser::parse(tinyxml2::XMLElement *element) {
    state = DEFINE;
    element->Accept(this);
    trim();
}

void vkgen::XMLDefineParser::trim() {
    const auto it = value.find_first_not_of(' ');
    if (it != std::string::npos) {
        value.erase(0, it);
    }
}

bool vkgen::XMLDefineParser::Visit(const tinyxml2::XMLText &text) {
    std::string_view tag   = text.Parent()->Value();
    std::string_view value = text.Value();
    if (tag == "name") {
        state = NAME;
    }

    switch (state) {
        case DEFINE: break;
        case NAME:
            name  = value;
            state = VALUE;
            break;
        case VALUE:
            this->value = value;
            state       = DONE;
            break;
        default: return false;
    }
    return true;
}

vkgen::XMLTextParser::XMLTextParser(xml::Element element) {
    prev = element.toElement();
    root = element.toElement();
    auto name = element.optional("name");
    if (name) {
        fields["name"] = name.value();
    }
    element->Accept(this);
}

std::string& vkgen::XMLTextParser::operator[](const std::string &field) {
    auto it = fields.find(field);
    if (it == fields.end()) {
        throw std::runtime_error("Parse error: Missing XML node: " + field + "\n" + text + "\n");
    }
    return it->second;
}

bool vkgen::XMLTextParser::Visit(const tinyxml2::XMLText &text) {
    const auto *node = text.Parent();
    std::string_view tag   = node->Value();
    std::string_view value = xml::value(text);
    // std::cout << "P: " << text.ToText() << ", " << text.Parent() << ", " << prev << ", <" << tag << ">: " << value << "\n";
    if (prev != root && node != root && prev != node) {
        this->text += ' ';
    }
    if (node != root) {
        // std::cout << "  <" << tag << ">: " << value << "\n";
        fields[std::string(tag)] = value;
    }
    this->text += value;
    prev = node;
    return true;
}

/*
vkgen::XMLBaseTypeParser::XMLBaseTypeParser(tinyxml2::XMLElement *element) {
    element->Accept(this);
}

bool vkgen::XMLBaseTypeParser::Visit(const tinyxml2::XMLText &text) {
    std::string_view tag   = text.Parent()->Value();
    std::string_view value = xml::value(text);

    std::cout << "P: " << text.Parent() << ", " << prev << "\n";
    // std::cout << tag << "<<" << value << ">>\n";
    if (tag == "name") {
//        if (!this->text.starts_with(' ')) {
//            this->text += ' ';
//        }
        this->name = value;
    }
    this->text += value;
    return true;
}
*/
