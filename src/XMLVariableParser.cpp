#include "XMLVariableParser.hpp"

#include "Generator.hpp"

void VariableFields::set(size_t index, const std::string &str) {
    if (index >= size()) {
        return;
    }
    std::array<std::string, 4>::operator[](index) = str;
}

const std::string &VariableFields::get(size_t index) const {
    if (index >= size()) {
        throw std::runtime_error("index out of bounds");
    }
    return std::array<std::string, 4>::operator[](index);
}

VariableData::VariableData(const Generator &gen, Type type) : gen(gen) {
    specialType = type;
    ns = Namespace::NONE;
    ignoreFlag = specialType == TYPE_INVALID;
    arrayLengthFound = false;
    ignorePFN = false;
    nullTerminated = false;
}

VariableData::VariableData(const Generator &gen, const String &object) : VariableData(gen, object, strFirstLower(object))
{}

VariableData::VariableData(const Generator &gen, const String &object, const std::string &id) : VariableData(gen, TYPE_DEFAULT) {
    setIdentifier(id);
    original.setFullType("", object.original, " *");
    setFullType("", object, " *");
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

std::string VariableData::namespaceString() const {
    return gen.getNamespace(ns);
}

bool VariableData::isLenAttribIndirect() const {
    size_t pos = lenAttribStr.find_first_of("->");
    return pos != std::string::npos;
}

void VariableData::setNamespace(Namespace value) {
    ns = value;
}

void VariableData::toRAII() {
    ns = Namespace::RAII;
    convertToReference();
    setConst(true);
}

Namespace VariableData::getNamespace() const {
    return ns;
}

void VariableData::convertToCpp(const Generator &gen) {
    for (size_t i = 0; i < size(); ++i) {
        original.set(i, get(i));
    }

    if (gen.isInNamespace(get(TYPE))) {
        ns = Namespace::VK;        
    }
    set(TYPE, strStripVk(get(TYPE)));
    set(IDENTIFIER, strStripVk(get(IDENTIFIER)));
}

bool VariableData::isNullTerminated() const {
    return nullTerminated;
}

void VariableData::convertToArrayProxy() {
    specialType = TYPE_ARRAY_PROXY;
    removeLastAsterisk();    
}

void VariableData::bindLengthVar(const std::shared_ptr<VariableData> &var) {
    lenghtVar = var;

    flags |= Flags::ARRAY;
    if (isConst()) {
        flags |= Flags::ARRAY_IN;
    } else {
        flags |= Flags::ARRAY_OUT;
    }
}

void VariableData::bindArrayVar(const std::shared_ptr<VariableData> &var) {
    arrayVar = var;
}

const std::shared_ptr<VariableData>& VariableData::getLengthVar() const {
    if (!lenghtVar.get()) {
        throw std::runtime_error("access to null lengthVar");
    }
    return lenghtVar;
}

const std::shared_ptr<VariableData> &VariableData::getArrayVar() const {
    if (!arrayVar.get()) {
        throw std::runtime_error("access to null arrayVar");
    }
    return arrayVar;
}

void VariableData::convertToReturn() {
    specialType = TYPE_RETURN;
    ignoreFlag = true;

    removeLastAsterisk();
}

void VariableData::convertToReference() {
    //specialType = TYPE_REFERENCE;
    removeLastAsterisk();
    setReferenceFlag(true);
}

void VariableData::convertToPointer() {
    // specialType = TYPE_DEFAULT;
    if (!isPointer()) {
        set(SUFFIX, get(SUFFIX) + "*");
    }
    setReferenceFlag(false);
}

void VariableData::convertToOptional() {
    setReferenceFlag(false);
    specialType = TYPE_OPTIONAL;
}

void VariableData::convertToStdVector() {
    specialType = TYPE_VECTOR;
    removeLastAsterisk();
    std::string s = get(PREFIX);
    auto pos = s.find("const");
    if (pos != std::string::npos) {
        s.erase(pos, 5);
        set(PREFIX, s);
    }
}

bool VariableData::removeLastAsterisk() {
    std::string &suffix = VariableFields::operator[](SUFFIX);
    if (suffix.ends_with("*")) {
        suffix.erase(suffix.size() - 1); // removes * at end
        return true;
    }
    return false;
}

void VariableData::setConst(bool enabled) {
    if (enabled) {
        if (get(PREFIX) != "const ") {
            set(PREFIX, "const ");
        }
    }
    else {
        if (get(PREFIX) == "const ") {
            set(PREFIX, "");
        }
    }
}

VariableData::Flags VariableData::getFlags() const {
    return flags;
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

std::string VariableData::toArgument(bool useOriginal) const {
    if (!altPFN.empty()) {
        return altPFN;
    }    
    switch (specialType) {
    case TYPE_VECTOR:
    case TYPE_ARRAY_PROXY:
        return toArgumentArrayProxy();
    default:
        return toArgumentDefault(useOriginal);
    }
}

std::string VariableData::fullType() const {
    std::string type = get(PREFIX);
    type += namespaceString();
    type += get(TYPE) + get(SUFFIX);
    switch (specialType) {
    case TYPE_ARRAY_PROXY:    
        return "ArrayProxy<" + type + "> const &";
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
    out += optionalAmp;
    out += get(IDENTIFIER);
    out += optionalArraySuffix();
    return out;
}

std::string VariableData::declaration() const {
    std::string out = fullType();
    if (!out.ends_with(" ")) {
        out += " ";
    }
    out += get(IDENTIFIER);
    out += optionalArraySuffix();
    return out;
}

std::string VariableData::toStringWithAssignment() const {
    std::string out = toString();
    out += " = ";
    if (!_assignment.empty()) {
        out += _assignment;
    } else {
        out += "{}";
    }
    return out;
}

std::string VariableData::originalToString() const {
    std::string out = originalFullType();
    if (!out.ends_with(" ")) {
        out += " ";
    }
    out += original.get(IDENTIFIER);
    out += optionalArraySuffix();
    return out;
}

void VariableData::setTemplate(const std::string &str) {
    optionalTemplate = str;
}

std::string VariableData::getTemplate() const {
    return optionalTemplate;
}

void VariableData::evalFlags(const Generator &gen) {
    flags = Flags::NONE;
    if (gen.isHandle(original.type())) {
        flags |= Flags::HANDLE;
    }
}

std::string VariableData::optionalArraySuffix() const {
    if (arrayLengthFound) {
        return "[" + arrayLengthStr + "]";
    }
    return "";
}

std::string VariableData::toArgumentArrayProxy() const {
    std::string out = get(IDENTIFIER) + ".data()";
    if (get(TYPE) == original.get(TYPE)) {
        return out;
    }
    return "std::bit_cast<" + originalFullType() + ">(" + out + ")";
}

std::string VariableData::createCast(std::string from) const {
    std::string cast = "static_cast";
    if ((strContains(original.get(SUFFIX), "*") || arrayLengthFound)) {
        cast = "std::bit_cast";
    }    
    return cast + "<" + originalFullType() + (arrayLengthFound ? "*" : "") +
           ">(" + from + ")";
}

std::string VariableData::toArgumentDefault(bool useOriginal) const {    
    if (hasArrayVar()) {
        const auto &var = getArrayVar();
        if (var->isArrayIn() && !var->isLenAttribIndirect()) {
            std::string size = var->identifier() + ".size()";
            if (const auto &str = var->getTemplate(); !str.empty()) {
                size += " * sizeof(" + str+ ")";
            }
            return size;
        }
    }
    std::string id = identifierAsArgument();
    if (get(TYPE) == original.get(TYPE) || useOriginal) {
        return id;
    }
    return createCast(id);
}

std::string VariableData::identifierAsArgument() const {
    std::string_view ogsuf = original.get(SUFFIX);
    std::string_view suf = get(SUFFIX);
    std::string id = get(IDENTIFIER);
    if (specialType == TYPE_OPTIONAL) {
        std::string type = get(PREFIX);
        type += namespaceString();
        type += get(TYPE) + get(SUFFIX);
        return "static_cast<" + type + "*>(" + id + ")";
    }
    if (std::count(ogsuf.begin(), ogsuf.end(), '*') > std::count(suf.begin(), suf.end(), '*')) {
        return "&" + id;
    }
    if (ns == Namespace::RAII) {
        return "*" + id;
    }
    return id;
}

XMLVariableParser::XMLVariableParser(tinyxml2::XMLElement *element, const Generator &gen) : VariableData(gen) {
    parse(element, gen);
}

void XMLVariableParser::parse(tinyxml2::XMLElement *element, const Generator &gen) {
    const char *len = element->Attribute("len");
    if (len) {
        const auto s = split(std::string(len), ",");
        for (const auto &str : s) {
            if (str.empty() || std::isdigit(str[0])) {
                continue;
            }
            if (str == "null-terminated") {
                nullTerminated = true;
            }
            else {
                if (!lenAttribStr.empty()) {
                    std::cout << "Warn: len attrib currently set (is " << lenAttribStr << ", new: " << str << "). xml: " << len << std::endl;
                }
                lenAttribStr = str;
            }
        }
    }

    state = PREFIX;
    arrayLengthFound = false;
    element->Accept(this);

    trim();    
    convertToCpp(gen);
    evalFlags(gen);        
}

bool XMLVariableParser::Visit(const tinyxml2::XMLText &text) {
    std::string_view tag = text.Parent()->Value();
    std::string_view value = text.Value();

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
            arrayLengthStr = value.substr(1, value.size() - 2);
            arrayLengthFound = true;
            state = DONE;
            return false;
        } else {
            state = DONE;
            return false;
        }
    } else if (state == BRACKET_LEFT) { // text after [ is <enum>SIZE</enum>
        state = ARRAY_LENGTH;
        arrayLengthStr = value;
    } else if (state == ARRAY_LENGTH) {
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

void XMLVariableParser::trim() {
    std::string &suffix = VariableFields::operator[](SUFFIX);
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
