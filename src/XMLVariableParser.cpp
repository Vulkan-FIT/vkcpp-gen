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
    // arrayLengthFound = false;
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
    for (const auto &v : arrayVars) {
        if (v == var) {
            // already in array
            std::cout << "Warning: bindArrayVar(): duplicate" << std::endl;
            return;
        }
    }
    arrayVars.push_back(var);
}

const std::shared_ptr<VariableData>& VariableData::getLengthVar() const {
    if (!lenghtVar.get()) {
        throw std::runtime_error("access to null lengthVar");
    }
    return lenghtVar;
}

const std::vector<std::shared_ptr<VariableData>>& VariableData::getArrayVars() const {
    return arrayVars;
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
    case TYPE_ARRAY_PROXY_NO_TEMPORARIES:
        return toArgumentArrayProxy();
    default:
        return toArgumentDefault(useOriginal);
    }
}

std::string VariableData::toStructArgumentWithAssignment() const {
    std::string out;
    if (hasArrayLength()) {
        std::string atype = "std::array";
          switch (arrayAttrib) {
            case ArraySize::NONE:
                break;
            case ArraySize::DIM_1D:
                out += gen.format("{0}<{1}, {2}>", atype, get(TYPE), arraySizes[0]);
                break;
            case ArraySize::DIM_2D:
                out += gen.format("{0}<{0}<{1}, {2}>, {3}>", atype, get(TYPE), arraySizes[1], arraySizes[0]);
                break;
        }
        out += " const &" + get(IDENTIFIER);
    }
    else {
        out = toString();
    }
    if (!_assignment.empty()) {
        out += _assignment;
    }
    return out;
}

std::string VariableData::fullType() const {
    std::string type = get(PREFIX);
    type += namespaceString();
    type += get(TYPE) + get(SUFFIX);
    switch (specialType) {
    case TYPE_ARRAY: {
        std::string out;
        std::string atype = "std::array";
        switch (arrayAttrib) {
            case ArraySize::NONE:
                break;
            case ArraySize::DIM_1D:
                out += gen.format("{0}<{1}, {2}>", atype, get(TYPE), arraySizes[0]);
                break;
            case ArraySize::DIM_2D:
                out += gen.format("{0}<{0}<{1}, {2}>, {3}>", atype, get(TYPE), arraySizes[1], arraySizes[0]);
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
    out += optionalAmp;
    out += get(IDENTIFIER);
    if (specialType != TYPE_ARRAY) {
        out += optionalArraySuffix();
    }
    return out;
}

std::string VariableData::toStructString() const {
    const auto &id = get(IDENTIFIER);
    switch (arrayAttrib) {
        case ArraySize::NONE:
            return toString();
        case ArraySize::DIM_1D:
            return gen.format("{NAMESPACE}::ArrayWrapper1D<{0}, {1}> {2}", get(TYPE), arraySizes[0], id);
        case ArraySize::DIM_2D:
            return gen.format("{NAMESPACE}::ArrayWrapper2D<{0}, {1}, {2}> {3}", get(TYPE), arraySizes[0], arraySizes[1], id);
    }
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
//    if (arrayLengthFound) {
//        return "[" + arrayLengthStr + "]";
//    }
    switch (arrayAttrib) {
        case ArraySize::NONE:
            return "";
        case ArraySize::DIM_1D:
            return "[" + arraySizes[0] + "]";
        case ArraySize::DIM_2D:
            return "[" + arraySizes[0] + "][" + arraySizes[1] + "]";
    }

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
    if ((strContains(original.get(SUFFIX), "*") || hasArrayLength())) {
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
    if (get(TYPE) == original.get(TYPE) || useOriginal) {
        return id;
    }
    return createCast(id);
}

std::string VariableData::toArrayProxySize() const {
    std::string s = get(IDENTIFIER) + ".size()";
    if (original.type() == "void") {
        if (optionalTemplate.empty()) {
            std::cerr << "Warning: ArrayProxy " << get(IDENTIFIER) << " has no template set, but is required" << std::endl;
        }
        s += " * sizeof(" + optionalTemplate + ")";
    }
    return s;
}

std::string VariableData::toArrayProxyData() const {
    return get(IDENTIFIER) + ".data()";
}

std::pair<std::string, std::string> VariableData::toArrayProxyRhs() const {
    if (specialType != TYPE_ARRAY_PROXY && specialType != TYPE_ARRAY_PROXY_NO_TEMPORARIES) {
        return std::make_pair("", "");
    }    
    return std::make_pair(toArrayProxySize(), toArrayProxyData());
}

std::string VariableData::identifierAsArgument() const {
    const std::string &suf = get(SUFFIX);
    std::string id = get(IDENTIFIER);
    if (specialType == TYPE_OPTIONAL) {
        std::string type = get(PREFIX);
        type += namespaceString();
        type += get(TYPE) + get(SUFFIX);
        return "static_cast<" + type + "*>(" + id + ")";
    }
//    if (std::count(ogsuf.begin(), ogsuf.end(), '*') > std::count(suf.begin(), suf.end(), '*')) {
//        return "&" + id;
//    }
    if (ns == Namespace::RAII) {
        return "*" + id;
    }
    return matchTypePointers(suf, original.get(SUFFIX)) + id;
}

XMLVariableParser::XMLVariableParser(tinyxml2::XMLElement *element, const Generator &gen) : VariableData(gen) {
    parse(element, gen);
}

void XMLVariableParser::parse(tinyxml2::XMLElement *element, const Generator &gen) {
    const char *len = element->Attribute("len");
    const char *altlen = element->Attribute("altlen");
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
    if (altlen) {
        altlenAttribStr = altlen;
    }

    state = PREFIX;
    //arrayLengthFound = false;
    element->Accept(this);

    trim();    
    convertToCpp(gen);
    evalFlags(gen);

//    if (!arrayLengthStr.empty()) {
//        auto pos = arrayLengthStr.find("][");
//        if (pos != std::string::npos) {

//        }

//    }
}

bool XMLVariableParser::Visit(const tinyxml2::XMLText &text) {
    std::string_view tag = text.Parent()->Value();
    std::string value;
    if (auto v = text.Value(); v) {
        value = v;
    }

    const auto addArrayLength =[&](const std::string &length) {
        switch (arrayAttrib) {
            case ArraySize::NONE:
                arraySizes[0] = length;
                arrayAttrib = ArraySize::DIM_1D;
                break;
            case ArraySize::DIM_1D:
                arraySizes[1] = length;
                arrayAttrib = ArraySize::DIM_2D;
                break;
            case ArraySize::DIM_2D:
                throw std::runtime_error("xml registry: unsupported array dimension");
                break;
        }
    };

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
                //std::cout << "  " << match << " at position " << m.position() << std::endl;
                addArrayLength(match);
                ++it;
                if (it == std::sregex_iterator()) {
                    suffix = temp->suffix();
                    break;
                }
            }
            if (!suffix.empty()) {
                std::cerr << "[visit] unprocessed suffix: " << suffix << std::endl;
            }

            //arrayLengthStr = value.substr(1, value.size() - 2);
            //arrayLengthFound = true;
            state = DONE;
            return false;
        } else {
            state = DONE;
            return false;
        }
    } else if (state == BRACKET_LEFT) { // text after [ is <enum>SIZE</enum>
        state = ARRAY_LENGTH;
        //arrayLengthStr = value;
        addArrayLength(value);
    } else if (state == ARRAY_LENGTH) {        
//        if (value == "]") { //  ] after SIZE confirms arrayLength field
//            arrayLengthFound = true;
//        }
//        else
        if (value == "][") { // multi dimensional array
            state = BRACKET_LEFT;
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
