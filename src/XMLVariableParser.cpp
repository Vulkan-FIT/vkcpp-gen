#include "XMLVariableParser.hpp"

void VariableFields::set(size_t index, const std::string &str) {
    if (index >= size()) {
        return;
    }
    std::array<std::string, 4>::operator[](index) = str;
}

const std::string &VariableFields::get(size_t index) const {
    if (index >= size()) {
        throw std::exception();
        // return "";
    }
    return std::array<std::string, 4>::operator[](index);
}

VariableData::VariableData() {
    arrayLengthFound = false;
    ignoreFlag = false;
    ignorePFN = false;
    _hasLenAttrib = false;
    specialType = TYPE_DEFAULT;
}

std::string VariableData::lenAttribVarName() {
    size_t pos = _lenAttrib.find_first_of("->");
    if (pos != std::string::npos) {
        return _lenAttrib.substr(0, pos);
    }
    return _lenAttrib;
}

void VariableData::convertToCpp() {
    for (size_t i = 0; i < size(); ++i) {
        original.set(i, get(i));
    }

    set(TYPE, strStripVk(get(TYPE)));
    // std::string temp = get(IDENTIFIER);
    set(IDENTIFIER, strStripVk(get(IDENTIFIER)));
    // std::cout << "Original id: " << temp << " converted: " <<
    // get(IDENTIFIER) << std::endl;
}

void VariableData::convertToArrayProxy(bool bindSizeArgument) {
    specialType =
        bindSizeArgument ? TYPE_SIZE_AND_ARRAY_PROXY : TYPE_ARRAY_PROXY;

         // it->setFullType("", "ArrayProxy<const " + it->type() + ">", " const
         // &");
    removeLastAsterisk();
    // std::cout << get(IDENTIFIER) << " --> to array proxy"  << std::endl;
}

std::shared_ptr<VariableData> VariableData::lengthAttribVar() {
    // std::cout << "Obj[" << this << "] " << "len attrib var get:" <<
    // _lenAttribVar.get() << std::endl;
    if (!_lenAttribVar.get()) {
        throw std::runtime_error("access to null lengthAttrib");
    }
    return _lenAttribVar;
}

void VariableData::convertToTemplate() {
    // specialType = bindSizeArgument ? TYPE_TEMPLATE_WITH_SIZE
    //                              : TYPE_TEMPLATE;

    setFullType("", "T", "");
    // std::cout << get(IDENTIFIER) << " --> to array proxy"  << std::endl;
}

void VariableData::convertToReturn() {
    specialType = TYPE_RETURN;
    ignoreFlag = true;

    removeLastAsterisk();
}

void VariableData::convertToStdVector() {
    specialType = TYPE_VECTOR;
    removeLastAsterisk();
}

bool VariableData::removeLastAsterisk() {
    std::string &suffix = VariableFields::operator[](SUFFIX);
    if (suffix.ends_with("*")) {
        suffix.erase(suffix.size() - 1); // removes * at end
        return true;
    }
    return false;
}

std::string VariableData::toArgument() const {
    if (!altPFN.empty()) {
        return altPFN;
    }
    // std::cout << "to argument: " << get(TYPE) << " vs " <<
    // original.get(TYPE) << std::endl;
    switch (specialType) {
    case TYPE_VECTOR:
    case TYPE_ARRAY_PROXY:
        return toArgumentArrayProxy();
    case TYPE_SIZE_AND_ARRAY_PROXY:
        return toArgumentSizeAndArrayProxy();    
    // case TYPE_TEMPLATE_WITH_SIZE:
    //   return toArgumentTemplateWithSize();
    default:
        return toArgumentDefault();
    }
}

std::string VariableData::fullType() const {
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

std::string VariableData::optionalArraySuffix() const {
    if (arrayLengthFound) {
        return "[" + arrayLengthStr + "]";
    }
    return "";
}

std::string VariableData::createCast(std::string from) const {
    std::string cast = "static_cast";
    if (strContains(original.get(SUFFIX), "*") || arrayLengthFound) {
        cast = "std::bit_cast";
    }    
    return cast + "<" + originalFullType() + (arrayLengthFound ? "*" : "") +
           ">(" + from + ")";
}

std::string VariableData::toArgumentDefault() const {
    std::string id = identifierAsArgument();
    if (get(TYPE) == original.get(TYPE)) {
        return id;
    }
    return createCast(id);
}

std::string VariableData::identifierAsArgument() const
{
    std::string_view osuf = original.get(SUFFIX);
    std::string_view suf = get(SUFFIX);
    if (std::count(osuf.begin(), osuf.end(), '*') > std::count(suf.begin(), suf.end(), '*')) {
        return "&" + get(IDENTIFIER);
    }
    return get(IDENTIFIER);
}

XMLVariableParser::XMLVariableParser(tinyxml2::XMLElement *element) {
    const char *len = element->Attribute("len");
    if (len) {
        // std::cout << "Parsing has len: " << len << std::endl;
        _lenAttrib = len;
        _hasLenAttrib = true;
    }

    parse(element);
}

void XMLVariableParser::parse(tinyxml2::XMLElement *element) {
    state = PREFIX;
    arrayLengthFound = false;
    element->Accept(this);
    trim();
    convertToCpp();
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
    // suffix += " ";
}
