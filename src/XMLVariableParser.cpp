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

#include "XMLVariableParser.hpp"

#include "Generator.hpp"

void VariableFields::set(size_t index, const std::string &str) {
    if (index >= N) {
        std::cerr << "VariableFields set index out of bounds" << '\n';
        return;
    }
    fields[index] = str;
}

VariableData::VariableData(const Generator &gen, tinyxml2::XMLElement *element) : gen(gen) {

    XMLVariableParser p(*this, element);

    const char *len = element->Attribute("len");
    const char *altlen = element->Attribute("altlen");
    const char *optional = element->Attribute("optional");
    if (len) {
        lenExpressions = split(std::string(len), ",");
        for (const auto &str : lenExpressions) {
            if (str.empty() || std::isdigit(str[0])) {
                continue;
            }
            if (str == "null-terminated") {
                nullTerminated = true;
            }
            else {
                if (!lenAttribStr.empty()) {
                    std::cout << "Warn: len attrib currently set (is " << lenAttribStr << ", new: " << str << "). xml: " << len << '\n';
                }
                lenAttribStr = str;
            }
        }
    }
    if (altlen) {
        altlenAttribStr = altlen;
    }
    if (optional) {
        this->optional = std::string_view{optional} == "true";
    }

    trim();
    original = *reinterpret_cast<VariableFields*>(this);

    convertToCpp(gen);
    // evalFlags(gen);
    if (gen.isHandle(original.type())) {
        flags |= Flags::HANDLE;
    }

    // save();
}

VariableData::VariableData(const Generator &gen, Type type) : gen(gen) {
    specialType = type;    
    ignoreFlag = specialType == TYPE_INVALID;    
    ignoreProto = specialType == TYPE_INVALID;

    // std::cout << "var ctor1 " << this << '\n';
}

VariableData::VariableData(const Generator &gen, const VariableDataInfo &info) : gen(gen) {
    if (!info.stdtype.empty()) {
        original.setType(info.stdtype);
        setFullType(info.prefix, String(info.stdtype, false), info.suffix);
    }
    else {
        original.setType(info.vktype);
        setFullType(info.prefix, String(info.vktype, true), info.suffix);
    }
    setIdentifier(info.identifier);
    setAssignment(info.assigment);
    setNamespace(info.ns);
    setFlag(info.flag, true);
    setSpecialType(info.specialType);

    // std::cout << "var ctor2 " << this << '\n';
    // save();
}

VariableData::VariableData(const Generator &gen, const String &type) : VariableData(gen, type, strFirstLower(type))
{}

VariableData::VariableData(const Generator &gen, const String &type, const std::string &id) : VariableData(gen, TYPE_DEFAULT) {
    setIdentifier(id);
    original.setFullType("", type.original, " *");
    setFullType("", type, " *");
    convertToReference();
}

std::string VariableData::getLenAttribIdentifier() const {
    size_t pos = lenAttribStr.find_first_of("->");
    if (pos != std::string::npos) {        
        return lenAttribStr.substr(0, pos);
    }
    return lenAttribStr;
}

std::string VariableData::getLenAttribRhs() const {
    size_t pos = lenAttribStr.find_first_of("->");
    if (pos != std::string::npos) {
        return lenAttribStr.substr(pos + 2);
    }
    return lenAttribStr;
}

std::string VariableData::namespaceString(bool forceNamespace) const {
    if (forceNamespace && ns == Namespace::RAII) {
        return gen.getNamespace(Namespace::VK);
    }
    return gen.getNamespace(ns);
}

bool VariableData::isLenAttribIndirect() const {
    auto pos = lenAttribStr.find_first_of("->");
    bool indirect = pos != std::string::npos;
    return indirect;
}

void VariableData::setNamespace(Namespace value) {
    ns = value;
}

void VariableData::toRAII() {
    ns = Namespace::RAII;
    if (!isOutParam()) {
        convertToReference();
        setConst(true);
    }
}

Namespace VariableData::getNamespace() const {
    return ns;
}

void VariableData::convertToCpp(const Generator &gen) {

    const auto &t = original.type();
    const auto &id = original.identifier();
    if (gen.isInNamespace(t)) {
        ns = Namespace::VK;        
    }
    setType(strStripVk(t));
    setIdentifier(strStripVk(id));
}

bool VariableData::isNullTerminated() const {
    return nullTerminated;
}

void VariableData::convertToArrayProxy() {
    specialType = TYPE_ARRAY_PROXY;
    removeLastAsterisk();
    setReference(false);
}

void VariableData::convertToConstReference() {
    if (specialType == TYPE_ARRAY_PROXY) {
        specialType = TYPE_DEFAULT;
    }
    else {
        removeLastAsterisk();
    }
    if (!isConst()) {
        setConst(true);
    }
    setReference(true);
}

void VariableData::bindLengthVar(VariableData &var) {
    lenghtVar = &var;

    flags |= Flags::ARRAY;
    if (isConst()) {
        flags |= Flags::ARRAY_IN;
    } else {
        flags |= Flags::ARRAY_OUT;
    }
}

void VariableData::bindArrayVar(VariableData *var) {
    for (const auto &v : arrayVars) {
        if (v == var) {
            // already in array
            std::cout << "Warning: bindArrayVar(): duplicate" << '\n';
            return;
        }
    }
    arrayVars.push_back(var);
}

VariableData* VariableData::getLengthVar() const {
    return lenghtVar;
}

const std::vector<VariableData*>& VariableData::getArrayVars() const {
    return arrayVars;
}

void VariableData::convertToReturn() {
    specialType = TYPE_RETURN;
    ignoreProto = true;
}

void VariableData::convertToReference() {
    int cfrom;
    int cto;
    std::tie(cfrom, cto) = countPointers(original.suffix(), suffix());
    if (cfrom >= cto) {
        removeLastAsterisk();
    }
    setReference(true);
}

void VariableData::convertToPointer() {
    if (!isPointer()) {
        fields[SUFFIX] = fields[SUFFIX] + "*";
    }
    setReference(false);
}

void VariableData::convertToOptionalWrapper() {
    setReference(false);
    specialType = TYPE_OPTIONAL;
}

void VariableData::convertToStdVector() {
    specialType = TYPE_VECTOR;
    auto &s = fields[PREFIX];
    auto pos = s.find("const");
    if (pos != std::string::npos) {
        s.erase(pos, 5);
    }
}

bool VariableData::removeLastAsterisk() {
    std::string &suffix = fields[SUFFIX];
    if (suffix.ends_with("*")) {
        suffix.erase(suffix.size() - 1); // removes * at end
        return true;
    }
    return false;
}

void VariableData::setConst(bool enabled) {
    if (enabled) {
        if (fields[PREFIX] != "const ") {
            fields[PREFIX] = "const ";
        }
    }
    else {
        if (fields[PREFIX] == "const ") {
            fields[PREFIX] = "";
        }
    }
}

VariableData::Flags VariableData::getFlags() const {
    return flags;
}

void VariableData::setFlag(Flags flag, bool enabled) {
    if (enabled) {
        flags |= flag;
    } else {
        flags &= ~EnumFlag<Flags>{flag};
    }
}

bool VariableData::isHandle() const {
    return hasFlag(flags, Flags::HANDLE);
}

bool VariableData::isArray() const {
    return hasFlag(flags, Flags::ARRAY);
}

bool VariableData::isArrayIn() const {
    return hasFlag(flags, Flags::ARRAY_IN);
}

bool VariableData::isArrayOut() const {
    return hasFlag(flags, Flags::ARRAY_OUT);
}

bool VariableData::isOutParam() const {
    return hasFlag(flags, Flags::OUT);
}

std::string VariableData::flagstr() const {
    std::string s;
    if (hasFlag(flags, Flags::OUT)) {
        s += "OUT|";
    }
    if (hasFlag(flags, Flags::HANDLE)) {
        s += "HANDLE|";
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

std::string VariableData::toClassVar() const {
    return fullType() + " " + identifier() + getAssignment() + ";\n";
}

std::string VariableData::toArgument(bool useOriginal) const {
    if (!altPFN.empty()) {
        return altPFN;
    }
    switch (specialType) {
    case TYPE_VECTOR:
    case TYPE_ARRAY_PROXY:
    case TYPE_ARRAY_PROXY_NO_TEMPORARIES:
        return toArgumentArrayProxy();
    default:
        return toArgumentDefault(useOriginal);
    }
}

std::string VariableData::toVariable(const VariableData &dst, bool useOriginal) const {
    // TODO check array/vector match        
    std::string s = useOriginal? dst.original.suffix() : dst.suffix();
    std::string t = useOriginal? dst.original.type() : dst.type();

    std::string id;
    if (specialType == TYPE_OPTIONAL) {
        id = identifierAsArgument();
    }
    else {
        if (isHandle() && ns == Namespace::RAII && (dst.ns != Namespace::RAII || useOriginal)) {
            id = "*"; // deference raii object
        }
        id += matchTypePointers(suffix(), s) + identifier();
    }
    std::string out;
    if (t != type()) {
        std::string cast = "static_cast";
        if (strContains(s, "*")) {
            cast = "std::bit_cast";
        }
        std::string f = useOriginal ? dst.originalFullType() : dst.fullType();

        out = cast + "<" + f + ">(" + id + ")";
    }
    else {
        out = id;
    }

    return out; //"/* ${" + fullType() + " -> " + originalFullType() + "} */";
}

std::string VariableData::toStructArgumentWithAssignment() const {
    std::string out;
    if (hasArrayLength()) {
        std::string atype = "std::array";
          switch (arrayAttrib) {
            case ArraySize::NONE:
                break;
            case ArraySize::DIM_1D:
                out += gen.format("{0}<{1}, {2}>", atype, fields[TYPE], arraySizes[0]);
                break;
            case ArraySize::DIM_2D:
                out += gen.format("{0}<{0}<{1}, {2}>, {3}>", atype, fields[TYPE], arraySizes[1], arraySizes[0]);
                break;
        }
        out += " const &" + fields[IDENTIFIER];
    }
    else {
        out = toString();
    }
    if (!_assignment.empty()) {
        out += _assignment;
    }
    return out;
}

std::string VariableData::fullType(bool forceNamespace) const {
    std::string type = fields[PREFIX];
    type += namespaceString(forceNamespace);
    type += fields[TYPE];
    type += fields[SUFFIX];
    switch (specialType) {
    case TYPE_ARRAY: {
        std::string out;
        std::string atype = "std::array";
        switch (arrayAttrib) {
            case ArraySize::NONE:
                break;
            case ArraySize::DIM_1D:
                out += gen.format("{0}<{1}, {2}>", atype, fields[TYPE], arraySizes[0]);
                break;
            case ArraySize::DIM_2D:
                out += gen.format("{0}<{0}<{1}, {2}>, {3}>", atype, fields[TYPE], arraySizes[1], arraySizes[0]);
                break;
        }
        out += " const &";
        return out;
        }
    case TYPE_ARRAY_PROXY:    
        return "ArrayProxy<" + type + "> const &";
    case TYPE_ARRAY_PROXY_NO_TEMPORARIES:
        return "ArrayProxyNoTemporaries<" + type + "> const &";
    case TYPE_VECTOR:
        return "std::vector<" + type + ">";
    case TYPE_OPTIONAL:
        return "Optional<" + type + ">";
    default:
        return type;
    }
}

std::string VariableData::toString() const {
    std::string out = fullType();
    if (!out.ends_with(" ")) {
        out += " ";
    }
    out += fields[IDENTIFIER];
    if (specialType != TYPE_ARRAY) {
        out += optionalArraySuffix();
    }
    return out;
}

std::string VariableData::toStructString() const {
    const auto &id = fields[IDENTIFIER];
    switch (arrayAttrib) {
        case ArraySize::DIM_1D:
            return gen.format("{NAMESPACE}::ArrayWrapper1D<{0}, {1}> {2}", fields[TYPE], arraySizes[0], id);
        case ArraySize::DIM_2D:
            return gen.format("{NAMESPACE}::ArrayWrapper2D<{0}, {1}, {2}> {3}", fields[TYPE], arraySizes[0], arraySizes[1], id);
        case ArraySize::NONE:
            return toString();
    }
    return "";
}

std::string VariableData::declaration() const {
    std::string out = fullType();
    if (!out.ends_with(" ")) {
        out += " ";
    }
    out += fields[IDENTIFIER];
    out += optionalArraySuffix();
    return out;
}

std::string VariableData::toStringWithAssignment() const {
    std::string out = toString();
    if (!_assignment.empty()) {
        out += _assignment;
    }
    return out;
}

std::string VariableData::toStructStringWithAssignment() const {
    std::string out = toStructString();
    if (!_assignment.empty()) {
        out += _assignment;
    }
    return out;
}

std::string VariableData::originalToString() const {
    std::string out = originalFullType();
    if (!out.ends_with(" ")) {
        out += " ";
    }
    out += original.identifier();
    out += optionalArraySuffix();
    return out;
}

void VariableData::setTemplate(const std::string &str) {
    optionalTemplate = str;
}

void VariableData::setTemplateAssignment(const std::string &str) {
    optionalTemplateAssignment = str;
}

std::string VariableData::getTemplate() const {
    return optionalTemplate;
}

std::string VariableData::getTemplateAssignment() const {
    return optionalTemplateAssignment;
}

void VariableData::evalFlags() {
    if (isPointer() && !isConst() && arrayVars.empty()) {
        flags |= Flags::OUT;
    }
}

std::string VariableData::optionalArraySuffix() const {

    switch (arrayAttrib) {
        case ArraySize::NONE:
            return "";
        case ArraySize::DIM_1D:
            return "[" + arraySizes[0] + "]";
        case ArraySize::DIM_2D:
            return "[" + arraySizes[0] + "][" + arraySizes[1] + "]";
    }
    return "";
}

std::string VariableData::toArgumentArrayProxy() const {
    std::string out = fields[IDENTIFIER] + ".data()";
    if (fields[TYPE] == original.type()) {
        return out;
    }
    return "std::bit_cast<" + originalFullType() + ">(" + out + ")";
}

std::string VariableData::createCast(std::string from) const {
    std::string cast = "static_cast";
    if ((strContains(original.suffix(), "*") || hasArrayLength())) {
        cast = "std::bit_cast";
    }    
    return cast + "<" + originalFullType() + (hasArrayLength() ? "*" : "") +
           ">(" + from + ")";
}

std::string VariableData::toArgumentDefault(bool useOriginal) const {
    if (!arrayVars.empty()) {
        const auto &var = arrayVars[0];
        if (var->isArrayIn() && !var->isLenAttribIndirect() && (var->specialType == TYPE_ARRAY_PROXY ||  var->specialType == TYPE_ARRAY_PROXY_NO_TEMPORARIES)) {
            std::string size = var->identifier() + ".size()";
            if (const auto &str = var->getTemplate(); !str.empty()) {
                size += " * sizeof(" + str+ ")";
            }
            return size;
        }
    }
    std::string id = identifierAsArgument();
    bool same = fields[TYPE] == original.type();
    if ((same && specialType != TYPE_OPTIONAL) || useOriginal) {
        return id;
    }
    return createCast(id);
}

std::string VariableData::toArrayProxySize() const {
    std::string s = fields[IDENTIFIER] + ".size()";
    if (original.type() == "void") {
        if (optionalTemplate.empty()) {
            std::cerr << "Warning: ArrayProxy " << fields[IDENTIFIER] << " has no template set, but is required" << '\n';
        }
        s += " * sizeof(" + optionalTemplate + ")";
    }
    return s;
}

std::string VariableData::toArrayProxyData() const {
    return fields[IDENTIFIER] + ".data()";
}

std::pair<std::string, std::string> VariableData::toArrayProxyRhs() const {
    if (specialType != TYPE_ARRAY_PROXY && specialType != TYPE_ARRAY_PROXY_NO_TEMPORARIES) {
        return std::make_pair("", "");
    }    
    return std::make_pair(toArrayProxySize(), toArrayProxyData());
}

std::string VariableData::identifierAsArgument() const {
    const std::string &suf = fields[SUFFIX];
    const std::string &id = fields[IDENTIFIER];
    if (specialType == TYPE_OPTIONAL) {
        std::string type = fields[PREFIX];
        type += namespaceString();
        type += fields[TYPE] + fields[SUFFIX];
        return "static_cast<" + type + "*>(" + id + ")";
    }

    if (ns == Namespace::RAII) {
        return "*" + id;
    }
    return matchTypePointers(suf, original.suffix()) + id;
}

XMLVariableParser::XMLVariableParser(VariableData &data, tinyxml2::XMLElement *element)
    : data(data)
{
    element->Accept(this);
}

bool XMLVariableParser::Visit(const tinyxml2::XMLText &text) {
    std::string_view tag = text.Parent()->Value();
    std::string value;
    if (auto v = text.Value(); v) {
        value = v;
    }

    if (tag == "type") { // text is type field
        state = TYPE;
    } else if (tag == "name") { // text is name field
        state = IDENTIFIER;
    } else if (state == TYPE) { // text after type is suffix (optional)
        state = SUFFIX;
    } else if (state == IDENTIFIER) {

        if (value == "[") { // text after name is array size if [
            state = BRACKET_LEFT;
        } else if (value.starts_with("[") && value.ends_with("]")) {

            std::regex rgx("\\[[0-9]+\\]");
            std::sregex_iterator it = std::sregex_iterator(value.begin(), value.end(), rgx);
            std::string suffix;
            while (it != std::sregex_iterator())
            {
                auto temp = it;
                std::smatch m = *temp;
                std::string match = m.str();
                match = match.substr(1, match.size() - 2);
                //std::cout << "  " << match << " at position " << m.position() << '\n';
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
            state = DONE;
            return false;
        }
    } else if (state == BRACKET_LEFT) { // text after [ is <enum>SIZE</enum>
        state = ARRAY_LENGTH;
        data.addArrayLength(value);
    } else if (state == ARRAY_LENGTH) {
        if (value == "][") { // multi dimensional array
            state = BRACKET_LEFT;
        }
        state = DONE;
        return false;
    }

    if (state < 4) { // append if state index is in range
        data.set(state, value);
    }
    return true;
}

void VariableData::trim() {
    std::string &suffix = fields[SUFFIX];
    const auto it = suffix.find_last_not_of(' ');
    if (it != std::string::npos) {
        suffix.erase(it + 1); // removes trailing space
    }
}

XMLDefineParser::XMLDefineParser(tinyxml2::XMLElement *element, const Generator &gen) {
    parse(element, gen);
}

void XMLDefineParser::parse(tinyxml2::XMLElement *element, const Generator &gen) {
    state = DEFINE;
    element->Accept(this);
    trim();
}

void XMLDefineParser::trim() {
    const auto it = value.find_first_not_of(' ');
    if (it != std::string::npos) {
        value.erase(0, it);
    }
}

bool XMLDefineParser::Visit(const tinyxml2::XMLText &text) {
    std::string_view tag = text.Parent()->Value();
    std::string_view value = text.Value();
    if (tag == "name") {
        state = NAME;
    }

    switch (state) {
        case DEFINE:
            break;
        case NAME:
            name = value;
            state = VALUE;
            break;
        case VALUE:
            this->value = value;
            state = DONE;
            break;
        default:
            return false;
    }
    return true;
}
