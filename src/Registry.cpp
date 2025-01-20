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

#include "Registry.hpp"

#include "Generator.hpp"
#include "Format.hpp"

#include <filesystem>
#include <ranges>
#include <utility>
#include <vector>
#include <string>
#include <functional>
#include <cassert>

static constexpr auto no_ver = "";

namespace fs = std::filesystem;

namespace vkgen
{

    std::string Registry::systemRegistryPath;
    std::string Registry::localRegistryPath;

    std::string GenericType::getVersionDebug() const {
        std::string out = "// ";
        out += name.original;
        out += " (";
        out += metaTypeString();
        out += ") ";
        if (version) {
            out += version;
        }
        if (ext) {
            out += "  ext: ";
            out += ext->name;
            out += " (";
            out += std::to_string(ext->number);
            out += ")";
        }
        out += tempversion;
        out += "\n";
        return out;
    }

    GenericType::GenericType(const GenericType &parent, std::string_view name, bool firstCapital)
      : MetaType(parent.metaType()),
      name(std::string{ name }, firstCapital),
      parentExtension(parent.ext)
    {
    }

    std::string_view GenericType::getProtect() const {
        return protect;
    }

    vkr::Platform* GenericType::getPlatfrom() const {
        return ext? ext->platform : nullptr;
    }

    void GenericType::setProtect(const std::string_view protect) {
        this->protect = protect;
    }

    void GenericType::setExtension(vkr::Extension *ext) {
        if (ext) {
            if (this->ext && ext->number > this->ext->number) {
//                if (!tempversion.empty()) {
//                    tempversion += ", ";
//                }
//                tempversion += std::to_string(ext->number);
                return;
            }
            if (!ext->protect.empty()) {
                protect = ext->protect;
            }
            this->ext = ext;
            // this->feature = ext;
        }
    }

    void GenericType::bind(vkr::Feature *feature, vkr::Extension *ext, const std::string_view protect) {
        setExtension(ext);
        if (feature) {
            this->feature = feature;
        }
        if (!protect.empty()) {
            this->protect = protect;
        }
    }

    void Registry::loadRegistryPath() {
        loadSystemRegistryPath();
        loadLocalRegistryPath();
//        std::cout << "Lookup paths: \n";
//        std::cout << systemRegistryPath << "\n";
//        std::cout << localRegistryPath << "\n";
    }

    void Registry::loadSystemRegistryPath() {
        size_t size = 0;
        getenv_s(&size, nullptr, 0, "VULKAN_SDK");
        // std::cout << "getenv_s: " << size << "\n";
        if (size == 0) {
            return;
        }

        std::string sdk;
        sdk.resize(size);
        getenv_s(&size, sdk.data(), sdk.size(), "VULKAN_SDK");
        // std::cout << "sdk path: " << sdk << "\n";
        sdk.resize(size - 1);

        auto regPath = fs::path{ sdk } / fs::path{ "share/vulkan/registry/vk.xml" };
        if (fs::exists(regPath)) {
            systemRegistryPath = fs::absolute(regPath).string();
        }
    }

    void Registry::loadLocalRegistryPath() {
        const fs::path regPath{ "vk.xml" };
        if (fs::exists(regPath)) {
            localRegistryPath = fs::absolute(regPath).string();
        }
    }

    std::string Registry::getDefaultRegistryPath() {
        if (localRegistryPath.empty()) {
            return systemRegistryPath;
        }
        return localRegistryPath;
    }

    std::string Registry::strRemoveTag(std::string &str) const {
        if (str.empty()) {
            return "";
        }
        std::string suffix;
        auto        it = str.rfind('_');
        if (it != std::string::npos) {
            suffix = str.substr(it + 1);
            if (tags.find(suffix) != tags.end()) {
                str.erase(it);
            } else {
                suffix.clear();
            }
        }

        for (const auto &t : tags) {
            if (str.ends_with(t)) {
                str.erase(str.size() - t.size());
                return t;
            }
        }
        return suffix;
    }

    std::string Registry::strWithoutTag(const std::string &str) const {
        std::string out = str;
        for (const std::string &tag : tags) {
            if (out.ends_with(tag)) {
                out.erase(out.size() - tag.size());
                break;
            }
        }
        return out;
    }

    bool Registry::strEndsWithTag(const std::string_view str) const {
        return std::ranges::any_of(tags, [&](const auto &tag) { return str.ends_with(tag); });
    }

    std::string Registry::snakeToCamel(std::string str) const {
        const std::string suffix = strRemoveTag(str);
        std::string       out    = convertSnakeToCamel(str);

        out = std::regex_replace(out, std::regex("bit"), "Bit");
        out = std::regex_replace(out, std::regex("Rgba10x6"), "Rgba10X6");
        out = std::regex_replace(out, std::regex("1d"), "1D");
        out = std::regex_replace(out, std::regex("2d"), "2D");
        out = std::regex_replace(out, std::regex("3d"), "3D");
        if (out.size() >= 2) {
            for (int i = 0; i < out.size() - 1; i++) {
                const char &c    = out[i];
                const bool  rgba = c == 'r' || c == 'g' || c == 'b' || c == 'a';
                if (rgba && std::isdigit(out[i + 1])) {
                    out[i] = std::toupper(c);
                }
            }
        }

        return out + suffix;
    }

    std::string Registry::enumConvertCamel(const std::string &enumName, std::string value, bool isBitmask) const {
        std::string dbg = value;

        strStripPrefix(value, "VK_");

        std::string out;
        if (!enumName.empty()) {
            std::string enumSnake = enumName;
            std::string tag       = strRemoveTag(enumSnake);
            if (!tag.empty()) {
                tag = "_" + tag;
            }
            enumSnake = camelToSnake(enumSnake);
            strStripPrefix(enumSnake, "VK_");

            const auto &tokens = split(enumSnake, "_");
            for (const auto &token  : tokens) {
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

            for (const auto &it : std::ranges::reverse_view(tokens)) {
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
            std::string tag       = strRemoveTag(out);
            strStripSuffix(out, "Bit");
            if (!tag.empty()) {
                out += tag;
            }
        }
        return out;
    }

    bool Registry::containsFuncPointer(const Struct &data) const {
        for (const auto &m : data.members) {
            const auto &type = m->original.type();
            if (type.starts_with("PFN_")) {
                return true;
            }
            if (type != data.name.original) {
                const auto &s = structs.find(type);
                if (s != structs.end()) {
                    if (containsFuncPointer(*s)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    String& Registry::getHandleSuperclass(const Handle &data) {
        auto *it = data.parent;
        while (it->parent) {
            if (it->name.original == "VkInstance" || it->name.original == "VkDevice") {
                break;
            }
            it = it->parent;
        }
        return it->name;
    }

    void Registry::parsePlatforms(Generator &gen, xml::Element elem, xml::Element children) {
        if (verbose)
            std::cout << "Parsing platforms" << '\n';

        // iterate contents of <platforms>, filter only <platform> children
        for (const auto &platform : xml::vulkanElements(children, "platform")) {
            auto name    = platform["name"];
            auto protect = platform["protect"];
            auto &p   = platforms.items.emplace_back(name, protect, defaultWhitelistOption);
            p.version = no_ver;
        }
        platforms.prepare();

        if (verbose)
            std::cout << "Parsing platforms done" << '\n';
    }

    void Registry::parseTags(Generator &gen, xml::Element elem, xml::Element children) {
        if (verbose)
            std::cout << "Parsing tags" << '\n';
        // iterate contents of <tags>, filter only <tag> children
        for (const auto &tag : xml::elements(children, "tag")) {
            auto name = tag["name"];
            tags.emplace(name);
        }
        if (verbose)
            std::cout << "Parsing tags done" << '\n';
    }

    template <typename T>
    class ItemInserter
    {
      public:
        using Alias = std::pair<std::string, std::string>;

        T                 &storage;
        std::vector<Alias> aliased;
        bool firstCapital;

        void addAliases(const std::function<void(Alias &)> &func) {
            std::for_each(aliased.begin(), aliased.end(), std::move(func));
        }

        void addAliases() {
            for (const auto &a : aliased) {
                const auto &name  = a.first;
                const auto &alias = a.second;
                auto        dst   = storage.find(alias);
                if (dst == storage.end()) {
                    std::cerr << "Error: aliased type not found: " << alias << " -> " << name << '\n';
                    continue;
                }
                dst->addAlias(name, firstCapital);
            }
        }

      public:
        explicit ItemInserter(T &storage, bool firstCapital) : storage(storage), firstCapital(firstCapital) {}

        template <class... Args>
        void insert(Generator &gen, xml::Element e, const std::string_view name, Args&&... args) {
            const auto alias = e.optional("alias");
            if (alias) {
                aliased.emplace_back(e.optional("name").value_or(name), std::string(alias.value()));
            } else {
                storage.items.emplace_back(gen, e, name, std::forward<Args>(args)...);
            }
        }

        void addAlias(const std::string &name, const std::string &alias) {
            aliased.emplace_back(name, alias);
        }

        void finalize() {
            storage.prepare();
            addAliases();
        }
    };

    void Registry::parseTypes(Generator &gen, xml::Element elem, xml::Element children) {
        if (verbose)
            std::cout << "Parsing declarations" << '\n';

        ItemInserter enumsInserter  { enums,   true };
        ItemInserter structsInserter{ structs, true };
        ItemInserter handlesInserter{ handles, true };

        // iterate contents of <types>, filter only <type> children
        for (const auto &type : xml::vulkanElements(children, "type")) {
            const auto categoryAttrib = type.optional("category");
            const auto name = type.optional("name");
            if (!categoryAttrib) {
                const auto requiresAttrib = type.optional("requires");
                if (name && requiresAttrib && requiresAttrib.value() != "vk_platform") {
                    parse->typeRequires.emplace_back(requiresAttrib.value(), name.value());
                }
                continue;
            }
            const auto cat  = categoryAttrib.value();


            if (cat == "enum") {
                if (name && name->find("FlagBits") == std::string::npos) {
                    enumsInserter.insert(gen, type, name.value(), "VkFlags");
                }
            } else if (cat == "bitmask") {
                std::string name  = std::string(type.getNested("name"));
                std::string ctype  = std::string(type.getNested("type"));
                enumsInserter.insert(gen, type, name, ctype, true);
            } else if (cat == "handle") {
                XMLTextParser parser{ type };
                handlesInserter.insert(gen, type, "", std::move(parser.text));
            } else if (cat == "struct" || cat == "union") {
                if (name) {
                    const auto alias = type.optional("alias");
                    if (alias) {
                        structsInserter.addAlias(std::string(name.value()), std::string(alias.value()));
                    } else {
                        MetaType::Value const metaType = (cat == "struct") ? MetaType::Struct : MetaType::Union;

                        auto &s = structs.items.emplace_back(gen, name.value(), metaType, type);

                        auto extends = type.optional("structextends");
                        if (extends) {
                            for (const auto &e : split2(extends.value(), ",")) {
                                parse->structExtends.emplace_back(name.value(), e);
                            }
                        }
                    }
                }
            } else if (cat == "define") {
                XMLTextParser parser{ type };
                const auto &name = parser["name"];
                defines.emplace(std::piecewise_construct, std::make_tuple(name), std::make_tuple(name, std::move(parser.text)));
            } else if (cat == "basetype") {
                XMLTextParser parser{ type };
                const auto &name = parser["name"];
                baseTypes.emplace(std::piecewise_construct, std::make_tuple(name), std::make_tuple(name, std::move(parser.text)));
            } else if (cat == "funcpointer") {
                XMLTextParser parser{ type };
                const auto &name = parser["name"];
                funcPointers.emplace(std::piecewise_construct, std::make_tuple(name), std::make_tuple(name, std::move(parser.text)));
            }
            else if (cat == "include") {
                const auto &name = type["name"];
                const auto *text = type->GetText();
                std::string inc;
                if (text) {
                    inc = text;
                }
                else {
                    inc = "#include <";
                    inc += name;
                    inc += ">\n";
                }
                includes.emplace(name, inc);
            }
        }

        handlesInserter.finalize();
        enumsInserter.finalize();
        structsInserter.finalize();

        if (verbose)
            std::cout << "Parsing declarations done" << '\n';
    }

    void Registry::parseApiConstants(Generator &gen, xml::Element elem) {

        std::map<std::string, xml::Element> aliased;
        for (const auto &e : xml::elements(elem.firstChild(), "enum")) {
            const auto alias = e.optional("alias");
            if (alias) {
                aliased.emplace(std::string(alias.value()), e);
                continue;
            }

            const auto name  = std::string(e["name"]);
            const auto type  = std::string(e["type"]);
            const auto value = std::string(e["value"]);
            apiConstants.emplace_back(*this, name, value, type);
        }

        static const auto find = [&](const std::string &name) {
            for (const auto &e : apiConstants) {
                if (e.name.original == name) {
                    return e;
                }
            }
            throw std::runtime_error("can't find api constant: " + name);
        };

        for (const auto &a : aliased) {
            const auto &target = find(a.first);
            const auto name = std::string(a.second["name"]);
            apiConstants.emplace_back(*this, name, target.value, target.type);
        }
    }

    void Registry::parseEnums(Generator &gen, xml::Element elem, xml::Element children) {
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
            auto &en = enums[name];

            for (const auto &value : xml::vulkanElements(children, "enum")) {
                parseEnumValue(value, en);
            }

        }
    }

    void Registry::parseCommands(Generator &gen, xml::Element elem, xml::Element children) {
        if (verbose)
            std::cout << "Parsing commands" << '\n';

        ItemInserter inserter{ commands, false };
        for (const auto &commandElement : xml::vulkanElements(children, "command")) {
            inserter.insert(gen, commandElement, "");
        }
        commands.prepare();
        inserter.addAliases([&](auto &a) {
            const auto &name    = a.first;
            const auto &alias   = a.second;
            auto       &command = commands[alias];
            commands.items.emplace_back(gen, command, name);
        });
        commands.prepare();

        for (auto &c : commands) {
            if (c.destroysObject()) {
                bool hasOverload = false;
                if (c.name.original == "vkDestroyInstance" || c.name.original == "vkDestroyDevice") {
                    hasOverload = true;
                } else {
                    auto        name = c.name.original;
                    const auto &tag  = strRemoveTag(name);
                    hasOverload      = !tag.empty() && commands.contains(name);
                }
                if (hasOverload) {
                    c.setFlagBit(Command::CommandFlags::OVERLOADED_DESTROY, true);
                }
            }
        }

        if (verbose)
            std::cout << "Parsing commands done" << '\n';
    }

    void Registry::orderCommands() {
        std::sort(commands.ordered.begin(), commands.ordered.end(), [](const Command &a, const Command &b){ return a.successCodes.size() < b.successCodes.size(); });

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
                hist[c.successCodes.size()] += 1;
            }
            else {
                arraycnt++;
            }

            if (c.successCodes.size() < 2 || retarray)
                continue;

            continue;
            if (c.pfnReturn == vkr::Command::PFNReturnCategory::VOID) {
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
    }

    void Registry::assignCommands(Generator &gen) {
        if (handles.items.empty()) {
            return;
        }

        auto &instance = findHandle("VkInstance");
        auto &device   = findHandle("VkDevice");

        std::vector<std::string_view> deviceObjects;
        std::vector<std::string_view> instanceObjects;

        std::vector<Command> elementsUnassigned;

        for (auto &h : handles.items) {
            if (h.name.original == "VkDevice" || h.superclass == "Device") {
                deviceObjects.push_back(h.name.original);
            } else if (h.name.original == "VkInstance" || h.superclass == "Instance") {
                instanceObjects.push_back(h.name.original);
            }
        }

        const auto addCommand = [&](const std::string &type, Handle &handle, Command &command) {
            bool indirect = type != handle.name.original && command.isIndirectCandidate(type);

            handle.addCommand(gen, command);

            // std::cout << "=> added direct to: " << handle.name << ", " << command.name << '\n';
            if (indirect) {
                command.setFlagBit(Command::CommandFlags::INDIRECT, true);
                auto &handle = findHandle(type);
                handle.addCommand(gen, command);
                // std::cout << "=> added indirect to: " << handle.name << ", " << command.name << '\n';
            }

            Handle *second = command.secondIndirectCandidate(gen);
            if (second) {
                if (second->superclass.original == type) {
                    second->addCommand(gen, command, true);
                    // std::cout << "=> added 2nd indirect to: " << second->name << ", " << command.name << '\n';
                }
            }
        };

        const auto assignGetProc = [&](Handle &handle, Command &command) {
            if (command.name.original == "vkGet" + handle.name + "ProcAddr") {
                handle.getAddrCmd.emplace(&gen, &handle, command);
                return true;
            }
            return false;
        };

        const auto assignConstruct = [&](Command &command) {
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
                auto       &handle     = findHandle(type);
                if (last->isArrayOut() && (command.nameCat == Command::NameCategory::CREATE || command.nameCat == Command::NameCategory::ALLOCATE ||
                                           command.nameCat == Command::NameCategory::ENUMERATE))
                {
                    handle.vectorVariant = true;
                    handle.vectorCmds.emplace_back(&gen, &handle, command);
                } else {
                    handle.ctorCmds.emplace_back(&gen, &handle, command);
                }
            }
            catch (std::runtime_error e) {
                std::cerr << "warning: can't assign constructor: " << type << " (from " << command.name << "): " << e.what() << '\n';
            }
        };

        const auto assignDestruct2 = [&](Command &command, Handle::CreationCategory cat) {
            try {
                auto last = command.getLastHandleVar();
                if (!last) {
                    throw std::runtime_error("can't get param (last handle)");
                }

                std::string type = last->original.type();

                auto &handle = findHandle(type);
                if (handle.dtorCmd) {
                    // std::cerr << "warning: handle already has destruct command:" << handle.name << '\n';
                    return;
                }

                handle.creationCat = cat;
                handle.setDestroyCommand(gen, command);
            }
            catch (std::runtime_error e) {
                std::cerr << "warning: can't assign destructor: " << " (from " << command.name << "): " << e.what() << '\n';
            }
        };

        const auto assignDestruct = [&](Command &command) {
            if (command.name.starts_with("destroy")) {
                assignDestruct2(command, Handle::CreationCategory::CREATE);
            } else if (command.name.starts_with("free")) {
                assignDestruct2(command, Handle::CreationCategory::ALLOCATE);
            }
        };

        for (Command &command : commands.ordered) {
            if (assignGetProc(instance, command) || assignGetProc(device, command)) {
                continue;
            }

            std::string first;
            bool        isHandle = false;
            if (command.hasParams()) {
                assignConstruct(command);
                assignDestruct(command);
                // type of first argument
                auto &p  = command.params.begin()->get();
                first    = p.original.type();
                isHandle = p.isHandle();
            }

            if (!isHandle) {
                staticCommands.push_back(command);
                gen.loader.addCommand(gen, command);
                command.top = &gen.loader;
                continue;
            }

            if (isInContainter(deviceObjects, first)) {  // command is for device
                addCommand(first, device, command);
                command.top = &device;
            } else if (isInContainter(instanceObjects, first)) {  // command is for instance
                addCommand(first, instance, command);
                command.top = &instance;
            } else {
                std::cerr << "warning: can't assign command: " << command.name << '\n';
            }
        }

        if (!elementsUnassigned.empty()) {
            std::cerr << "Unassigned commands: " << elementsUnassigned.size() << '\n';
            for (auto &c : elementsUnassigned) {
                std::cerr << "  " << c.name << '\n';
            }
        }
        std::cout << "instance: " << instance.members.size() * 8<< " commands\n";
        std::cout << "device: " << device.members.size() * 8<< " commands\n";
        if (verbose)
            std::cout << "Assign commands done" << '\n';
    }

    GenericType &Registry::get(const std::string &name) {
        assert(!types.empty() && "type map not build yet\n");

        auto it = types.find(name);
        if (it != types.end()) {
            return *it->second;
        }
        else {
            auto it = aliases.find(name);
            if (it != aliases.end()) {
                return it->second.get();
            }
        }
        throw std::runtime_error("Error: " + std::string{ name } + " not found in reg");
    }

    GenericType *Registry::find(const std::string &name) noexcept {
        assert(!types.empty() && "type map not build yet\n");

        if (auto type = types.find(name); type != types.end()) {
            return type->second;
        }
        if (auto type = commands.find(name); type != commands.end()) {
            return &*type;
        }
        if (auto type = baseTypes.find(name); type != baseTypes.end()) {
            return &type->second;
        }
        if (auto type = funcPointers.find(name); type != funcPointers.end()) {
            return &type->second;
        }
        if (auto type = aliases.find(name); type != aliases.end()) {
            return &type->second.get();
        }
        return nullptr;
    }

    void Registry::orderStructs() {
        DependencySorter<Struct> sorter;

        sorter.sort(structs, "structs", [&](DependencySorter<Struct>::Item &i) {
            for (auto &m : i.data->members) {
                if (!m->isPointer() && m->isStructOrUnion()) {
                    // std::cout << i.data->name << " dep: " << m->type() << "\n";
                    i.addDependency(sorter, m->original.type());
                }
            }
        });
    }

    void Registry::orderHandles() {
        DependencySorter<Handle> sorter;
        const auto               filter = [&](DependencySorter<Handle>::Item &i, ClassCommand &m) {
            std::string_view                        name = m.name.original;
            static const std::array<std::string, 4> names{
                "vkCreate",
                "vkAllocate",
                "vkDestroy",
                "vkFree",
            };

            for (const auto &n : names) {
                if (name.starts_with(n)) {
                    std::string_view str = name.substr(n.size());
                    if (str == i.data->name) {
                        return false;
                    }
                }
            }

            return true;
        };

        sorter.sort(handles, "handles", [&](DependencySorter<Handle>::Item &i) {
            // if (i.data->isSubclass && !i.data->members.empty()) {
            // i.addDependency(sorter, i.data->superclass.original);
            // }
            for (auto &m : i.data->members) {
                if (!filter(i, m)) {
                    continue;
                }
                for (auto &p : m.src->_params) {
                    if (p->isHandle() && !p->isPointer()) {
                        if (p->original.type() == "VkInstance" || p->original.type() == "VkDevice" || p->original.type() == i.data->name.original) {
                            continue;
                        }
                        i.addDependency(sorter, p->original.type());
                    }
                }
                auto p = m.src->getProtect();
                if (!p.empty()) {
                    i.plats.insert(std::string{ p });
                }
            }
        });
    }

    void Registry::parseFeature(Generator &gen, xml::Element elem, xml::Element children) {
        if (xml::isVulkan(elem)) {
            parse->xmlSupportedFeatures.emplace_back(elem);
        }
        else {
            parse->xmlUnsupportedFeatures.emplace_back(elem);
        }
    }

    void Registry::parseExtensions(Generator &gen, xml::Element elem, xml::Element children) {
        for (const auto &extension : xml::elements(children, "extension")) {
            if ( xml::isVulkanExtension(extension)) {
                parse->xmlSupportedExtensions.emplace_back(extension);
            }
            else {
                parse->xmlUnsupportedExtensions.emplace_back(extension);
            }
        }
    }

    static constexpr uint64_t calcEnumExtensionValue(int extnumber) {
        return 1000000000 + 1000 * (extnumber - 1);
    }

    void Registry::parseEnumValue(const xml::Element &elem, vkr::Enum &e, vkr::Feature *feature, vkr::Extension *ext, const std::string_view protect) {

        const auto name = elem["name"];
        EnumValue *type = e.find(name);
        if (!type) {
            type = &e.members.emplace_back(*this, std::string(name), "", e.name, e.isBitmask());
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
            type->value = neg? "-" : "";
            type->value += value.value();
        }
        else  {
            uint64_t eval = 0;
            if (!e.isBitmask()) {
                const auto extnumber = elem.optional("extnumber");
                if (extnumber) {
                    eval = calcEnumExtensionValue(toInt(std::string(extnumber.value())));
                }
                else if (ext) {
                    eval = calcEnumExtensionValue(ext->number);
                }
            }
            const auto bitpos = elem.optional("bitpos");
            const auto offset = elem.optional("offset");
            if (bitpos) {
                eval += 1ULL << toInt(std::string(bitpos.value()));
                type->setValue(eval, neg, e);
            }
            else if (offset) {
                eval += toInt(std::string(offset.value()));
                type->setValue(eval, neg, e);
            }
        }
        type->bind(feature, ext, protect);
    }

    void Registry::buildTypesMap() {
        types.clear();
        aliases.clear();

        handles.addTypes(types);
        enums.addTypes(types);
        structs.addTypes(types);
        commands.addTypes(types);
        for (auto &a : apiConstants) {
            types.emplace(a.name.original, &a);
        }
        for (auto &h : handles) {
            for (auto &a : h.aliases) {
                aliases.emplace(a.name.original, std::ref(a));
                aliases.emplace(a.name, std::ref(a));
            }
        }
        for (auto &e : enums) {
            for (auto &a : e.aliases) {
                aliases.emplace(Enum::toFlags(a.name.original), std::ref(a));
                aliases.emplace(Enum::toFlags(a.name), std::ref(a));
                aliases.emplace(Enum::toFlagBits(a.name.original), std::ref(a));
                aliases.emplace(Enum::toFlagBits(a.name), std::ref(a));
            }
        }
        for (auto &s : structs) {
            for (auto &a : s.aliases) {
                aliases.emplace(a.name.original, std::ref(a));
                aliases.emplace(a.name, std::ref(a));
            }
        }

    }

    void Registry::removeUnsupportedFeatures() {

        const auto disableType = [](GenericType *type) {
            if (!type) {
                return;
            }
            // if (type->version || !type->tempversion.empty() || type->getExtension()) {
            if (type->getFeature()) {
                // std::cout << "skip disable: " << type->name << "\n";
                return;
            }
            type->setUnsupported();
        };

        const auto disableTypes = [&](xml::Element e) {
            for (const auto &entry : xml::View(e.firstChild())) {
                const auto name = entry.optional("name");
                if (!name) {
                    continue;
                }

//                    if (name.value() == "VkPipelineCacheCreateFlagBits") {
//                        auto *type = find(name.value());
//                        std::cout << "sc un: " << type->version << ", " << type->tempversion << "\n";
//                        continue;
//                    }
//                    const std::string_view value = entry->Value();
//                    if (value == "type" || value == "command") {
//                        auto *type = find(name.value());
//                        if (type) {
//                            type->setUnsuppored();
//                        }
//                    }

                auto *type = find(name.value());
                disableType(type);
                //        else {
                //          std::cout << "not found: " << name << "\n";
                //        }

                if (name.value().find("FlagBits") != std::string::npos) {
                    std::string tmp = std::string{ name.value() };
                    tmp             = std::regex_replace(tmp, std::regex("FlagBits"), "Flags");
                    auto *type      = find(tmp);
                    disableType(type);
                }
                if (name.value().find("Flags") != std::string::npos) {
                    std::string tmp = std::string{ name.value() };
                    tmp             = std::regex_replace(tmp, std::regex("Flags"), "FlagBits");
                    auto *type      = find(tmp);
                    disableType(type);
                }
            }
        };

        const auto assignVersions = [&](xml::Element elem, Feature *feature, Extension *ext) {
            for (const auto &entry : xml::View(elem.firstChild())) {

                const std::string_view value = entry->Value();
                // std::cout << value << ", " << number << "\n";
                if (value == "enum") {
                    const auto extends = entry.optional("extends");
                    if (extends) {
                        auto &en = enums[extends.value()];
                        parseEnumValue(entry, en, feature, ext);
                    }
                    else {
                        const auto name = entry.optional("name");
                        const auto value = entry.optional("value");
                        if (name && value) {
                            std::string code = "#define ";
                            code += name.value();
                            code += " ";
                            code += value.value();
                            code += "\n";
                            ext->constants.emplace_back(std::move(code));
                        }
                    }
                } else if (value == "command") {
                    auto &command = commands[entry["name"]];
                    command.bind(feature, ext);
                } else if (value == "type") {
                    auto *type = find(entry["name"]);
                    if (type) {
                        type->bind(feature, ext);
                    }
//                    else {
//                        std::cout << "can't find type: " << entry["name"] << "\n";
//                    }
                }
            }
        };

        const auto assignTypes = [&](xml::Element elem, Feature *feature) {
            for (const auto &entry : xml::View(elem.firstChild()) | xml::filter_vulkan()) {
                const auto& value = entry.value();
                auto protect = entry.optional("protect");

                if (value == "command") {
                    const auto name    = entry["name"];
                    feature->tryInsertFrom(commands, std::string(name), feature->commands);
                } else if (value == "type") {
                    std::string name = std::string(entry["name"]);
                    if (name == "vk_platform") {
                        continue;
                    }
                    if (!feature->tryInsert(*this, name)) {
                        std::cout << "(ext) can't find: " << name << "\n";
                    }
                } else if (value == "enum") {
                    feature->elements++;
                }

            }
        };

        std::vector<xml::Element> unsupported;
        features.items.reserve(parse->xmlSupportedFeatures.size());
        for (const auto &elem : parse->xmlSupportedFeatures) {

            const auto name   = elem["name"];
            const auto number = elem["number"];

            auto &feature = features.items.emplace_back(name);
            feature.version = number.data();

            for (const auto &require : xml::elements(elem.firstChild(), "require")) {
                assignVersions(require, &feature, nullptr);
            }
        }
        features.prepare();

        for (const auto &elem : parse->xmlSupportedExtensions) {
            const auto name   = elem["name"];
            // std::cout << "Extension: " << name << '\n';

            Platform  *platform       = nullptr;
            const auto platformAttrib = elem.optional("platform");
            if (platformAttrib) {
                platform = &platforms[platformAttrib.value()];
            }

            bool enabled = defaultWhitelistOption;
            // std::cout << "add ext: " << name << '\n';
            auto &ext   = extensions.items.emplace_back(std::string{ name }, platform, true, enabled);
            // ext.version = no_ver;
            const auto number = elem.optional("number");
            if (number) {
                ext.number = toInt(std::string(number.value()));
            }
            const auto comment = elem.optional("comment");
            if (comment) {
                ext.comment = comment.value();
            }
            //            std::cout << "ext: " << ext << '\n';
            //            std::cout << "> n: " << ext->name.original << '\n';
        }
        extensions.prepare();

        for (const auto &elem : parse->xmlSupportedExtensions) {

            const auto name = elem["name"];
            Extension *ext = &extensions[name];
            Feature *feature = nullptr;

            const auto promote = elem.optional("promotedto");
            if (promote) {
                auto it = features.find(promote.value());
                if (it != features.end()) {
                    feature = &*it;
                }
            }

            const auto depends = elem.optional("depends");
            if (depends) {
                for (const auto &dep : split2(depends.value(), "+")) {
                    if (auto it = extensions.find(dep); it != extensions.end()) {
                        ext->depends.emplace_back(&*it);
                    }
                    else {
                        ext->versiondepends += dep;
                    }
                }
            }

            for (const auto &require : xml::elements(elem.firstChild(), "require")) {
                if (!xml::isVulkan(require)) {
                    unsupported.emplace_back(require);
                }
                else {
                    assignVersions(require, feature, ext);
                }
            }
        }

        for (const auto &elem : parse->xmlUnsupportedFeatures) {
            for (const auto &require : xml::elements(elem.firstChild(), "require")) {
                disableTypes(require);
            }
        }
        for (const auto &elem : parse->xmlUnsupportedExtensions) {
            for (const auto &require : xml::elements(elem.firstChild(), "require")) {
                disableTypes(require);
            }
        }
        for (const auto &elem : unsupported) {
            disableTypes(elem);
        }

        commands.removeUnsupported();
        enums.removeUnsupported();
        structs.removeUnsupported();
        buildTypesMap();

        for (const auto &elem : parse->xmlSupportedExtensions) {
            const auto name = elem["name"];
            Extension *extension = &extensions[name];

            for (const auto &require : xml::elements(elem.firstChild(), "require")) {
                if (xml::isVulkan(require)) {
                    assignTypes(require, extension);
                }
            }
        }
        for (const auto &elem : parse->xmlSupportedFeatures) {
            const auto name = elem["name"];
            Feature *feature = &features[name];

            for (const auto &require : xml::elements(elem.firstChild(), "require")) {
                assignTypes(require, feature);
            }
        }

        for (const auto &elem : parse->xmlSupportedExtensions) {
            const auto name      = elem["name"];
            Extension *extension = &extensions[name];

            const auto promote = elem.optional("promotedto");
            if (promote) {
                auto it = features.find(promote.value());
                if (it != features.end()) {
                    auto &feature = *it;
                    for (Command &c : extension->commands) {
                        feature.promotedTypes.emplace_back(std::ref(c));
                    }
                    for (Struct &s : extension->structs) {
                        feature.promotedTypes.emplace_back(std::ref(s));
                    }
                    for (Enum &e : extension->enums) {
                        feature.promotedTypes.emplace_back(std::ref(e));
                    }
                }
            }
        }
    }

    void Registry::buildDependencies(Generator &gen) {
        if (verbose)
            std::cout << "Building dependencies information" << '\n';

        for (auto &e : parse->structExtends) {
            auto src = structs.find(e.first.data());
            if (src != structs.end()) {
                auto dst = structs.find(e.second.data());
                if (dst != structs.end()) {
                    dst->extends.emplace_back(&*src);
                }
            }
        };

        for (auto &h : handles.items) {
            if (!h.superclass.empty()) {
                h.setParent(*this, &handles[h.superclass]);
            }
        }
        for (auto &h : handles.items) {
            h.init(gen);
        }

        for (auto &e : enums) {
            for (auto &a : e.aliases) {
                a.parentExtension = e.getExtension();
            }
        }

        for (auto &s : structs) {
            for (auto &a : s.aliases) {
                a.parentExtension = s.getExtension();
            }
            for (const auto &m : s.members) {
                m->updateMetaType(*this);
                const std::string &type = m->original.type();
                auto              *d    = find(type);
                if (d) {
                    s.dependencies.insert(d);
                }
            }
        }

        for (auto &command : commands) {
            for (auto &a : command.aliases) {
                a.parentExtension = command.getExtension();
            }
            for (const auto &m : command._params) {
                m->updateMetaType(*this);
                const std::string &type = m->original.type();
                auto              *d    = find(type);
                if (d) {
                    command.dependencies.insert(d);
                }
            }
            command.init(*this);
        }

        for (auto &h : handles.items) {
            for (auto &c : h.ctorCmds) {
                h.dependencies.insert(c.src);
            }
            if (h.dtorCmd) {
                h.dependencies.insert(h.dtorCmd);
            }
        }

        if (verbose)
            std::cout << "Building dependencies done" << '\n';
    }

    bool Registry::load(Generator &gen, const std::string &xmlPath) {
        parse = std::make_unique<Parse>();

        if (isLoaded()) {
            unload();
        }

        if (!loadXML(xmlPath)) {
            unload();
            return false;
        }

        parseXML(gen);
        buildTypesMap();
        removeUnsupportedFeatures();
        buildDependencies(gen);

        for (const auto &r : parse->typeRequires) {

            const auto &req = r.first;
            auto it = includes.find(req);
            if (it == includes.end()) {
                std::cerr << "Parse error: missing include node: " << req << "\n";
                continue;
            }
            auto &inc = it->second;
            const auto &name = r.second;

            for (const auto &s : structs) {
                for (const auto &m : s.members) {
                    if (m->original.type() == name) {
                        auto *platform = s.getPlatfrom();
                        if (platform) {
                            platform->includes.insert(inc);
                        }
                        else {
                            auto *ext = s.getExtension();
                            if (!ext) {
                                std::cerr << "found: " << name << " -> " << req << "\n";
                            }
                            else {
                                // std::cout << "found: " << name << " -> " << req << " -> " << ext->name << "\n";
                                bool dup = false;
                                for (const auto &i : ext->includes) {
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
            for (const auto &c : commands) {
                for (const auto &p : c._params) {
                    auto *platform = c.getPlatfrom();
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

        orderCommands();
        assignCommands(gen);
        orderStructs();
        orderHandles();

        for (auto &c : commands) {
            if (c.destroysObject()) {
                auto *handle = c.getLastHandleVar();
                if (handle) {
                    handle->overrideOptional(false);
                }
            }
        }

        for (auto &f : features) {
            DependencySorter<Struct> sorter;
            sorter.sort(f.structs, "structs");

        }

        for (auto &e : extensions) {
            DependencySorter<Struct> sorter;
            sorter.sort(e.structs, "structs");
        }


        DependencySorter<Extension> sorter;
        sorter.sort(extensions, "extensions", [&](DependencySorter<Extension >::Item &i) {
            // std::cout << "e: " << i.data->name << "\n";
            const auto findDependencies = [&](const GenericType &item) {
                for (const auto &dep : item.dependencies) {
                    auto *type = find(dep->name.original);
                    if (type) {
                        auto *ext = type->getExtension();
                        if (ext && ext != i.data) {
                            // std::cout << "  -> " << ext->name << "\n";
                            i.addDependency(sorter, ext->name.original);
                        }
                        auto *parExt = type->parentExtension;
                        if (parExt && parExt != i.data) {
                            // std::cout << "  -> " << ext->name << "\n";
                            i.addDependency(sorter, parExt->name.original);
                        }
                    }
                }
            };

            for (const auto &t : i.data->structs) {
                findDependencies(t);
            }
            for (const auto &t : i.data->commands) {
                findDependencies(t);
            }
        });

        platforms.ordered.clear();
        platforms.ordered.reserve(platforms.items.size());
        for (Extension &e : extensions.ordered) {
            auto *platform = e.platform;
            // std::cout << platform << "\n";
            if (platform) {
                // std::cout << "ext: " << e.name << " -> " << platform->name << "\n";
                platform->extensions.emplace_back(std::ref(e));
                bool dup = false;
                for (const auto &p : platforms.ordered) {
                    if (&p.get() == platform) {
                        dup = true;
                        // std::cout << "  dup!\n";
                        break;
                    }
                }
                if (!dup) {
                    platforms.ordered.emplace_back(std::ref(*platform));
                }
            }
            for (FuncPointer &f : e.funcPointers) {
                for (const Struct &s : e.structs) {
                    for (const auto &m : s.members) {
                        if (m->original.type() == f.name.original) {
                            // std::cout << "in struct: " << f.name.original << "\n";
                            f.inStruct = true;

                                std::regex r("Vk[a-zA-Z0-9_]+");
                                for(std::sregex_iterator i = std::sregex_iterator(f.code.begin(), f.code.end(), r);
                                     i != std::sregex_iterator();
                                     ++i )
                                {
                                    const std::smatch &m = *i;

                                    auto it = structs.find(m.str());
                                    if (it != structs.end()) {
                                        it->needForwardDeclare = true;
                                        auto *feature = s.getFeature();
                                        // std::cout << "  -> " << m.str() << "  " << feature << '\n';
                                        if (feature) {
                                            Feature::insert(feature->forwardStructs, *it);
                                        }
                                    }
                                }

                            break;
                        }
                    }
                }
            }
        }

        for (auto &feature : features) {
            for (FuncPointer &f : feature.funcPointers) {
                for (const Struct &s : feature.structs) {
                    for (const auto &m : s.members) {
                        if (m->original.type() == f.name.original) {
                            f.inStruct = true;
                            break;
                        }
                    }
                }
            }
        }

        for (auto &e : enums) {
            for (auto &m : e.members) {
                if (!m.alias.empty()) {
                    auto *src = e.find(m.alias);
                    if (src) {
                        m.value = src->value;
                        // m.value += " // " + m.alias;
                    }
                }
                if (m.value.empty()) {
                    std::cout << "warn: " << m.name.original << " has no value\n";
                }
            }
            if (e.type.empty()) {
                std::cout << "warn: " << e.name.original << " has no type\n";
            }
        }

        const auto lockDependency = [&](const std::string &name) {
            auto *type = find(name);
            if (type) {
                type->forceRequired = true;
            }
        };

        lockDependency("VkStructureType");
        lockDependency("VkResult");
        lockDependency("VkObjectType");
        lockDependency("VkDebugReportObjectTypeEXT");
        lockDependency("vkEnumerateInstanceVersion");

        for (auto &c : enums) {
            c.setEnabled(true);
        }
        for (auto &c : structs) {
            c.setEnabled(true);
        }
        for (auto &c : handles) {
            c.setEnabled(true);
        }
        for (auto &c : commands) {
            c.setEnabled(true);
        }

        for (auto &d : enums.items) {
            if (d.isSupported()) {
                d.setEnabled(true);
            }
        }

#ifdef INST
        std::vector<std::string> cmds;
        cmds.reserve(commands.size());
        for (const auto &c : commands.items) {
            cmds.emplace_back(c.name.original);
        }
        Inst::processCommands(cmds);
#endif

        for (auto &s : structs) {
            for (const auto &m : s.members) {
                auto p = s.getProtect();
                if (!m->isPointer() && m->isStructOrUnion()) {
                    auto pm = structs[m->original.type()].getProtect();
                    // std::cout << pm << '\n';
                    if (!p.empty() && !pm.empty() && p != pm) {
                        std::cout << ">> platform dependency: " << p << " -> " << pm << '\n';
                    }
                }
            }
        }


//        for (const Command &c : commands.ordered) {
//            std::cout << c.name.original << ": ";
//            auto *f = c.getFeature();
//            if (f) {
//                std::cout << "F: " << f->name.original;
//            }
//            auto *e = c.getExtension();
//            if (e) {
//                std::cout << "   E: " << e->name.original;
//            }
//            std::cout << "\n";
//        }

//        for (const Extension &e : extensions) {
//            std::cout << e.name.original << "\n";
//            for (auto &d : e.depends) {
//                std::cout << "  " << d->name.original << "\n";
//            }
//        }


        parse = nullptr;
        registryPath = xmlPath;
        loadFinished();
        return true;
    }

    void Registry::loadFinished() {
        if (onLoadCallback) {
            onLoadCallback();
        }
    }

    void Registry::bindGUI(const std::function<void()> &onLoad) {
        onLoadCallback = onLoad;

        if (isLoaded()) {
            loadFinished();
        }
    }

    void Registry::unload() {
        root         = nullptr;
        registryPath = "";

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

    }

    std::string Registry::to_string(vkr::Command::PFNReturnCategory value) {
        using enum vkr::Command::PFNReturnCategory;
        switch (value) {
            case OTHER: return "OTHER";
            case VOID: return "VOID";
            case VK_RESULT: return "VK_RESULT";
            default: return "";
        }
    };

    std::string Registry::to_string(vkr::Command::NameCategory value) {
        using enum vkr::Command::NameCategory;
        switch (value) {
            case UNKNOWN: return "UNKNOWN";
            case GET: return "GET";
            case ALLOCATE: return "ALLOCATE";
            case ACQUIRE: return "ACQUIRE";
            case CREATE: return "CREATE";
            case ENUMERATE: return "ENUMERATE";
            case WRITE: return "WRITE";
            case DESTROY: return "DESTROY";
            case FREE: return "FREE";
            default: return "";
        }
    }

    bool Registry::loadXML(const std::string &xmlPath) {
        std::cout << "load: " << xmlPath << "\n";
        const auto err = doc.LoadFile(xmlPath.c_str());
        if (err != tinyxml2::XML_SUCCESS) {
            std::cerr << "XML load failed: " << std::to_string(err) << " (file: " << xmlPath + ")\n";
            return false;
        }

        root = doc.RootElement();
        if (!root) {
            std::cerr << "XML file is empty\n";
            return false;
        }
        return true;
    }

    void Registry::parseXML(Generator &gen) {
        using Func    = void (vkgen::Registry::*)(Generator &, xml::Element, xml::Element);
        using Binding = std::pair<const std::string_view, Func>;
        // specifies order of parsing vk.xml registry
        const std::array<Binding, 7> loadOrder{ Binding{ "platforms", &Registry::parsePlatforms },  Binding{ "tags", &Registry::parseTags },
                                                Binding{ "types", &Registry::parseTypes },          Binding{ "enums", &Registry::parseEnums },
                                                Binding{ "commands", &Registry::parseCommands },    Binding{ "feature", &Registry::parseFeature },
                                                Binding{ "extensions", &Registry::parseExtensions } };

        // call each function in rootParseOrder with corresponding XMLNode
        auto *elements = root->FirstChildElement();
        for (const auto &key : loadOrder) {
            for (const auto &elem : xml::View(elements)) {
                if (key.first == elem->Value()) {
                    const auto &func = key.second;
                    (this->*func)(gen, elem, elem.firstChild());
                }
            }
        }
    }


    VulkanRegistry::VulkanRegistry(Generator &gen) : loader(gen) {
        loader.name.convert("VkContext", true);
        loader.forceRequired = true;
    }

    String& VulkanRegistry::getHandleSuperclass(const Handle &data) {
        if (!data.parent) {
            return loader.name;
        }
        return Registry::getHandleSuperclass(data);
    }

    void VulkanRegistry::createErrorClasses() {
        auto &e = enums["VkResult"];
        std::unordered_set<std::string> values;
        for (const auto &m : e.members) {
            if (!m.isAlias && m.name.starts_with("eError")) {
                if (values.find(m.value) != values.end()) {
                    continue;
                }
                values.insert(m.value);
                errorClasses.emplace_back(m);
            }
        }
    }

    bool VulkanRegistry::load(Generator &gen, const std::string &xmlPath) {
        unload();

        {
            std::filesystem::path path = xmlPath;
            path                       = path.replace_filename("video.xml");
            if (std::filesystem::exists(path)) {
                video = std::make_unique<VideoRegistry>();
                video->load(gen, path.string());
            }
        }

        auto result = Registry::load(gen, xmlPath);
        if (!result) {
            return false;
        }

        auto it = defines.find("VK_HEADER_VERSION");
        if (it != defines.end()) {
            const auto &text = it->second.code;;
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

        for (auto &h : handles.items) {
            if (!h.isSubclass) {
                topLevelHandles.emplace_back(std::ref(h));
            }
        }

        const auto setForward = [&](const std::string_view name) {
            auto s = structs.find(name);
            if (s != structs.end()) {
                auto *extension = s->getExtension();
                if (extension) {
                    Feature::insert(extension->forwardStructs, *s);
                }
            }
        };

        setForward("VkDebugUtilsMessengerCallbackDataEXT");
        setForward("VkDeviceMemoryReportCallbackDataEXT");

        return result;
    }

    void VulkanRegistry::unload() {
        topLevelHandles.clear();
        headerVersion.clear();
        errorClasses.clear();
        loader.clear();
        video = nullptr;

        Registry::unload();
    }

}  // namespace vkgen

namespace vkgen::vkr
{

    Snippet::Snippet(const std::string &name, std::string&& code) : GenericType(MetaType::BaseType, name), code(std::move(code))
    {}

    FuncPointer::FuncPointer(const std::string &name, std::string&& code) : Snippet(name, std::move(code))
    {}

    ClassCommand::operator GenericType() const {
        return *src;
    }

    ClassCommand::ClassCommand(const Generator *gen, const Handle *cls, Command &o)
      : cls(cls), src(&o)//, name(gen->convertCommandName(o.name.original, cls->name), false)
    {

        name.assign(o.name);
        std::string cname = cls->name;
        std::string tag = gen->strRemoveTag(cname);
        if (!cls->name.empty()) {
            name.assign(std::regex_replace(name, std::regex(cname, std::regex_constants::icase), ""));
        }
        if (!tag.empty()) {
            strStripSuffix(name, tag);
        }

        name.original = o.name.original;

        if (cls->name.original == "VkCommandBuffer" && name.starts_with("cmd")) {
            // auto old = name;
            name.assign(strFirstLower(name.substr(3)));
            // std::cout << "convert name: " << old << " -> " << name << std::endl;
        }
    }

    Handle::Handle(Generator &gen, xml::Element elem, const std::string_view, std::string &&code)
      : GenericType(MetaType::Handle, elem.getNested("name"), true)
      , superclass(std::string{elem.optional("parent").value_or("")})
      , objType(std::string{elem.optional("objtypeenum").value_or("VK_OBJECT_TYPE_UNKNOWN")})
      , vkhandle(VariableDataInfo{ .vktype     = this->name.original,
                                   // .identifier = "m_" + strFirstLower(this->name),
                                   .identifier = "m_handle",
                                   .assigment  = " = {}",
                                   .ns         = Namespace::VK,
                                   .flag       = VariableData::Flags::CLASS_VAR_VK | VariableData::Flags::CLASS_VAR_RAII,
                                   .metaType   = MetaType::Handle })
      , code(std::move(code))

    {
        isSubclass = name != "Instance" && name != "Device";
        objType = gen.enumConvertCamel("ObjectType", objType.original, false);
        // std::cout << objType.original << " -> " << objType << "\n";
    }

    void Handle::init(Generator &gen) {
        // std::cout << "Handle init: " << name << '\n';
        superclass = gen.getHandleSuperclass(*this);
        if (isSubclass) {
            ownerhandle = "m_" + strFirstLower(superclass);
        }

        if (isSubclass) {
            ownerUnique = std::make_unique<VariableData>(VariableDataInfo{ .vktype     = superclass.original,
                                                                           .identifier = "m_owner",
                                                                           .assigment  = " = {}",
                                                                           .ns         = Namespace::VK,
                                                                           .flag       = VariableData::Flags::CLASS_VAR_UNIQUE,
                                                                           .metaType   = MetaType::Handle });

            ownerRaii = std::make_unique<VariableData>(VariableDataInfo{ .vktype     = superclass.original,
                                                                         .suffix     = " const *",
                                                                         .identifier = "m_" + strFirstLower(superclass),
                                                                         .assigment  = " = nullptr",
                                                                         .ns         = Namespace::RAII,
                                                                         .flag       = VariableData::Flags::CLASS_VAR_RAII,
                                                                         .metaType   = MetaType::Handle });
        }
        // std::cout << "Handle init done " << '\n';
    }

    void Handle::setDestroyCommand(const Generator &gen, vkr::Command &cmd) {
        dtorCmd = &cmd;
        for (const auto &p : cmd._params) {
            if (!p->isHandle()) {
                continue;
            }
            if (p->original.type() == name.original) {
                continue;
            }
            if (p->original.type() == superclass.original) {
                continue;
            }
            //        std::cout << name << '\n';
            //        std::cout << "  <>" << p->fullType() << '\n';
            secondOwner = std::make_unique<VariableData>(VariableDataInfo{ .vktype     = p->original.type(),
                                                                           .identifier = "m_" + strFirstLower(p->type()),
                                                                           .assigment  = " = {}",
                                                                           .ns         = Namespace::VK,
                                                                           .flag = VariableData::Flags::CLASS_VAR_RAII | VariableData::Flags::CLASS_VAR_UNIQUE,
                                                                           .metaType = MetaType::Handle });
        }
    }

    void Handle::setParent(const Registry &reg, Handle *h) {
        parent = h;
        std::string name = h->name;
        reg.strRemoveTag(name);
        if (name.ends_with("Pool")) {
            poolFlag = true;
        }
    }

    void Handle::prepare(const Generator &gen) {
        clear();

        if (!parent) {
            superclass = gen.loader.name;
        }

        const auto &cfg  = gen.getConfig();
        effectiveMembers = 0;
        filteredMembers.clear();
        filteredMembers.reserve(members.size());

        std::unordered_map<std::string, ClassCommand *> stage;
        bool                                            order = !gen.orderedCommands.empty();

        for (auto &m : members) {
            if (m.src->canGenerate()) {
            // std::string const s = gen.genOptional(m, [&](std::string &output) {
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
            for (const auto &o : gen.orderedCommands) {
                auto it = stage.find(o.get().name.original);
                if (it != stage.end()) {
                    filteredMembers.push_back(it->second);
                    stage.erase(it);
                }
            }
            for (auto &c : stage) {
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

    void Handle::addCommand(const Generator &gen, vkr::Command &cmd, bool raiiOnly) {
        auto &c    = members.emplace_back(&gen, this, cmd);
        c.raiiOnly = raiiOnly;
    }

    std::string Enum::toFlags(const std::string &name) {
        return std::regex_replace(name, std::regex("FlagBits"), "Flags");
    }

    std::string Enum::toFlagBits(const std::string &name) {
        return std::regex_replace(name, std::regex("Flags"), "FlagBits");
    }

    EnumValue::EnumValue(const Registry &reg, std::string name, const std::string &value, const std::string &enumName, bool isBitmask)
      : GenericType(MetaType::EnumValue, name), value(value)
    {
        this->name.assign(reg.enumConvertCamel(enumName, name, isBitmask));
        this->enabled = true;
        // std::cout << "Value: " << this->name << ", " << this->name.original << "\n";
    }

    std::string EnumValue::toHex(uint64_t value, bool is64bit) {
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

    void EnumValue::setValue(uint64_t v, bool negative, const vkr::Enum &parent) {
        value = negative? "-" : "";
        if (parent.isBitmask()) {
            value += toHex(v, parent.is64bit());
        }
        else {
            value += std::to_string(v);
        }
        numericValue = negative? -v : v;
    }

    Enum::Enum(Generator &gen, xml::Element elem, const std::string_view name, const std::string_view type, bool isBitmask)
      : GenericType(MetaType::Enum, Enum::toFlags(std::string(name)), true),
        type(type),
        bitmask(isBitmask? Enum::toFlagBits(std::string(name)) : "", true)
    {}

    bool Enum::containsValue(const std::string &value) const {
        return std::ranges::any_of(members, [&](const auto &m) { return m.name == value; });
    }

    EnumValue *Enum::find(const std::string_view value) noexcept {
        for (auto &m : members) {
            if (m.name.original == value || m.name == value) {
                return &m;
            }
        }
        return nullptr;
    }

    void Command::init(const Registry &reg) {
        const bool noArray = name.original == "vkGetDescriptorEXT";
        _params.bind(noArray);

        initParams();

        bool hasHandle    = false;
        bool hasTopHandle    = false;
        bool canTransform = false;
        for (auto &p : _params) {
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
                    const auto &s = reg.structs.find(p->original.type());
                    if (!s->extends.empty()) {
                        structChain = &*s;
                    }
                }
                canTransform = true;
            }

            auto *sizeVar = p->getLengthVar();
            if (sizeVar) {
                canTransform = true;
            }
            if (p->isPointer() && p->isStructOrUnion()) {
                canTransform = true;
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

        prepared = true;
    }

    Command::Command(Generator &gen, xml::Element elem, const std::string_view)
      : GenericType(MetaType::Command) {
        // iterate contents of <command>
        std::string dbg;
        std::string name;
        // add more space to prevent reallocating later
        _params.reserve(16);
        for (const auto &child : xml::View(elem.firstChild())) {
            // <proto> section
            dbg += std::string(child->Value()) + "\n";
            if (child->Value() == std::string_view("proto")) {
                // get <name> field in proto
                auto *nameElement = child->FirstChildElement("name");
                if (nameElement) {
                    name = nameElement->GetText();
                }
                // get <type> field in proto
                auto *typeElement = child->FirstChildElement("type");
                if (typeElement) {
                    type = typeElement->GetText();
                }
            }
            // <param> section
            else if (child->Value() == std::string_view("param"))
            {
                if (xml::isVulkan(child)) {
                    _params.emplace_back(std::make_unique<VariableData>(gen, child));
                }
            }
        }
        if (name.empty()) {
            std::cerr << "Command has no name" << '\n';
        }

        setName(gen, name);

        const auto successcodes = elem.optional("successcodes");
        if (successcodes) {
            for (const auto &str : split(std::string(successcodes.value()), ",")) {
                successCodes.push_back(str);
            }
        }

        // init(gen);
    }

    Command::Command(const Registry &reg, const vkr::Command &o, const std::string_view alias) : GenericType(MetaType::Command) {
        type         = o.type;
        successCodes = o.successCodes;
        nameCat      = o.nameCat;
        pfnReturn    = o.pfnReturn;
        flags        = o.flags;
        setFlagBit(CommandFlags::ALIAS, true);
        setName(reg, std::string(alias));

        _params.reserve(o._params.size());
        for (const auto &p : o._params) {
            _params.push_back(std::make_unique<VariableData>(*p));
        }

        // init(reg);
    }

    Command::PFNReturnCategory getPFNReturnCategory(const std::string &type) {
        using enum Command::PFNReturnCategory;
        if (type == "void") {
            return VOID;
        }
        if (type == "VkResult") {
            return VK_RESULT;
        }
        return OTHER;
    }

    Command::NameCategory getMemberNameCategory(const std::string &name) {
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

    Handle *Command::secondIndirectCandidate(Generator &gen) const {
        if (_params.size() < 2) {
            return nullptr;
        }
        if (!_params[1]->isHandle()) {
            return nullptr;
        }
        if (destroysObject()) {
            return nullptr;
        }

        const auto &type        = _params[1]->original.type();
        bool        isCandidate = true;
        try {
            const auto &var = getLastPointerVar();
            if (getsObject() || createsHandle()) {
                if (nameCat != NameCategory::GET) {
                    isCandidate = !var->isArray();
                } else {
                    isCandidate = var->original.type() != type;
                }
            } else if (destroysObject()) {
                isCandidate = var->original.type() != type;
            }
        }
        catch (std::runtime_error) {
        }
        if (!isCandidate) {
            return nullptr;
        }
        auto &handle = gen.findHandle(type);
        return &handle;
    }

    void Command::setName(const Registry &reg, const std::string &name) {
        this->name.convert(name);
        pfnReturn = getPFNReturnCategory(type);
        // std::string tag = reg.strWithoutTag(name);
        nameCat = getMemberNameCategory(name);
    }

    bool Feature::tryInsert(Registry &reg, const std::string &name) {
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
        if (tryInsertFromMap(reg.aliases, name, aliases)) {
            return true;
        }
        if (tryInsertFromMap(reg.includes, name, includes)) {
            return true;
        }
        return false;
    }

    Struct::Struct(Generator &gen, const std::string_view name, MetaType::Value type, const xml::Element &e) : GenericType(type, name, true) {
        auto returnedonly = e.optional("returnedonly");
        if (returnedonly.value_or("") == "true") {
            this->returnedonly = true;
        }

        // iterate contents of <type>, filter only <member> children
        for (const auto &member : xml::vulkanElements(e.firstChild(), "member")) {
            auto &v = members.emplace_back(std::move(std::make_unique<VariableData>(gen, member)));

            const std::string &type = v->type();
            const std::string &name = v->identifier();

            if (!v->isPointer() && (type == "float" || type == "double")) {
                containsFloatingPoints = true;
            }

            if (const char *values = member->ToElement()->Attribute("values")) {
                std::string value = gen.enumConvertCamel(type, values);
                v->setAssignment(" = " + type + "::" + value);
                if (v->original.type() == "VkStructureType") {  // save sType information for structType
                    structTypeValue.original = values;
                    structTypeValue = value;
                }
            }
        }
        members.bind();
    }

}  // namespace vkgen::vkr
