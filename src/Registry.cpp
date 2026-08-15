// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
// "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef USE_PCH
#include <cassert>
#include <filesystem>
#include <functional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>
#endif

#include "Registry.hpp"

#include "Format.hpp"
#include "Generator.hpp"

namespace fs = std::filesystem;

namespace vkgen {

std::string Registry::systemRegistryPath;
std::string Registry::localRegistryPath;

static bool
compare(const Handle& lhs, const Handle& rhs) noexcept
{
    if (lhs.isSubclass < rhs.isSubclass) {
        return false;
    }
    if (lhs.isSubclass > rhs.isSubclass) {
        return true;
    }
    return lhs.name < rhs.name;
}

template<typename T>
class MutableCollection
{
    Container<T>& storage;

  public:
    explicit MutableCollection(Container<T>& storage)
        : storage(storage)
    {
    }

    T& emplace_back(Registry& reg, const xml::Element& element)
    {
#ifndef NDEBUG
        storage.mapBuilt = false;
#endif
        return storage.items.emplace_back(reg, element);
    }

    void buildMapping()
    {
        storage.map.clear();

        for (size_t i = 0; i < storage.items.size(); ++i) {
            auto& item = storage.items[i];
            storage.addMapping(item.name, i);
            if constexpr (std::is_same_v<T, Enum>) {
                if (item.isBitmask()) {
                    storage.addMapping(item.bitmask, i);
                }
            }
        }
#ifndef NDEBUG
        storage.mapBuilt = true;
#endif
    }
};

struct SupportedInfo
{
    std::string_view feature;
    std::string_view extension;
};

template<typename T>
class ItemInserter
{
    // public:
    using Alias = std::pair<std::string, std::string>; // TODO string_view

    const std::unordered_map<std::string, SupportedInfo>& supported;
    T& storage;
    std::vector<Alias> aliased;
    std::unordered_set<std::string> inserted;

    // TODO id

    void addAliases()
    {
        for (const auto& a : aliased) {
            const auto& name = a.first;
            const auto& alias = a.second;
            auto dst = storage.find(name);
            auto src = storage.find(alias);
            if (!dst || !src) {
                std::cerr << "Error: aliased type not found: " << alias
                          << " -> " << name << '\n';
                continue;
            }
            dst->aliasParent = src;
            dst->directDependencies.insert(src->name.original);
            dst->dependencies.insert(src);
        }
    }

  public:
    explicit ItemInserter(
        const std::unordered_map<std::string, SupportedInfo>& supported,
        T& storage
    )
        : supported(supported)
        , storage(storage)
    {
    }

    template<class... Args>
    void insert(
        Generator& gen,
        xml::Element e,
        const std::string_view name,
        Args&&... args
    )
    {
        typename T::value_type item{
            gen, e, name, std::forward<Args>(args)...
        };
        if constexpr (std::is_same_v<typename T::value_type, Enum>) {
            if (!supported.contains(item.name.original) &&
                (item.isBitmask() &&
                 !supported.contains(item.bitmask.original))) {
                std::cout << "Unsupported: " << item.name.original << ", "
                          << item.bitmask.original << "\n";
                return;
            }
        } else {
            if (!supported.contains(item.name.original)) {
                std::cout << "Unsupported: " << item.name.original << "\n";
                return;
            }
        }
        if (inserted.contains(item.name.original)) {
            return;
        }
        inserted.insert(item.name.original);
        storage.items.emplace_back(std::move(item));
        const auto alias = e.optional("alias");
        if (alias) {
            // std::cout << "Inserted alias: " << item.name << "\n";
            if (name.empty()) {
                throw std::runtime_error("Aliased element has no name");
            }
            aliased.emplace_back(name, std::string(alias.value()));
        }
    }

    void finalize()
    {
        MutableCollection collection{ storage };
        collection.buildMapping();

        addAliases();
    }
};

class DependsParser
{
    const char* pos = {};
    const char* end = {};

  public:
    DependsParser(std::string_view str)
    {
        pos = str.data();
        end = str.data() + str.size();
    }

    void parse(std::vector<std::string_view>& fields)
    {
        int state = 0;
        int indent = 2;

        const char* field = nullptr;

        const auto print = [&](const std::string_view text) {
            for (int i = 0; i < indent; i++) {
                std::cout << ' ';
            }
            std::cout << text
                      // << "[" << text.size() << "]"
                      << "\n";
        };

        const auto parseField = [&]() {
            if (field) {
                std::string_view temp(field, pos);
                fields.emplace_back(temp);
                print(temp);
                field = nullptr;
            }
        };

        while (pos != end) {
            if (*pos == ' ') {
                pos++;
                continue;
            }

            if (*pos == '+') {
                parseField();
                print("AND");
            } else if (*pos == ',') {
                parseField();
                print("OR");
            } else if (*pos == '(') {
                parseField();
                print("LEFT(");
                indent += 2;
            } else if (*pos == ')') {
                parseField();
                indent -= 2;
                if (indent < 0) {
                    std::cerr << "ERROR\n";
                }
                print("RIGHT)");
            } else if (!field) {
                field = pos;
            }
            pos++;
        }
        parseField();
    }
};

std::string
GenericType::getVersionDebug() const
{
    std::string out = "// ";
    out += name.original;
    out += " (";
    out += metaTypeString();
    out += ") ";
    if (ext) {
        out += "  ext: ";
        out += ext->name;
        out += " (";
        out += std::to_string(ext->number);
        out += ")";
    }
    out += "\n";
    return out;
}

GenericType::GenericType(
    const GenericType& parent,
    std::string_view name,
    bool firstCapital
)
    : MetaType(parent.metaType())
    , name(std::string{ name }, firstCapital)
{
}

std::string_view
GenericType::getProtect() const
{
    return protect;
}

Platform*
GenericType::getPlatfrom() const
{
    return ext ? ext->platform : nullptr;
}

void
GenericType::setProtect(const std::string_view protect)
{
    this->protect = protect;
}

bool
GenericType::updateBinding(Feature* feature)
{
    if (!feature) {
        return false;
    }
    if (this->feature) {
        // std::cout << "Overriding feature: " << this->feature->name << " -> "
        // << feature->name << ", " << name << '\n';
        return false;
    }
    this->feature = feature;
    return true;
}

bool
GenericType::updateBinding(Extension* ext)
{
    if (!ext) {
        return false;
    }
    if (this->ext) {
        // std::cout << "Overriding extension: " << this->ext->name << " -> " <<
        // ext->name << ", " << name << '\n';
    }
    if (this->ext && ext->number > this->ext->number) {
        return false;
    }
    if (!ext->protect.empty()) {
        protect = ext->protect;
    }
    this->ext = ext;
    return true;
}



Snippet::Snippet(const std::string& name, std::string&& code)
    : GenericType(MetaType::Value::BaseType, name)
    , code(std::move(code))
{
}

FuncPointer::FuncPointer(const std::string& name, std::string&& code)
    : Snippet(name, std::move(code))
{
}

ClassCommand::
operator GenericType() const
{
    return *src;
}

ClassCommand::ClassCommand(const Generator* gen, const Handle* cls, Command& o)
    : cls(cls)
    , src(
          &o
      ) //, name(gen->convertCommandName(o.name.original, cls->name), false)
{

    name.assign(o.name);
    std::string cname = cls->name;
    std::string tag = gen->strRemoveTag(cname);
    if (!cls->name.empty()) {
        name.assign(
            std::regex_replace(
                name, std::regex(cname, std::regex_constants::icase), ""
            )
        );
        if (!name.empty() && std::isupper(name[0])) {
            name[0] = std::tolower(name[0]);
            // std::cout << "CAPITAL: " << name << ", " << o.name << "\n";
        }
    }
    if (!tag.empty()) {
        strStripSuffix(name, tag);
    }

    name.original = o.name.original;

    if (cls->name.original == "VkCommandBuffer" && name.starts_with("cmd")) {
        name.assign(strFirstLower(name.substr(3)));
    }
}

Handle::Handle(
    Generator& gen,
    xml::Element elem,
    const std::string_view,
    std::string&& code
)
    : GenericType(MetaType::Value::Handle, elem.getNested("name"), true)
    , superclass(std::string{ elem.optional("parent").value_or("") })
    , objType(
          std::string{
              elem.optional("objtypeenum").value_or("VK_OBJECT_TYPE_UNKNOWN") }
      )
    , vkhandle(
          VariableDataInfo{ .vktype = this->name.original,
                            // .identifier = "m_" + strFirstLower(this->name),
                            .identifier = "m_handle",
                            .assigment = " = {}",
                            .ns = Namespace::VK,
                            .flag = VariableData::Flags::CLASS_VAR_VK |
                                    VariableData::Flags::CLASS_VAR_RAII,
                            .metaType = MetaType::Value::Handle }
      )
    , code(std::move(code))
{
    objType = gen.enumConvertCamel("ObjectType", objType.original, false);
}

void
Handle::init(Generator& gen)
{
    if (isSubclass) {
        ownerhandle = "m_" + strFirstLower(superclass);
    }

    if (isSubclass) {
        ownerUnique = std::make_unique<VariableData>(VariableDataInfo{
            .vktype = superclass.original,
            .identifier = "m_owner",
            .assigment = " = {}",
            .ns = Namespace::VK,
            .flag = VariableData::Flags::CLASS_VAR_UNIQUE,
            .metaType = MetaType::Value::Handle });

        ownerRaii = std::make_unique<VariableData>(VariableDataInfo{
            .vktype = superclass.original,
            .suffix = " const *",
            .identifier = "m_" + strFirstLower(superclass),
            .assigment = " = nullptr",
            .ns = Namespace::RAII,
            .flag = VariableData::Flags::CLASS_VAR_RAII,
            .metaType = MetaType::Value::Handle });
    }
}

void
Handle::setDestroyCommand(const Generator& gen, Command& cmd)
{
    dtorCmd = &cmd;
    for (const auto& p : cmd._params) {
        if (!p->isHandle()) {
            continue;
        }
        if (p->original.type() == name.original) {
            continue;
        }
        if (p->original.type() == superclass.original) {
            continue;
        }
        secondOwner = std::make_unique<VariableData>(VariableDataInfo{
            .vktype = p->original.type(),
            .identifier = "m_" + strFirstLower(p->type()),
            .assigment = " = {}",
            .ns = Namespace::VK,
            .flag = VariableData::Flags::CLASS_VAR_RAII |
                    VariableData::Flags::CLASS_VAR_UNIQUE,
            .metaType = MetaType::Value::Handle });
    }
}

void
Handle::setParent(const Registry& reg, Handle* h)
{
    parent = h;
    std::string name = h->name;
    reg.strRemoveTag(name);
    if (name.ends_with("Pool")) {
        poolFlag = true;
    }
}

void
Handle::prepare(const Generator& gen)
{
    clear();

    if (!parent) {
        superclass = gen.loader.name;
    }

    const auto& cfg = gen.config();
    effectiveMembers = 0;
    filteredMembers.clear();
    filteredMembers.reserve(members.size());

    std::unordered_map<std::string, ClassCommand*> stage;
    bool order = !gen.orderedCommands.empty();

    for (auto& m : members) {
        if (m.src->canGenerate()) {
            // std::string const s = gen.genOptional(m, [&](std::string &output)
            // {
            effectiveMembers++;
            if (order) {
                stage.emplace(m.name.original, &m);
            } else {
                filteredMembers.push_back(&m);
            }
        }
        // });
    }
    if (order) {
        for (const auto& o : gen.orderedCommands) {
            auto it = stage.find(o.get().name.original);
            if (it != stage.end()) {
                filteredMembers.push_back(it->second);
                stage.erase(it);
            }
        }
        for (auto& c : stage) {
            filteredMembers.push_back(c.second);
        }
    }

    vars.clear();
    if (this == &gen.loader) {
        return;
    }

    vars.emplace_back(std::ref(vkhandle));
    if (ownerUnique) {
        vars.emplace_back(std::ref(*ownerUnique));
    }
    if (ownerRaii) {
        vars.emplace_back(std::ref(*ownerRaii));
    }
    if (secondOwner) {
        vars.emplace_back(std::ref(*secondOwner));
    }
    if (cfg.gen.allocatorParam) {
        vars.emplace_back(std::ref(gen.cvars.raiiAllocator));
        vars.emplace_back(std::ref(gen.cvars.uniqueAllocator));
    }

    vars.emplace_back(std::ref(gen.cvars.uniqueDispatch));

    if (name.original == "VkInstance" && !cfg.gen.raii.staticInstancePFN) {
        vars.emplace_back(std::ref(gen.cvars.raiiInstanceDispatch));
    } else if (name.original == "VkDevice" && !cfg.gen.raii.staticDevicePFN) {
        vars.emplace_back(std::ref(gen.cvars.raiiDeviceDispatch));
    }
}

void
Handle::addCommand(const Generator& gen, Command& cmd, bool raiiOnly)
{
    auto& c = members.emplace_back(&gen, this, cmd);
    c.raiiOnly = raiiOnly;
}

std::string
Enum::toFlags(const std::string& name)
{
    return std::regex_replace(name, std::regex("FlagBits"), "Flags");
}

std::string
Enum::toFlagBits(const std::string& name)
{
    return std::regex_replace(name, std::regex("Flags"), "FlagBits");
}

EnumValue::EnumValue(
    const Registry& reg,
    std::string name,
    const std::string& value,
    const std::string& enumName,
    bool isBitmask
)
    : GenericType(MetaType::Value::EnumValue, name)
    , value(value)
{
    this->name.assign(reg.enumConvertCamel(enumName, name, isBitmask));
    this->enabled = true;
    // std::cout << "Value: " << this->name << ", " << this->name.original <<
    // "\n";
}

std::string
EnumValue::toHex(uint64_t value, bool is64bit)
{
    std::string str = vkgen::format("{:x}", value);
    if (str.size() > 8) {
        str = str.substr(str.size() - 8);
    }
    str = "0x" + str;
    if (is64bit) {
        str += "ULL";
    }
    return str;
}

void
EnumValue::setValue(uint64_t v, bool negative, const Enum& parent)
{
    value = negative ? "-" : "";
    if (parent.isBitmask()) {
        value += toHex(v, parent.is64bit());
    } else {
        value += std::to_string(v);
    }
    numericValue =
        negative ? -static_cast<int64_t>(v) : static_cast<int64_t>(v);
}

Enum::Enum(
    Generator& gen,
    xml::Element elem,
    const std::string_view name,
    bool isBitmask
)
    : GenericType(MetaType::Value::Enum, name, true)
{
    if (isBitmask) {
        auto nameFlags = elem.getNested("name");
        bitmask = String(nameFlags, true);
        type = elem.getNested("type");
        auto req = elem.optional("requires");
        // TODO
        // this->name = String(Enum::toFlagBits(std::string{nameFlags}), true);
        this->name = String(Enum::toFlagBits(std::string{ nameFlags }), true);
        if (req) {

        } else {
        }
    } else {
        type = "VkFlags";
    }
}

bool
Enum::containsValue(const std::string& value) const
{
    return std::ranges::any_of(members, [&](const auto& m) {
        return m.name == value;
    });
}

EnumValue*
Enum::find(const std::string_view value) noexcept
{
    for (auto& m : members) {
        if (m.name.original == value || m.name == value) {
            return &m;
        }
    }
    return nullptr;
}

void
Command::init(const Registry& reg)
{
    const bool noArray = name.original == "vkGetDescriptorEXT";
    _params.bind(noArray);

    initParams();

    bool toReference = false;
    bool hasHandle = false;
    bool hasTopHandle = false;
    bool canTransform = false;
    for (auto& p : _params) {
        if (p->isOutParam()) {
            if (p->getArrayVars().empty()) {
                outParams.push_back(std::ref(*p));
                if (p->isHandle()) {
                    if (!reg.findHandle(p->original.type()).isSubclass) {
                        hasTopHandle = true;
                    }
                    hasHandle = true;
                }
            }
            if (p->isStruct()) {
                const auto& s = reg.structs.find(p->original.type());
                if (s && !s->extends.empty()) {
                    structChain = &*s;
                    if (p->isArray()) {
                        structChainVector = true;
                    }
                }
            }
            canTransform = true;
        }

        auto* sizeVar = p->getLengthVar();
        if (sizeVar) {
            canTransform = true;
        }
        if (p->isPointer() && p->isStructOrUnion()) {
            canTransform = true;
        }

        if (p->isPointer() && !p->isArray() && p->original.type() != "void" &&
            p->original.type() != "VkAllocationCallbacks") {
            if (!p->isStructOrUnion() || p->isOutParam()) {
                toReference = true;
            }
        }
    }

    if (hasHandle) {
        setFlagBit(CommandFlags::CREATES_HANDLE, true);
    }
    if (hasTopHandle) {
        setFlagBit(CommandFlags::CREATES_TOP_HANDLE, true);
    }
    if (canTransform) {
        setFlagBit(CommandFlags::CPP_VARIANT, true);
    }
    if (toReference) {
        setFlagBit(CommandFlags::REFERENCE_PARAM, true);
    }

    prepared = true;
}

Command::Command(
    Generator& gen,
    xml::Element elem,
    const std::string_view nameFallback
)
    : GenericType(MetaType::Value::Command)
{
    // iterate contents of <command>
    std::string dbg;
    std::string name = std::string{ nameFallback };
    // add more space to prevent reallocating later
    _params.reserve(16);
    for (const auto& child : xml::View(elem.firstChild())) {
        // <proto> section
        dbg += std::string(child->Value()) + "\n";
        if (child->Value() == std::string_view("proto")) {
            // get <name> field in proto
            auto* nameElement = child->FirstChildElement("name");
            if (nameElement) {
                name = nameElement->GetText();
            }
            // get <type> field in proto
            auto* typeElement = child->FirstChildElement("type");
            if (typeElement) {
                type = typeElement->GetText();
                auto* next = typeElement->NextSibling();
                if (next) {
                    auto* text = next->ToText();
                    if (text) {
                        auto* typeSuffix = text->Value();
                        if (typeSuffix) {
                            type += typeSuffix;
                        }
                    }
                }
            }
        }
        // <param> section
        else if (child->Value() == std::string_view("param")) {
            if (xml::isVulkan(child)) {
                _params.emplace_back(
                    std::make_unique<VariableData>(gen, child)
                );
            }
        }
    }
    if (name.empty()) {
        std::cerr << "Command has no name" << '\n';
    }

    setName(gen, name);

    const auto successcodes = elem.optional("successcodes");
    if (successcodes) {
        for (const auto& str : split(std::string(successcodes.value()), ",")) {
            successCodes.push_back(str);
        }
    }

    // init(gen);
}

Command::Command(
    const Registry& reg,
    const Command& o,
    const std::string_view alias
)
    : GenericType(MetaType::Value::Command)
{
    type = o.type;
    successCodes = o.successCodes;
    nameCat = o.nameCat;
    pfnReturn = o.pfnReturn;
    flags = o.flags;
    setFlagBit(CommandFlags::ALIAS, true);
    setName(reg, std::string(alias));

    _params.reserve(o._params.size());
    for (const auto& p : o._params) {
        _params.push_back(std::make_unique<VariableData>(*p));
    }

    // init(reg);
}

Command::PFNReturnCategory
getPFNReturnCategory(const std::string& type)
{
    using enum Command::PFNReturnCategory;
    if (type == "void") {
        return VOID;
    }
    if (type == "VkResult") {
        return VK_RESULT;
    }
    return OTHER;
}

Command::NameCategory
getMemberNameCategory(const std::string& name)
{
    using enum Command::NameCategory;
    if (name.starts_with("vkGet")) {
        return GET;
    }
    if (name.starts_with("vkAllocate")) {
        return ALLOCATE;
    }
    if (name.starts_with("vkAcquire")) {
        return ACQUIRE;
    }
    if (name.starts_with("vkCreate")) {
        return CREATE;
    }
    if (name.starts_with("vkEnumerate")) {
        return ENUMERATE;
    }
    if (name.starts_with("vkWrite")) {
        return WRITE;
    }
    if (name.starts_with("vkDestroy")) {
        return DESTROY;
    }
    if (name.starts_with("vkFree")) {
        return FREE;
    }
    return UNKNOWN;
}

Handle*
Command::secondIndirectCandidate(Generator& gen) const
{
    if (_params.size() < 2) {
        return nullptr;
    }
    if (!_params[1]->isHandle()) {
        return nullptr;
    }
    if (destroysObject()) {
        return nullptr;
    }

    const auto& type = _params[1]->original.type();
    bool isCandidate = true;
    try {
        const auto& var = getLastPointerVar();
        if (getsObject() || createsHandle()) {
            if (nameCat != NameCategory::GET) {
                isCandidate = !var->isArray();
            } else {
                isCandidate = var->original.type() != type;
            }
        } else if (destroysObject()) {
            isCandidate = var->original.type() != type;
        }
    } catch (std::runtime_error) {
    }
    if (!isCandidate) {
        return nullptr;
    }
    auto& handle = gen.findHandle(type);
    return &handle;
}

void
Command::setName(const Registry& reg, const std::string& name)
{
    this->name.convert(name);
    pfnReturn = getPFNReturnCategory(type);
    nameCat = getMemberNameCategory(name);
}

bool
Feature::tryInsert(Registry& reg, const std::string& name)
{
    if (tryInsertFrom(reg.structs, name, structs)) {
        return true;
    }
    if (tryInsertFrom(reg.enums, name, enums)) {
        return true;
    }
    if (tryInsertFrom(reg.handles, name, handles)) {
        return true;
    }
    if (tryInsertFromMap(reg.baseTypes, name, baseTypes)) {
        return true;
    }
    if (tryInsertFromMap(reg.funcPointers, name, funcPointers)) {
        return true;
    }
    if (tryInsertFromMap(reg.defines, name, defines)) {
        return true;
    }
    // if (tryInsertFromMap(reg.aliases, name, aliases)) {
    //     return true;
    // }
    if (tryInsertFromMap(reg.includes, name, includes)) {
        return true;
    }
    return false;
}

Struct::Struct(
    Generator& gen,
    const xml::Element& e,
    const std::string_view name,
    MetaType::Value type
)
    : GenericType(type, name, true)
{
    auto returnedonly = e.optional("returnedonly");
    if (returnedonly.value_or("") == "true") {
        this->returnedonly = true;
    }

    // iterate contents of <type>, filter only <member> children
    for (const auto& member : xml::vulkanElements(e.firstChild(), "member")) {
        auto& v = members.emplace_back(
            std::move(std::make_unique<VariableData>(gen, member))
        );

        const std::string& type = v->type();
        const std::string& name = v->identifier();

        if (!v->isPointer() && (type == "float" || type == "double")) {
            containsFloatingPoints = true;
        }

        if (const char* values = member->ToElement()->Attribute("values")) {
            std::string value = gen.enumConvertCamel(type, values);
            v->setAssignment(" = " + type + "::" + value);
            if (v->original.type() ==
                "VkStructureType") { // save sType information for structType
                structTypeValue.original = values;
                structTypeValue = value;
            }
        }
    }
    members.bind();
}



class RegistryLoader
{
    Registry& reg;

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* root = {};

    std::unordered_map<std::string, xml::Element> xmlInternalFeatures;
    std::unordered_map<std::string, std::string> handleParent;
    std::unordered_map<std::string, SupportedInfo> supportedElements;
    std::vector<std::pair<std::string, std::string>> structExtends;
    std::vector<std::pair<std::string, std::string>> typeRequires;

    bool loadXML(const std::string& xmlPath)
    {
        std::cout << "load: " << xmlPath << "\n";
        const auto err = doc.LoadFile(xmlPath.c_str());
        if (err != tinyxml2::XML_SUCCESS) {
            std::cerr << "XML load failed: " << std::to_string(err)
                      << " (file: " << xmlPath + ")\n";
            return false;
        }

        root = doc.RootElement();
        if (!root) {
            std::cerr << "XML file is empty\n";
            return false;
        }
        return true;
    }

    void parseXML(Generator& gen)
    {
        using Func =
            void (RegistryLoader::*)(Generator&, xml::Element, xml::Element);
        using Binding = std::pair<const std::string_view, Func>;
        // specifies order of parsing vk.xml registry
        const auto loadOrder = std::array{
            Binding{ "platforms", &RegistryLoader::parsePlatforms },
            Binding{ "tags", &RegistryLoader::parseTags },
            Binding{ "feature", &RegistryLoader::parseFeature },
            Binding{ "extensions", &RegistryLoader::parseExtensions },
            Binding{ "types", &RegistryLoader::parseTypes },
            Binding{ "enums", &RegistryLoader::parseEnums },
            Binding{ "commands", &RegistryLoader::parseCommands },
        };

        // call each function in rootParseOrder with corresponding XMLNode
        auto* elements = root->FirstChildElement();
        for (const auto& key : loadOrder) {
            for (const auto& elem : xml::View(elements)) {
                if (key.first == elem->Value()) {
                    const auto& func = key.second;
                    (this->*func)(gen, elem, elem.firstChild());
                }
            }
        }
    }

    void
    parsePlatforms(Generator& gen, xml::Element elem, xml::Element children)
    {
        if (reg.verbose)
            std::cout << "Parsing platforms" << '\n';

        MutableCollection platforms{ reg.platforms };

        // iterate contents of <platforms>, filter only <platform> children
        int id = 0;
        for (const auto& element : xml::vulkanElements(children, "platform")) {
            auto& p = platforms.emplace_back(reg, element);
            p.id = ++id;
        }
        platforms.buildMapping();

        if (reg.verbose)
            std::cout << "Parsing platforms done" << '\n';
    }

    void parseTags(Generator& gen, xml::Element elem, xml::Element children)
    {
        if (reg.verbose)
            std::cout << "Parsing tags" << '\n';
        // iterate contents of <tags>, filter only <tag> children
        for (const auto& tag : xml::elements(children, "tag")) {
            auto name = tag["name"];
            reg.tags.emplace(name);
        }
        if (reg.verbose)
            std::cout << "Parsing tags done" << '\n';
    }

    void parseTypes(Generator& gen, xml::Element elem, xml::Element children)
    {
        if (reg.verbose)
            std::cout << "Parsing declarations" << '\n';

        std::unordered_map<
            std::string,
            std::pair<std::string_view, std::string_view>>
            enums;
        ItemInserter enumsInserter{ supportedElements, reg.enums };
        ItemInserter structsInserter{ supportedElements, reg.structs };
        ItemInserter handlesInserter{ supportedElements, reg.handles };

        // iterate contents of <types>, filter only <type> children
        for (const auto& type : xml::vulkanElements(children, "type")) {
            const auto categoryAttrib = type.optional("category");
            const auto name = type.optional("name");
            if (!categoryAttrib) {
                const auto requiresAttrib = type.optional("requires");
                if (name && requiresAttrib &&
                    requiresAttrib.value() != "vk_platform") {
                    typeRequires.emplace_back(
                        requiresAttrib.value(), name.value()
                    );
                }
                continue;
            }
            const auto cat = categoryAttrib.value();

            if (cat == "enum") {
                enumsInserter.insert(gen, type, name.value_or(""), false);
            } else if (cat == "bitmask") {
                enumsInserter.insert(gen, type, name.value_or(""), true);
            } else if (cat == "handle") {
                XMLTextParser parser{ type };
                const auto name = type.getNested("name");
                const auto parent = type.optional("parent");
                if (parent) {
                    handleParent[std::string{ name }] = parent.value();
                }
                handlesInserter.insert(gen, type, name, std::move(parser.text));
            } else if (cat == "struct" || cat == "union") {
                if (name) {
                    MetaType::Value const metaType =
                        (cat == "struct") ? MetaType::Value::Struct
                                          : MetaType::Value::Union;
                    structsInserter.insert(
                        gen, type, name.value_or(""), metaType
                    );

                    auto extends = type.optional("structextends");
                    if (extends) {
                        for (const auto& e : split2(extends.value(), ",")) {
                            structExtends.emplace_back(name.value(), e);
                        }
                    }
                }
            } else if (cat == "define") {
                XMLTextParser parser{ type };
                const auto& name = parser["name"];
                reg.defines.emplace(
                    std::piecewise_construct,
                    std::make_tuple(name),
                    std::make_tuple(name, std::move(parser.text))
                );
            } else if (cat == "basetype") {
                XMLTextParser parser{ type };
                const auto& name = parser["name"];
                reg.baseTypes.emplace(
                    std::piecewise_construct,
                    std::make_tuple(name),
                    std::make_tuple(name, std::move(parser.text))
                );
            } else if (cat == "funcpointer") {
                if (type->GetText()) {
                    XMLTextParser parser{ type };
                    const auto& name = parser["name"];
                    reg.funcPointers.emplace(
                        std::piecewise_construct,
                        std::make_tuple(name),
                        std::make_tuple(name, std::move(parser.text))
                    );
                } else {
                    Command temp(gen, type, "");
                    const auto& name = temp.name.original;
                    std::stringstream text;
                    text << "typedef " << temp.type << " (VKAPI_PTR *" << name
                         << ")(";
                    bool first = true;
                    for (const auto& p : temp._params) {
                        if (!first) {
                            text << ", ";
                        } else {
                            first = false;
                        }
                        // text << "\n  ";
                        text << p->originalFullType() << " " << p->identifier();
                    }
                    text << ");";
                    // std::cout << "funcpointer: " << text.str() << "\n";;
                    reg.funcPointers.emplace(
                        std::piecewise_construct,
                        std::make_tuple(name),
                        std::make_tuple(name, std::move(text.str()))
                    );
                }
            } else if (cat == "include") {
                const auto& name = type["name"];
                const auto* text = type->GetText();
                std::string inc;
                if (text) {
                    inc = text;
                } else {
                    inc = "#include <";
                    inc += name;
                    inc += ">\n";
                }
                reg.includes.emplace(name, inc);
            }
        }

        handlesInserter.finalize();
        enumsInserter.finalize();
        structsInserter.finalize();

        for (auto& e : reg.enums) {
            if (e.aliasParent) {
                e.type = reinterpret_cast<Enum*>(e.aliasParent)->type;
            }
        }

        if (reg.verbose)
            std::cout << "Parsing declarations done" << '\n';
    }

    void parseApiConstants(Generator& gen, xml::Element elem)
    {

        std::map<std::string, xml::Element> aliased;
        for (const auto& e : xml::elements(elem.firstChild(), "enum")) {
            const auto alias = e.optional("alias");
            if (alias) {
                aliased.emplace(std::string(alias.value()), e);
                continue;
            }

            const auto name = std::string(e["name"]);
            const auto type = std::string(e["type"]);
            const auto value = std::string(e["value"]);
            reg.apiConstants.emplace_back(reg, name, value, type);
        }

        static const auto find = [&](const std::string& name) {
            for (const auto& e : reg.apiConstants) {
                if (e.name.original == name) {
                    return e;
                }
            }
            throw std::runtime_error("can't find api constant: " + name);
        };

        for (const auto& a : aliased) {
            const auto& target = find(a.first);
            const auto name = std::string(a.second["name"]);
            reg.apiConstants.emplace_back(reg, name, target.value, target.type);
        }
    }

    static constexpr uint64_t calcEnumExtensionValue(int extnumber)
    {
        return 1000000000 + 1000 * (extnumber - 1);
    }

    void parseEnumValue(
        const xml::Element& elem,
        Enum& e,
        Feature* feature = {},
        Extension* ext = {}
    )
    {

        const auto name = elem["name"];
        EnumValue* type = e.find(name);
        if (!type) {
            type = &e.members.emplace_back(
                reg, std::string(name), "", e.name, e.isBitmask()
            );
        }

        const auto alias = elem.optional("alias");
        if (alias) {
            type->alias = alias.value();
            type->isAlias = true;
            return;
        }

        bool neg = false;
        const auto dir = elem.optional("dir");
        if (dir == "-") {
            neg = true;
        }

        const auto value = elem.optional("value");
        if (value) {
            type->value = neg ? "-" : "";
            type->value += value.value();
        } else {
            uint64_t eval = 0;
            if (!e.isBitmask()) {
                const auto extnumber = elem.optional("extnumber");
                if (extnumber) {
                    eval = calcEnumExtensionValue(
                        toInt(std::string(extnumber.value()))
                    );
                } else if (ext) {
                    eval = calcEnumExtensionValue(ext->number);
                }
            }
            const auto bitpos = elem.optional("bitpos");
            const auto offset = elem.optional("offset");
            if (bitpos) {
                eval += 1ULL << toInt(std::string(bitpos.value()));
                type->setValue(eval, neg, e);
            } else if (offset) {
                eval += toInt(std::string(offset.value()));
                type->setValue(eval, neg, e);
            }
        }

        type->updateBinding(feature);
        type->updateBinding(ext);
    }

    void parseEnums(Generator& gen, xml::Element elem, xml::Element children)
    {
        if (!xml::isVulkan(elem)) {
            return;
        }

        const auto name = elem["name"];
        if (name == "API Constants") {
            parseApiConstants(gen, elem);
            return;
        }

        const auto type = elem.optional("type");
        if (!type) {
            return;
        }

        bool const isBitmask = type == "bitmask";
        if (isBitmask || type == "enum") {
            // std::cout << "lookup " << name << "\n";
            auto& en = reg.enums[name];

            for (const auto& value : xml::vulkanElements(children, "enum")) {
                parseEnumValue(value, en);
            }
        }
    }

    void parseCommands(Generator& gen, xml::Element elem, xml::Element children)
    {
        if (reg.verbose)
            std::cout << "Parsing commands" << '\n';

        ItemInserter inserter{ supportedElements, reg.commands };
        for (const auto& commandElement :
             xml::vulkanElements(children, "command")) {
            inserter.insert(
                gen,
                commandElement,
                commandElement.optional("name").value_or("")
            );
        }
        inserter.finalize();

        for (auto& c : reg.commands) {
            if (c.destroysObject()) {
                bool hasOverload = false;
                if (c.name.original == "vkDestroyInstance" ||
                    c.name.original == "vkDestroyDevice") {
                    hasOverload = true;
                } else {
                    auto name = c.name.original;
                    const auto& tag = reg.strRemoveTag(name);
                    hasOverload = !tag.empty() && reg.commands.contains(name);
                }
                if (hasOverload) {
                    c.setFlagBit(
                        Command::CommandFlags::OVERLOADED_DESTROY, true
                    );
                }
            }
        }

        if (reg.verbose)
            std::cout << "Parsing commands done" << '\n';
    }

    // void addToSupported(xml::Element elem) {
    //
    // }

    void parseFeature(Generator& gen, xml::Element elem, xml::Element children)
    {
        if (xml::isVulkan(elem)) {
            const auto name = elem["name"];
            auto apitype = elem.optional("apitype");
            if (apitype && apitype.value() == "internal") {
                xmlInternalFeatures[std::string(name)] = elem;
            } else {
                auto& item = reg.features.items.emplace_back(reg, elem);
                item.id = reg.features.items.size();
            }

            for (const auto& require :
                 xml::elements(elem.firstChild(), "require")) {
                for (const auto& entry : xml::View(require.firstChild())) {
                    const auto itemName = entry.optional("name");
                    if (itemName) {
                        supportedElements[std::string{ itemName.value() }]
                            .feature = name;
                    }
                }
            }
        }
    }

    void
    parseExtensions(Generator& gen, xml::Element elem, xml::Element children)
    {
        for (const auto& extension : xml::elements(children, "extension")) {
            if (xml::isVulkanExtension(extension)) {
                const auto name = extension["name"];
                auto& item = reg.extensions.items.emplace_back(reg, extension);
                item.id = reg.extensions.items.size();

                for (const auto& require :
                     xml::elements(extension.firstChild(), "require")) {
                    for (const auto& entry : xml::View(require.firstChild())) {
                        const auto itemName = entry.optional("name");
                        if (itemName) {
                            supportedElements[std::string{ itemName.value() }]
                                .extension = name;
                        }
                    }
                }
            }
        }
    }

    template<typename T>
    bool contains(
        const std::unordered_map<std::string, int>& types,
        const T& item
    ) const noexcept
    {
        if (types.contains(item.name.original)) {
            return true;
        }
        if constexpr (std::is_same_v<T, Enum>) {
            return item.isBitmask() && types.contains(item.bitmask.original);
        }
        return false;
    }

    template<typename T>
    std::vector<std::reference_wrapper<typename T::value_type>> makeOrdered(
        T& collection
    ) const
    {
        const auto size = collection.items.size();
        std::vector<std::reference_wrapper<typename T::value_type>> ordered;
        ordered.reserve(size);
        for (auto& item : collection.items) {
            ordered.emplace_back(std::ref(item));
        }
        std::sort(
            ordered.begin(),
            ordered.end(),
            [](const typename T::value_type& a,
               const typename T::value_type& b) -> bool { return a < b; }
        );
        return ordered;
    }

    template<typename T>
    void sortDependencies(T& collection)
    {
        const auto size = collection.items.size();
        std::unordered_set<std::string> inserted;
        std::unordered_set<std::string> unsupported;
        std::vector<std::reference_wrapper<typename T::value_type>> ordered;
        std::vector<std::reference_wrapper<typename T::value_type>> output;
        ordered.reserve(size);
        output.reserve(size);
        {
            for (auto& item : collection.items) {
                ordered.emplace_back(std::ref(item));
            }
            std::sort(
                ordered.begin(),
                ordered.end(),
                [](const typename T::value_type& a,
                   const typename T::value_type& b) -> bool {
                    if constexpr (std::is_same_v<
                                      typename T::value_type,
                                      Handle>) {
                        return compare(a, b);
                    }
                    return a < b;
                }
            );
        }

        int id = 0;
        while (!ordered.empty()) {
            bool stuck = true;

            // std::cout << "Loop: " << ordered.size() << " remain\n";
            auto it = ordered.begin();
            while (it != ordered.end()) {
                auto& item = it->get();
                bool dependenciesSatisfied = true;
                for (const auto& d : item.directDependencies) {
                    if (!inserted.contains(d) && !unsupported.contains(d)) {
                        // std::cout << "  " <<  item.name << " missing " << d
                        // << "\n";
                        dependenciesSatisfied = false;
                        break;
                    }
                }
                if (!dependenciesSatisfied) {
                    ++it;
                    continue;
                }
                // std::cout << "Take: " << item.name;
                // if (!item.directDependencies.empty()) {
                //     std::cout << " -> ";
                //     for (const auto &d : item.directDependencies) {
                //         std::cout << d << ", ";
                //     }
                // }
                // std::cout << ", " << inserted.size() << "\n";
                item.id = ++id;
                output.emplace_back(*it);
                inserted.insert(item.name.original);
                stuck = false;
                it = ordered.erase(it);
            }
            if (stuck) {
                int i = 0;
                for (const auto& o : output) {
                    std::cout << "[" << i++ << "] " << o.get().name.original
                              << std::endl;
                }
                for (const auto& o : ordered) {
                    std::cout << "remain: " << o.get().name.original << " (";
                    for (const auto& d : o.get().directDependencies) {
                        std::cout << d;
                        if (!inserted.contains(d)) {
                            std::cout << "<MISS>";
                        }
                        std::cout << ", ";
                    }
                    std::cout << ")" << std::endl;
                }

                throw std::runtime_error(
                    "dependency sort: infinite loop detected"
                );
            }
        }

        // TODO simplify
        collection.vulkan = std::move(output);
        collection.ordered = makeOrdered(collection);

        // std::cout << typeid(typename T::value_type).name() << " sorted: " <<
        // size
        // << " -> " << collection.items.size() << ", " << osize << "\n";
    }

    void orderTypes()
    {
        sortDependencies(reg.structs);
        sortDependencies(reg.handles);
        sortDependencies(reg.commands);
        sortDependencies(reg.enums);
    }

    void buildTypesMap()
    {
        reg.types.clear();
        // reg.aliases.clear();

        reg.handles.addTypes(reg.types);
        reg.enums.addTypes(reg.types);
        reg.structs.addTypes(reg.types);
        reg.commands.addTypes(reg.types);
        for (auto& a : reg.apiConstants) {
            reg.types.emplace(a.name.original, &a);
        }
    }

    Handle* getTopLevelHandle(Handle& data) const
    {
        auto* it = &data;
        std::set<Handle*> visited;
        while (it) {
            if (it->name.original == "VkInstance" ||
                it->name.original == "VkDevice") {
                break;
            }
            bool loop = visited.contains(it);
            visited.insert(it);
            if (loop) {
                std::cout << "Loop: \n";
                for (auto h : visited) {
                    std::cout << h->name << " - ";
                }
                std::cout << "\n";
                throw std::runtime_error("Loop in parent relationship");
                break;
            }
            it = it->parent;
        }
        return it;
    }

    void assign(xml::Element elem, Feature* feature, Extension* ext)
    {
        for (const auto& entry : xml::View(elem.firstChild())) {

            const std::string_view value = entry->Value();
            // std::cout << value << ", " << number << "\n";
            if (value == "enum") {
                const auto extends = entry.optional("extends");
                if (extends) {
                    auto& en = reg.enums[extends.value()];
                    parseEnumValue(entry, en, feature, ext);
                } else {
                    const auto name = entry.optional("name");
                    const auto value = entry.optional("value");
                    if (name && value) {
                        std::string code = "#define ";
                        code += name.value();
                        code += " ";
                        code += value.value();
                        code += "\n";
                        // std::cout << "  " <<  name.value() << ": " << code <<
                        // "\n";
                        if (feature) {
                            feature->constants.emplace_back(std::move(code));
                        }
                        if (ext) {
                            ext->constants.emplace_back(std::move(code));
                        }
                    }
                }
                if (feature) {
                    feature->elements++; // TODO needed?
                }
                if (ext) {
                    ext->elements++; // TODO needed?
                }
            } else if (value == "command") {
                const auto name = entry["name"];
                auto& command = reg.commands[name];
                if (command.updateBinding(feature)) {
                    feature->insert(feature->commands, command);
                    // feature->tryInsertFrom(reg.commands, std::string(name),
                    // feature->commands);
                }
                if (command.updateBinding(ext)) {
                    ext->insert(ext->commands, command);
                    // ext->tryInsertFrom(reg.commands, std::string(name),
                    // ext->commands);
                }
            } else if (value == "type") {
                const auto name = entry["name"];
                auto* type = reg.find(name);
                if (type) {
                    if (type->updateBinding(feature)) {
                        if (!feature->tryInsert(reg, std::string(name))) {
                            std::cout << "(ext) can't find: " << name << ", "
                                      << type->metaTypeString() << "\n";
                        }
                    }
                    if (type->updateBinding(ext)) {
                        if (!ext->tryInsert(reg, std::string(name))) {
                            std::cout << "(ext) can't find: " << name << ", "
                                      << type->metaTypeString() << "\n";
                        }
                    }
                    // if (name == "vk_platform") {
                    //     continue;
                    // }
                } else {
                    std::cout << "can't find type: " << entry["name"] << "\n";
                }
            }
        }
    }

    void assign(Feature* target, const xml::Element source)
    {
        const auto depends = source.optional("depends");
        if (depends) {
            std::cout << "Parse depends: " << depends.value() << "\n";
            std::vector<std::string_view> fields;
            DependsParser(depends.value()).parse(fields);

            for (const auto& f : fields) {
                auto it = xmlInternalFeatures.find(std::string(f));
                if (it != xmlInternalFeatures.end()) {
                    std::cout << "Found internal: " << f << "\n";

                    assign(target, it->second);
                }
            }
        }

        for (const auto& require :
             xml::elements(source.firstChild(), "require")) {
            // if (require.optional("depends").has_value()) {
            //     continue;
            // }

            // if constexpr (std::is_same_v<T, Feature>) {
            assign(require, target, nullptr);
            // }
            // else if constexpr (std::is_same_v<T, Extension>) {
            //     assign(require, pfeature, &item);
            // }
            // else {
            //     static_assert(false);
            // }
        }
    }

    template<typename T>
    void prepareFeatureOrExtensions(Container<T>& collection)
    {

        if constexpr (std::is_same_v<T, Feature>) {
            std::sort(
                collection.begin(),
                collection.end(),
                [](const T& a, const T& b) { return a.number < b.number; }
            );
        }

        MutableCollection inserter{ collection }; // TODO rename
        inserter.buildMapping();

        collection.ordered = makeOrdered(collection);
        collection.vulkan.clear();
        collection.vulkan.reserve(collection.items.size());
        for (auto& f : collection.items) {
            collection.vulkan.emplace_back(std::ref(f));
        }

        if constexpr (std::is_same_v<T, Feature>) {
            for (auto& item : collection.items) {
                assign(&item, item.xmlElement);
            }
            return;
        }
        for (auto& item : collection.items) {
            const auto& elem = item.xmlElement;
            Feature* pfeature = nullptr;
            if constexpr (std::is_same_v<T, Extension>) {
                const auto promote = elem.optional("promotedto");
                if (promote) {
                    // pfeature = reg.features.find(promote.value());
                }

                const auto depends = elem.optional("depends");
                if (depends) {
                    // std::cout << "Parse depends: " << depends.value() <<
                    // "\n"; DependsParser (depends.value()).parse(); for (const
                    // auto &dep : split2(depends.value(), "+")) {
                    //     if (auto it = reg.extensions.find(dep); it) {
                    //         item.depends.emplace_back(it);
                    //     }
                    //     else {
                    //         item.versiondepends += dep;
                    //     }
                    // }
                }

            } else if constexpr (std::is_same_v<T, Feature>) {
                const auto depends = elem.optional("depends");
                if (depends) {
                    std::cout << "Parse depends: " << depends.value() << "\n";
                    std::vector<std::string_view> fields;
                    DependsParser(depends.value()).parse(fields);

                    for (const auto& f : fields) {
                        auto it = xmlInternalFeatures.find(std::string(f));
                        if (it != xmlInternalFeatures.end()) {
                            std::cout << "Found internal: " << f << "\n";
                        }
                    }
                }
            }

            for (const auto& require :
                 xml::elements(elem.firstChild(), "require")) {
                // if (require.optional("depends").has_value()) {
                //     continue;
                // }
                // TODO refactor
                if constexpr (std::is_same_v<T, Feature>) {
                    assign(require, &item, nullptr);
                } else if constexpr (std::is_same_v<T, Extension>) {
                    assign(require, nullptr, &item);
                } else {
                    static_assert(false);
                }

                // for (const auto &entry : xml::View(require.firstChild())) {
                //     const auto name = entry.optional("name");
                //     if (name) {
                //         supportedElements[std::string{name.value()}] = 10;
                //     }
                // }
            }
        }
    }

    void prepareFeatures()
    {
        prepareFeatureOrExtensions(reg.features);
        /*
        reg.features.items.reserve(xmlSupportedFeatures.size());
        MutableCollection features{reg.features};
        int id = 0;
        for (const auto &elem : xmlSupportedFeatures) {
            auto &feature = features.emplace_back(reg, elem);
            feature.id = ++id;
            supportedElements[feature.name.original] = 1;

            for (const auto &require : xml::elements(elem.firstChild(),
        "require"))
        {
                // if (require.optional("depends").has_value()) {
                //     continue;
                // }
                assignVersions(require, &feature, nullptr);

                for (const auto &entry : xml::View(require.firstChild())) {
                    const auto name = entry.optional("name");
                    if (name) {
                        supportedElements[std::string{name.value()}] = 10;
                    }
                }
            }
        }
        features.buildMapping();
        reg.features.ordered = makeOrdered(reg.features);
        reg.features.vulkan.clear();
        reg.features.vulkan.reserve(reg.features.items.size());
        for (auto &f : reg.features.items) {
            reg.features.vulkan.emplace_back(std::ref(f));
        }
        */
    }

    void prepareExtensions()
    {
        prepareFeatureOrExtensions(reg.extensions);
        /*
        MutableCollection extensions{reg.extensions};
        for (const auto &elem : xmlSupportedExtensions) {
            extensions.emplace_back(reg, elem);
            //
            // const auto name   = elem["name"];
            // // std::cout << "Extension: " << name << '\n';
            //
            // Platform  *platform       = nullptr;
            // const auto platformAttrib = elem.optional("platform");
            // if (platformAttrib) {
            //     platform = &platforms[platformAttrib.value()];
            // }
            //
            // bool enabled = defaultWhitelistOption;
            // // std::cout << "add ext: " << name << '\n';
            // auto &ext = extensions.emplace_back(std::string{ name },
        platform, true, enabled);
            // const auto number = elem.optional("number");
            // if (number) {
            //     ext.number = toInt(std::string(number.value()));
            // }
            // const auto comment = elem.optional("comment");
            // if (comment) {
            //     ext.comment = comment.value();
            // }
            // //            std::cout << "ext: " << ext << '\n';
            // //            std::cout << "> n: " << ext->name.original << '\n';
        }
        extensions.buildMapping();
        */
    }

    void initializeTypes(Generator& gen)
    {
        for (auto& type : reg.structs) {
            for (const auto& m : type.members) {
                m->updateMetaType(reg);
                if (m->isStructOrUnion()
                    // && !m->isPointer()
                    && m->original.type() != type.name.original) {
                    type.directDependencies.insert(m->original.type());
                }
                auto* d = reg.find(m->original.type());
                if (d) {
                    type.dependencies.insert(d);
                }
            }
        }
        for (auto& e : structExtends) {
            auto src = reg.structs.find(e.first.data());
            if (src) {
                auto dst = reg.structs.find(e.second.data());
                if (dst) {
                    dst->extends.emplace_back(&*src);
                }
            }
        }

        for (auto& h : reg.handles) {
            auto it = handleParent.find(h.name.original);
            if (it != handleParent.end()) {
                h.setParent(reg, reg.handles.find(it->second));
            }
        }
        for (auto& h : reg.handles) {
            auto it = getTopLevelHandle(h);
            if (it) {
                h.superclass = it->name;
            }
            h.isSubclass = &h != it;

            h.init(gen);
        }
        for (auto& h : reg.handles) {
            if (!h.isSubclass && h.parent) {
                h.superclass = h.parent->superclass;
            }
        }

        for (auto& command : reg.commands) {
            for (const auto& m : command._params) {
                m->updateMetaType(reg);
                const std::string& type = m->original.type();
                auto* d = reg.find(type);
                if (d) {
                    command.dependencies.insert(d);
                }
            }
            command.init(reg);
        }

        assignCommands(gen);

        for (auto& handle : reg.handles) {
            // std::cout << type.name.original << ": " << type.members.size() <<
            // " -> " << type.superclass.original << "\n";
            for (auto& m : handle.members) {
                // if (!filter(i, m)) {
                //     continue;
                // }
                for (auto& p : m.src->_params) {
                    if (p->isHandle() && !p->isPointer()) {
                        const auto& type = p->original.type();
                        if (type == "VkInstance" || type == "VkDevice" ||
                            type == handle.name.original) {
                            continue;
                        }
                        // std::cout << type.name.original << " Dependency: " <<
                        // p->original.type() << "\n";
                        handle.directDependencies.insert(p->original.type());
                    }
                }
                // auto p = m.src->getProtect();
                // if (!p.empty()) {
                //     i.plats.insert(std::string{ p });
                // }
            }
            // if (!type.directDependencies.empty()) {
            //     std::cout << type.name.original << " dependencies: {\n";
            //     for (const auto &d : type.directDependencies) {
            //         std::cout << "  " << d << "\n";
            //     }
            //     std::cout << "}\n";
            // }
        }

        // for (auto &e : reg.enums) {
        //     for (auto &a : e.aliases) {
        //         a.parentExtension = e.getExtension();
        //     }
        // }

        for (auto& h : reg.handles) {
            for (auto& c : h.ctorCmds) {
                h.dependencies.insert(c.src);
            }
            if (h.dtorCmd) {
                h.dependencies.insert(h.dtorCmd);
            }
        }
    }

    void removeUnsupported()
    {

        prepareFeatures();
        prepareExtensions();

        orderTypes();
        buildTypesMap();

        for (auto& extension : reg.extensions.items) {
            const auto promote = extension.xmlElement.optional("promotedto");
            if (promote) {
                auto feature = reg.features.find(promote.value());
                if (feature) {
                    for (Command& c : extension.commands) {
                        feature->promotedTypes.emplace_back(std::ref(c));
                    }
                    for (Struct& s : extension.structs) {
                        feature->promotedTypes.emplace_back(std::ref(s));
                    }
                    for (Enum& e : extension.enums) {
                        feature->promotedTypes.emplace_back(std::ref(e));
                    }
                }
            }
        }
    }

    void assignCommands(Generator& gen)
    {
        if (reg.handles.items.empty()) {
            return;
        }

        auto& instance = reg.findHandle("VkInstance");
        auto& device = reg.findHandle("VkDevice");

        std::vector<std::string_view> deviceObjects;
        std::vector<std::string_view> instanceObjects;

        for (auto& h : reg.handles.items) {
            if (h.name.original == "VkDevice" || h.superclass == "Device") {
                deviceObjects.push_back(h.name.original);
                // std::cout << "Device level: " << h.name.original << ", " <<
                // h.superclass << "\n";
            } else if (h.name.original == "VkInstance" ||
                       h.superclass == "Instance") {
                instanceObjects.push_back(h.name.original);
                // std::cout << "Instance level: " << h.name.original << ", " <<
                // h.superclass << "\n";
            } else {
                std::cout << "Unassigned: " << h.name.original << ", "
                          << h.superclass << "\n";
            }
        }

        const auto addCommand =
            [&](const std::string& type, Handle& handle, Command& command) {
                bool indirect = type != handle.name.original &&
                                command.isIndirectCandidate(type);

                handle.addCommand(gen, command);

                // std::cout << "=> added direct to: " << handle.name << ", " <<
                // command.name << '\n';
                if (indirect) {
                    command.setFlagBit(Command::CommandFlags::INDIRECT, true);
                    auto& handle = reg.findHandle(type);
                    handle.addCommand(gen, command);
                    // std::cout << "=> added indirect to: " << handle.name <<
                    // ", " << command.name << '\n';
                }

                Handle* second = command.secondIndirectCandidate(gen);
                if (second) {
                    if (second->superclass.original == type) {
                        second->addCommand(gen, command, true);
                        // std::cout << "=> added 2nd indirect to: " <<
                        // second->name << ", "
                        // << command.name << '\n';
                    }
                }
            };

        const auto assignGetProc = [&](Handle& handle, Command& command) {
            if (command.name.original == "vkGet" + handle.name + "ProcAddr") {
                handle.getAddrCmd.emplace(&gen, &handle, command);
                return true;
            }
            return false;
        };

        const auto assignConstruct = [&](Command& command) {
            if (!command.createsHandle() || command.isAlias()) {
                return;
            }

            auto last = command.getLastVar();
            if (!last) {
                std::cerr << "null access " << command.name << '\n';
                return;
            }

            std::string type = last->original.type();
            if (!last->isPointer() || !last->isHandle()) {
                return;
            }

            try {
                auto& handle = reg.findHandle(type);
                if (last->isArrayOut() &&
                    (command.nameCat == Command::NameCategory::CREATE ||
                     command.nameCat == Command::NameCategory::ALLOCATE ||
                     command.nameCat == Command::NameCategory::ENUMERATE)) {
                    handle.vectorVariant = true;
                    handle.vectorCmds.emplace_back(&gen, &handle, command);
                } else {
                    handle.ctorCmds.emplace_back(&gen, &handle, command);
                }
            } catch (std::runtime_error e) {
                std::cerr << "warning: can't assign constructor: " << type
                          << " (from " << command.name << "): " << e.what()
                          << '\n';
            }
        };

        const auto assignDestruct2 = [&](Command& command,
                                         Handle::CreationCategory cat) {
            try {
                auto last = command.getLastHandleVar();
                if (!last) {
                    throw std::runtime_error("can't get param (last handle)");
                }

                std::string type = last->original.type();

                auto& handle = reg.findHandle(type);
                if (handle.dtorCmd) {
                    // std::cerr << "warning: handle already has destruct
                    // command:" << handle.name << '\n';
                    return;
                }

                handle.creationCat = cat;
                handle.setDestroyCommand(gen, command);
            } catch (std::runtime_error e) {
                std::cerr << "warning: can't assign destructor: " << " (from "
                          << command.name << "): " << e.what() << '\n';
            }
        };

        const auto assignDestruct = [&](Command& command) {
            if (command.name.starts_with("destroy")) {
                assignDestruct2(command, Handle::CreationCategory::CREATE);
            } else if (command.name.starts_with("free")) {
                assignDestruct2(command, Handle::CreationCategory::ALLOCATE);
            }
        };

        for (Command& command : reg.commands.items) {
            if (assignGetProc(instance, command) ||
                assignGetProc(device, command)) {
                continue;
            }

            std::string first;
            bool isHandle = false;
            if (command.hasParams()) {
                assignConstruct(command);
                assignDestruct(command);
                // type of first argument
                auto& p = command.params.begin()->get();
                first = p.original.type();
                isHandle = p.isHandle();
            }

            if (!isHandle && !command.aliasParent) {
                reg.staticCommands.emplace_back(command);
                gen.loader.addCommand(gen, command);
                command.top = &gen.loader;
                continue;
            }

            if (isInContainter(deviceObjects, first)) { // command is for device
                addCommand(first, device, command);
                command.top = &device;
            } else if (isInContainter(
                           instanceObjects, first
                       )) { // command is for instance
                addCommand(first, instance, command);
                command.top = &instance;
            } else {
                std::cerr << "warning: can't assign command: " << command.name
                          << ", " << first << '\n';
            }
        }
        for (Command& command : reg.commands.items) {
            if (command.aliasParent) {
                command.top = reinterpret_cast<Command*>(command.aliasParent)->top;
            }
        }


        // std::cout << "instance: " << instance.members.size() << "
        // commands\n"; std::cout << "device: " << device.members.size() << "
        // commands\n";
        if (reg.verbose)
            std::cout << "Assign commands done" << '\n';
    }

    template<typename T>
    static void sortByID(std::vector<std::reference_wrapper<T>>& items)
    {
        std::sort(items.begin(), items.end(), [](const T& a, const T& b) {
            return a.id < b.id;
        });
    }

    template<typename T>
    void addDependencies(
        Extension& extension,
        std::vector<std::reference_wrapper<T>>& items
    )
    {
        for (const auto& item : items) {
            for (const auto& dep : item.get().dependencies) {
                auto* ext = dep->getExtension();
                if (ext && ext != &extension) {
                    // std::cout << extension.name << " -> " << ext->name << ":
                    // " << item.name.original << ", " << dep->name.original <<
                    // "\n";
                    extension.directDependencies.insert(ext->name.original);
                }
            }
        }
    }

  public:
    RegistryLoader(Registry& reg)
        : reg(reg)
    {
    }

    bool load(Generator& gen, const std::string& xmlPath)
    {

        if (reg.isLoaded()) {
            reg.unload();
        }

        if (!loadXML(xmlPath)) {
            reg.unload();
            return false;
        }

        parseXML(gen);

        initializeTypes(gen);
        removeUnsupported();

        for (const auto& r : typeRequires) {

            const auto& req = r.first;
            auto it = reg.includes.find(req);
            if (it == reg.includes.end()) {
                std::cerr << "Parse error: missing include node: " << req
                          << "\n";
                continue;
            }
            auto& inc = it->second;
            const auto& name = r.second;

            for (const auto& s : reg.structs) {
                for (const auto& m : s.members) {
                    if (m->original.type() == name) {
                        auto* platform = s.getPlatfrom();
                        if (platform) {
                            platform->includes.insert(inc);
                        } else {
                            auto* ext = s.getExtension();
                            if (!ext) {
                                // std::cerr << "found: " << name << " -> " <<
                                // req << "\n";
                            } else {
                                // std::cout << "found: " << name << " -> " <<
                                // req << " -> " << ext->name << "\n";
                                bool dup = false;
                                for (const auto& i : ext->includes) {
                                    if (i.get() == inc) {
                                        dup = true;
                                        break;
                                    }
                                }
                                if (!dup) {
                                    ext->includes.emplace_back(std::ref(inc));
                                }
                            }
                        }

                        break;
                    }
                }
            }
            for (const auto& c : reg.commands) {
                for (const auto& p : c._params) {
                    auto* platform = c.getPlatfrom();
                    if (!platform) {
                        continue;
                    }
                    if (p->original.type() == name) {
                        platform->includes.insert(inc);
                        break;
                    }
                }
            }
        }
        //        for (const auto &p : platforms) {
        //            std::cout << p.name << "\n";
        //            if (!p.includes.empty()) {
        //                for (const auto &i : p.includes) {
        //                    std::cout << "  -> " << i << "\n";
        //                }
        //            }
        //        }

        for (auto& c : reg.commands) {
            if (c.destroysObject()) {
                auto* handle = c.getLastHandleVar();
                if (handle) {
                    handle->overrideOptional(false);
                }
            }
        }

        for (auto& f : reg.features) {
            sortByID(f.enums);
            sortByID(f.structs);
            sortByID(f.commands);
        }

        for (auto& e : reg.extensions) {
            sortByID(e.enums);
            sortByID(e.structs);
            sortByID(e.commands);
        }

        for (auto& e : reg.extensions) {
            addDependencies(e, e.enums);
            addDependencies(e, e.structs);
            addDependencies(e, e.commands);
        }
        sortDependencies(reg.extensions);

        reg.platforms.ordered = makeOrdered(reg.platforms);
        reg.platforms.vulkan.clear();
        reg.platforms.vulkan.reserve(reg.platforms.items.size());
        for (auto& f : reg.platforms.items) {
            reg.platforms.vulkan.emplace_back(std::ref(f));
        }

        for (Extension& e : reg.extensions.ordered) {
            auto* platform = e.platform;
            // std::cout << platform << "\n";
            if (platform) {
                platform->extensions.emplace_back(std::ref(e));
                // bool dup = false;
                // for (const auto &p : reg.platforms.ordered) {
                //     if (&p.get() == platform) {
                //         dup = true;
                //         break;
                //     }
                // }
                // if (!dup) {
                //     reg.platforms.ordered.emplace_back(std::ref(*platform));
                // }
            }
            for (FuncPointer& f : e.funcPointers) {
                for (const Struct& s : e.structs) {
                    for (const auto& m : s.members) {
                        if (m->original.type() == f.name.original) {
                            // std::cout << "in struct: " << f.name.original <<
                            // "\n";
                            f.inStruct = true;

                            std::regex r("Vk[a-zA-Z0-9_]+");
                            for (std::sregex_iterator i = std::sregex_iterator(
                                     f.code.begin(), f.code.end(), r
                                 );
                                 i != std::sregex_iterator();
                                 ++i) {
                                const std::smatch& m = *i;

                                auto it = reg.structs.find(m.str());
                                if (it) {
                                    it->needForwardDeclare = true;
                                    auto* feature = s.getFeature();
                                    // std::cout << "  -> " << m.str() << "  "
                                    // << feature << '\n';
                                    if (feature) {
                                        Feature::insert(
                                            feature->forwardStructs, *it
                                        );
                                    }
                                }
                            }

                            break;
                        }
                    }
                }
            }
        }

        for (auto& feature : reg.features) {
            for (FuncPointer& f : feature.funcPointers) {
                for (const Struct& s : feature.structs) {
                    for (const auto& m : s.members) {
                        if (m->original.type() == f.name.original) {
                            f.inStruct = true;
                            break;
                        }
                    }
                }
            }
        }

        for (auto& e : reg.enums) {
            for (auto& m : e.members) {
                if (!m.alias.empty()) {
                    auto* src = e.find(m.alias);
                    if (src) {
                        m.value = src->value;
                        // m.value += " // " + m.alias;
                    }
                }
                if (m.value.empty()) {
                    std::cout << "warn: " << m.name.original
                              << " has no value\n";
                }
            }
            if (e.type.empty()) {
                std::cout << "warn: " << e.name.original << " has no type\n";
            }
        }

        const auto lockDependency = [&](const std::string& name) {
            auto* type = reg.find(name);
            if (type) {
                type->forceRequired = true;
            }
        };

        lockDependency("VkStructureType");
        lockDependency("VkResult");
        lockDependency("VkObjectType");
        lockDependency("VkDebugReportObjectTypeEXT");
        lockDependency("vkEnumerateInstanceVersion");

        for (auto& c : reg.enums) {
            c.setEnabled(true);
        }
        for (auto& c : reg.structs) {
            c.setEnabled(true);
        }
        for (auto& c : reg.handles) {
            c.setEnabled(true);
        }
        for (auto& c : reg.commands) {
            c.setEnabled(true);
        }
        for (auto& d : reg.enums) {
            d.setEnabled(true);
        }

        for (auto& c : reg.platforms) {
            c.setEnabled(true);
        }
        for (auto& c : reg.features) {
            c.setEnabled(true);
        }
        for (auto& c : reg.extensions) {
            c.setEnabled(true);
        }

#ifdef INST
        std::vector<std::string> cmds;
        cmds.reserve(commands.size());
        for (const auto& c : commands.items) {
            cmds.emplace_back(c.name.original);
        }
        Inst::processCommands(cmds);
#endif

        for (auto& s : reg.structs) {
            for (const auto& m : s.members) {
                auto p = s.getProtect();
                if (!m->isPointer() && m->isStructOrUnion()) {
                    auto pm = reg.structs[m->original.type()].getProtect();
                    // std::cout << pm << '\n';
                    if (!p.empty() && !pm.empty() && p != pm) {
                        std::cout << ">> platform dependency: " << p << " -> "
                                  << pm << '\n';
                    }
                }
            }
        }

        return true;
    }
};

Platform::Platform(Registry& reg, const xml::Element& element)
    : GenericType(MetaType::Value::Platform)
{
    name.reset(std::string{ element["name"] });
    protect = element["protect"];
}

Extension::Extension(Registry& reg, const xml::Element& element)
    : Feature(reg, element)
{
    setMetaType(MetaType::Value::Extension);

    const auto platformAttrib = element.optional("platform");
    if (platformAttrib) {
        platform = &reg.platforms[platformAttrib.value()];
        protect = platform->protect;
    }

    const auto numberAttrib = element.optional("number");
    if (numberAttrib) {
        number = toInt(std::string(numberAttrib.value()));
    }
    const auto commentAttrib = element.optional("comment");
    if (commentAttrib) {
        comment = commentAttrib.value();
    }
}

void
Registry::loadRegistryPath()
{
    loadSystemRegistryPath();
    loadLocalRegistryPath();
    //        std::cout << "Lookup paths: \n";
    //        std::cout << systemRegistryPath << "\n";
    //        std::cout << localRegistryPath << "\n";
}

void
Registry::loadSystemRegistryPath()
{
    size_t size = 0;
    getenv_s(&size, nullptr, 0, "VULKAN_SDK");
    if (size == 0) {
        return;
    }

    std::string sdk;
    sdk.resize(size);
    getenv_s(&size, sdk.data(), sdk.size(), "VULKAN_SDK");
    sdk.resize(size - 1);

    auto regPath = fs::path{ sdk } / fs::path{ "share/vulkan/registry/vk.xml" };
    if (fs::exists(regPath)) {
        systemRegistryPath = fs::absolute(regPath).string();
    }
}

void
Registry::loadLocalRegistryPath()
{
    const fs::path regPath{ "vk.xml" };
    if (fs::exists(regPath)) {
        localRegistryPath = fs::absolute(regPath).string();
    }
}

std::string
Registry::getDefaultRegistryPath()
{
    return localRegistryPath.empty() ? systemRegistryPath : localRegistryPath;
}

std::string
Registry::strRemoveTag(std::string& str) const
{
    if (str.empty()) {
        return "";
    }
    std::string suffix;
    auto it = str.rfind('_');
    if (it != std::string::npos) {
        suffix = str.substr(it + 1);
        if (tags.find(suffix) != tags.end()) {
            str.erase(it);
        } else {
            suffix.clear();
        }
    }

    for (const auto& t : tags) {
        if (str.ends_with(t)) {
            str.erase(str.size() - t.size());
            return t;
        }
    }
    return suffix;
}

std::string
Registry::strWithoutTag(const std::string& str) const
{
    std::string out = str;
    for (const std::string& tag : tags) {
        if (out.ends_with(tag)) {
            out.erase(out.size() - tag.size());
            break;
        }
    }
    return out;
}

bool
Registry::strEndsWithTag(const std::string_view str) const
{
    return std::ranges::any_of(tags, [&](const auto& tag) {
        return str.ends_with(tag);
    });
}

std::string
Registry::snakeToCamel(std::string str) const
{
    const std::string suffix = strRemoveTag(str);
    std::string out = convertSnakeToCamel(str);

    out = std::regex_replace(out, std::regex("bit"), "Bit");
    out = std::regex_replace(out, std::regex("Rgba10x6"), "Rgba10X6");
    out = std::regex_replace(out, std::regex("1d"), "1D");
    out = std::regex_replace(out, std::regex("2d"), "2D");
    out = std::regex_replace(out, std::regex("3d"), "3D");
    if (out.size() >= 2) {
        for (int i = 0; i < out.size() - 1; i++) {
            const char& c = out[i];
            const bool rgba = c == 'r' || c == 'g' || c == 'b' || c == 'a';
            if (rgba && std::isdigit(out[i + 1])) {
                out[i] = std::toupper(c);
            }
        }
    }

    return out + suffix;
}

std::string
Registry::enumConvertCamel(
    const std::string& enumName,
    std::string value,
    bool isBitmask
) const
{
    std::string dbg = value;

    strStripPrefix(value, "VK_");

    std::string out;
    if (!enumName.empty()) {
        std::string enumSnake = enumName;
        std::string tag = strRemoveTag(enumSnake);
        if (!tag.empty()) {
            tag = "_" + tag;
        }
        enumSnake = camelToSnake(enumSnake);
        strStripPrefix(enumSnake, "VK_");

        const auto& tokens = split(enumSnake, "_");
        for (const auto& token : tokens) {
            if (value.starts_with(token)) {
                value.erase(0, token.size());
                if (value.starts_with('_')) {
                    value.erase(0, 1);
                }
            }
        }
        if (value.ends_with(tag)) {
            value.erase(value.size() - tag.size());
        }

        for (const auto& it : std::ranges::reverse_view(tokens)) {
            const std::string token = "_" + it;
            if (!value.ends_with(token)) {
                break;
            }
            value.erase(value.size() - token.size());
        }

        out = "e";
    }

    out += strFirstUpper(snakeToCamel(value));
    if (isBitmask) {
        std::string tag = strRemoveTag(out);
        strStripSuffix(out, "Bit");
        if (!tag.empty()) {
            out += tag;
        }
    }
    return out;
}

bool
Registry::containsFuncPointer(const Struct& data) const
{
    for (const auto& m : data.members) {
        const auto& type = m->original.type();
        if (type.starts_with("PFN_")) {
            return true;
        }
        if (type != data.name.original) {
            const auto& s = structs.find(type);
            if (s) {
                if (containsFuncPointer(*s)) {
                    return true;
                }
            }
        }
    }
    return false;
}

void
Registry::orderCommands()
{
    std::sort(
        commands.ordered.begin(),
        commands.ordered.end(),
        [](const Command& a, const Command& b) {
            return a.successCodes.size() < b.successCodes.size();
        }
    );

    return;
    /*
    const auto findCode = [](const Command &cmd, const std::string_view code) {
        for (const auto &c : cmd.successCodes) {
            if (c == code) {
                return true;
            }
        }
        return false;
    };

    std::map<int, int> hist;
    int arraycnt = 0;

    for (const Command &c : commands.ordered) {

        bool retarray = false;
        if (c.successCodes.size() >= 2) {
            if (findCode(c, "VK_SUCCESS") && findCode(c, "VK_INCOMPLETE")) {
                retarray = true;
            }
        }
        if (!retarray) {
            hist[c.successCodes.size()]++;
        }
        else {
            arraycnt++;
        }

        if (c.successCodes.size() < 2 || retarray)
            continue;

        continue;
        if (c.pfnReturn == Command::PFNReturnCategory::VOID) {
            std::cout << "void ";
        }
        std::cout << c.name.original << "[" << c.successCodes.size() << "]: { ";
        for (const auto &code : c.successCodes) {
            std::cout << code << ", ";
        }
        std::cout << "}\n";
    }

    std::cout << "All functions: " << commands.size() << "\n";
    for (const auto &k : hist) {
        std::cout << k.first << " ret functions: " << k.second << "\n";
    }
    std::cout << "vector ret functions: " << arraycnt << "\n";
    */
}

GenericType&
Registry::get(const std::string& name)
{
    auto* it = find(name);
    if (!it) {
        throw std::runtime_error(
            "Error: " + std::string{ name } + " not found in reg"
        );
    }
    return *it;
}

GenericType*
Registry::find(const std::string& name) noexcept
{
    // assert(!types.empty() && "type map not build yet\n");

    if (auto type = types.find(name); type != types.end()) {
        return type->second;
    }
    if (auto type = commands.find(name); type) {
        return type;
    }
    if (auto type = structs.find(name); type) {
        return type;
    }
    if (auto type = enums.find(name); type) {
        return type;
    }
    if (auto type = handles.find(name); type) {
        return type;
    }
    if (auto type = baseTypes.find(name); type != baseTypes.end()) {
        return &type->second;
    }
    if (auto type = funcPointers.find(name); type != funcPointers.end()) {
        return &type->second;
    }
    // if (auto type = aliases.find(name); type != aliases.end()) {
    //     return &type->second.get();
    // }
    return nullptr;
}

bool
Registry::load(Generator& gen, const std::string& xmlPath)
{

    RegistryLoader loader{ *this };
    if (!loader.load(gen, xmlPath)) {
        return false;
    }

    const auto printDependencies = []<typename T>(const T& items) {
        // std::cout << "size: " << items.size() << "\n";
        // for (const auto &i : items) {
        //     if (!i.dependencies.empty()) {
        //         std::cout << i.name.original << ": { ";
        //         for (const auto &d : i.dependencies) {
        //             std::cout << d->name.original << ", ";
        //         }
        //         std::cout << "}\n";
        //     }
        // }
    };

    printDependencies(enums);
    printDependencies(structs);
    printDependencies(handles);
    printDependencies(commands);
    printDependencies(features);
    printDependencies(extensions);

    registryPath = xmlPath;
    loaded = true;
    loadFinished();
    return true;
}

void
Registry::loadFinished()
{
    if (onLoadCallback) {
        onLoadCallback();
    }
}

void
Registry::bindGUI(const std::function<void()>& onLoad)
{
    onLoadCallback = onLoad;

    if (isLoaded()) {
        loadFinished();
    }
}

void
Registry::unload()
{

    baseTypes.clear();
    apiConstants.clear();
    types.clear();

    platforms.clear();
    tags.clear();
    enums.clear();
    handles.clear();
    structs.clear();
    extensions.clear();
    staticCommands.clear();
    commands.clear();

    loaded = false;
    registryPath = "";
}

std::string
Registry::to_string(Command::PFNReturnCategory value)
{
    using enum Command::PFNReturnCategory;
    switch (value) {
        case OTHER:
            return "OTHER";
        case VOID:
            return "VOID";
        case VK_RESULT:
            return "VK_RESULT";
        default:
            return "";
    }
};

std::string
Registry::to_string(Command::NameCategory value)
{
    using enum Command::NameCategory;
    switch (value) {
        case UNKNOWN:
            return "UNKNOWN";
        case GET:
            return "GET";
        case ALLOCATE:
            return "ALLOCATE";
        case ACQUIRE:
            return "ACQUIRE";
        case CREATE:
            return "CREATE";
        case ENUMERATE:
            return "ENUMERATE";
        case WRITE:
            return "WRITE";
        case DESTROY:
            return "DESTROY";
        case FREE:
            return "FREE";
        default:
            return "";
    }
}

VulkanRegistry::VulkanRegistry(Generator& gen)
    : loader(gen)
{
    loader.name.convert("VkContext", true);
    loader.forceRequired = true;
}

void
VulkanRegistry::createErrorClasses()
{
    auto& e = enums["VkResult"];
    std::unordered_set<std::string> values;
    for (const auto& m : e.members) {
        if (!m.isAlias && m.name.starts_with("eError")) {
            if (values.find(m.value) != values.end()) {
                continue;
            }
            values.insert(m.value);
            errorClasses.emplace_back(m);
        }
    }
}

bool
VulkanRegistry::load(Generator& gen, const std::string& xmlPath)
{
    namespace fs = std::filesystem;

    unload();

    fs::path path = xmlPath.empty() ? getDefaultRegistryPath() : xmlPath;
    if (!path.has_filename()) {
        path.replace_filename("vk.xml");
    }
    if (!fs::exists(path)) {
        return false;
    }

    std::filesystem::path videoPath =
        fs::path(path).replace_filename("video.xml");
    if (std::filesystem::exists(videoPath)) {
        video = std::make_unique<VideoRegistry>();
        video->load(gen, videoPath.string());
    }

    auto result = Registry::load(gen, path.string());
    if (!result) {
        return false;
    }

    auto it = defines.find("VK_HEADER_VERSION");
    if (it != defines.end()) {
        const auto& text = it->second.code;
        ;
        auto pos = text.rfind(' ');
        if (pos != std::string::npos) {
            headerVersion = text.substr(pos);
            // std::cout << ">> VK_HEADER_VERSION: " << headerVersion << "\n";
        }
    }
    if (headerVersion.empty()) {
        throw std::runtime_error("header version not found.");
    }

    createErrorClasses();

    for (auto& h : handles) {
        if (!h.isSubclass) {
            topLevelHandles.emplace_back(std::ref(h));
        }
    }

    const auto setForward = [&](const std::string_view name) {
        auto s = structs.find(name);
        if (s) {
            auto* extension = s->getExtension();
            if (extension) {
                Feature::insert(extension->forwardStructs, *s);
            }
        }
    };

    setForward("VkDebugUtilsMessengerCallbackDataEXT");
    setForward("VkDeviceMemoryReportCallbackDataEXT");

    std::cout << "Loader functions: ";
    for (const auto &m : loader.members) {
        std::cout << "  " << m.name << "\n";
    }
    std::cout << "\n";

    return result;
}

void
VulkanRegistry::unload()
{
    topLevelHandles.clear();
    headerVersion.clear();
    errorClasses.clear();
    loader.clear();
    video = nullptr;

    Registry::unload();
}

} // namespace vkgen
