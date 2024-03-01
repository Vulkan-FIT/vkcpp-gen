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

#include <filesystem>
#include <ranges>
#include <utility>

static constexpr auto no_ver = "";

namespace fs = std::filesystem;

namespace vkgen
{

    std::string_view BaseType::getProtect() const {
        return protect;
    }

    void BaseType::setProtect(const std::string_view protect) {
        this->protect = protect;
    }

    void BaseType::setExtension(vkr::Extension *ext) {
        this->ext = ext;
        if (!ext->protect.empty()) {
            protect = ext->protect;
        }
    }

    void BaseType::bind(vkr::Extension *ext, const std::string_view protect) {
        if (ext) {
            setExtension(ext);
        } else {
            ext = nullptr;
        }
        if (!protect.empty()) {
            this->protect = protect;
        }
    }

    std::string Registry::findSystemRegistryPath() {
        size_t size = 0;

        getenv_s(&size, nullptr, 0, "VULKAN_SDK");
        if (size == 0) {
            return "";
        }
        std::string sdk;
        sdk.resize(size);
        getenv_s(&size, sdk.data(), sdk.size(), "VULKAN_SDK");
        sdk.resize(size - 1);
        // std::cout << "sdk path: " << sdk << "\n";
        auto regPath = fs::path{ sdk } / fs::path{ "share/vulkan/registry/vk.xml" };

        if (fs::exists(regPath)) {
            return fs::absolute(regPath).string();
        }
        return "";
    }

    std::string Registry::findLocalRegistryPath() {
        const fs::path regPath{ "vk.xml" };
        if (fs::exists(regPath)) {
            return fs::absolute(regPath).string();
        }
        return "";
    }

    std::string Registry::findDefaultRegistryPath() {
        std::string path = findSystemRegistryPath();
        if (!path.empty()) {
            return path;
        }
        return findLocalRegistryPath();
    }

    Registry::Registry(Generator &gen) : loader(gen) {
        loader.name.convert("VkContext", true);
        loader.forceRequired = true;
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

    std::pair<std::string, std::string> Registry::snakeToCamelPair(std::string str) const {
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

        return std::make_pair(out, suffix);
    }

    std::string Registry::snakeToCamel(const std::string &str) const {
        const auto &p = snakeToCamelPair(str);
        return p.first + p.second;
    }

    std::string Registry::enumConvertCamel(const std::string &enumName, std::string value, bool isBitmask) const {
        std::string enumSnake = enumName;
        std::string tag       = strRemoveTag(enumSnake);
        if (!tag.empty()) {
            tag = "_" + tag;
        }
        enumSnake = camelToSnake(enumSnake);

        strStripPrefix(value, "VK_");

        const auto &tokens = split(enumSnake, "_");

        for (const auto &it : tokens) {
            const std::string token = it + "_";
            if (!value.starts_with(token)) {
                break;
            }
            value.erase(0, token.size());
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

        value = strFirstUpper(snakeToCamel(value));
        if (isBitmask) {
            strStripSuffix(value, "Bit");
        }
        return "e" + value;
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

    String Registry::getHandleSuperclass(const Handle &data) {
        if (!data.parent) {
            return loader.name;
        }
        //  if (data.name.original == "VkSwapchainKHR") { // TODO unnecessary
        //    return String("VkDevice", true);
        //  }
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
            // std::cout << "Platform node: " << name << "\n";
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
                dst->addAlias(name);
                storage.addAlias(name, alias);
            }
        }

      public:
        explicit ItemInserter(T &storage) : storage(storage) {}

        template <class... Args>
        void insert(Generator &gen, xml::Element e, Args... args) {
            const auto alias = e.optional("alias");
            if (alias) {
                const auto name = e["name"];
                aliased.emplace_back(name, std::string(alias.value()));
            } else {
                storage.items.emplace_back(gen, e, args...);
            }
        }

        void finalize() {
            storage.prepare();
            addAliases();
        }
    };

    void Registry::parseTypes(Generator &gen, xml::Element elem, xml::Element children) {
        if (verbose)
            std::cout << "Parsing declarations" << '\n';

        std::vector<std::pair<std::string_view, std::string_view>> aliasedTypes;
        std::vector<std::pair<std::string, std::string>>           aliasedEnums;

        std::vector<std::pair<std::string, std::string>> bitmasks;
        std::vector<std::pair<std::string, std::string>> structextends;

        ItemInserter addHandles{ handles };

        // iterate contents of <types>, filter only <type> children
        for (const auto &type : xml::vulkanElements(children, "type")) {
            const auto categoryAttrib = type.optional("category");
            if (!categoryAttrib) {
                continue;
            }
            const auto cat  = categoryAttrib.value();
            const auto name = type.optional("name");

            if (cat == "enum") {
                if (name) {
                    auto alias = type.optional("alias");
                    if (alias) {
                        aliasedEnums.emplace_back(name.value(), std::string(alias.value()));
                    } else {
                        enums.items.emplace_back(name.value());
                    }
                }
            } else if (cat == "bitmask") {
                std::string name  = std::string(type.getNested("name"));
                auto        alias = type.optional("alias");
                if (alias) {
                    aliasedEnums.emplace_back(name, std::string(alias.value()));
                } else {
                    std::string bits;
                    auto        req = type.optional("requires");
                    if (req) {
                        bits = req.value();
                    } else {
                        bits = std::regex_replace(name, std::regex("Flags"), "FlagBits");
                        enums.items.emplace_back(bits);
                    }
                    bitmasks.emplace_back(name, bits);
                }
            } else if (cat == "handle") {
                addHandles.insert(gen, type);
            } else if (cat == "struct" || cat == "union") {
                if (name) {
                    const auto alias = type.optional("alias");
                    if (alias) {
                        aliasedTypes.emplace_back(name.value(), alias.value());
                    } else {
                        MetaType::Value const metaType = (cat == "struct") ? MetaType::Struct : MetaType::Union;

                        auto &s = structs.items.emplace_back(gen, name.value(), metaType, type);

                        auto extends = type.optional("structextends");
                        if (extends) {
                            for (const auto &e : split2(extends.value(), ",")) {
                                structextends.emplace_back(name.value(), e);
                                // s.extends.push_back(String(std::string{ e }, true));
                            }
                        }
                    }
                }
            } else if (cat == "define") {
                XMLDefineParser parser{ type.toElement() };
                if (parser.name == "VK_HEADER_VERSION") {
                    headerVersion = parser.value;
                }
            }
        }

        if (headerVersion.empty()) {
            throw std::runtime_error("header version not found.");
        }

        addHandles.finalize();
        for (auto &h : handles.items) {
            if (!h.superclass.empty()) {
                h.parent = &handles[h.superclass];
            }
        }
        for (auto &h : handles.items) {
            h.init(gen);
            if (!h.isSubclass) {
                topLevelHandles.emplace_back(std::ref(h));
            }
        }

        enums.prepare();

        for (auto &b : bitmasks) {
            enums.addAlias(b.first, b.second);
            auto &e     = enums[b.second];
            e.isBitmask = true;
        }
        for (const auto &a : aliasedEnums) {
            auto &e = enums[a.second];
            e.aliases.emplace_back(a.first, true);
        }

        structs.prepare();
        for (auto &a : aliasedTypes) {
            auto &s = structs[a.second.data()];
            s.aliases.emplace_back(a.first.data(), true);
        }
        for (auto &e : structextends) {
            auto &dst = structs[e.first.data()];
            auto &s   = structs[e.second.data()];
            dst.extends.emplace_back(&s);
        }

        handles.addTypes(types);
        enums.addTypes(types);
        structs.addTypes(types);

        if (verbose)
            std::cout << "Parsing declarations done" << '\n';
    }

    void Registry::parseApiConstants(Generator &gen, xml::Element elem) {
        apiConstants.clear();

        std::map<std::string, xml::Element> aliased;
        for (const auto &e : xml::elements(elem.firstChild(), "enum")) {
            const auto alias = e.optional("alias");
            if (alias) {
                aliased.emplace(std::string(alias.value()), e);
                continue;
            }

            String name = std::string(e["name"]);
            name        = snakeToCamel(name);
            strStripPrefix(name, "vk");
            const auto type  = std::string(e["type"]);
            const auto value = std::string(e["value"]);

            apiConstants.emplace_back(name, value, type);
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

            String name = std::string(a.second["name"]);
            name        = snakeToCamel(name);
            strStripPrefix(name, "vk");
            const std::string &type  = target.type;
            const std::string &value = target.value;
            apiConstants.emplace_back(name, value, type);
        }

        for (auto &a : apiConstants) {
            types.emplace(a.name.original, &a);
            // std::cout << "AC: " << a.name << "  " << a.name.original << std::endl;
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

            if (isBitmask) {
                std::string str = std::string{ name };
                str             = std::regex_replace(str, std::regex("FlagBits"), "Flags");
                if (str != name) {
                    enums.addAlias(str, name);
                }
            }

            en.isBitmask = isBitmask;  // maybe unnecessary

            std::vector<std::string> aliased;
            for (const auto &e : xml::vulkanElements(children, "enum")) {
                const auto name = e["name"];

                const auto alias = e.optional("alias");
                if (alias) {
                    const auto comment = e.optional("comment");
                    if (!comment) {
                        aliased.emplace_back(name);
                    }
                    continue;
                }
                const std::string cpp = enumConvertCamel(en.name, std::string(name), isBitmask);

                const auto value = e.optional("value").value_or("");
                String     enumName{ cpp };
                enumName.original = name;
                en.members.emplace_back(enumName, std::string(value));
            }

            for (const auto &a : aliased) {
                const std::string cpp = enumConvertCamel(en.name, a, isBitmask);
                if (!en.containsValue(cpp)) {
                    String v{ cpp };
                    v.original = a;
                    en.members.emplace_back(v, "", true);
                }
            }
        }
    }

    void Registry::parseCommands(Generator &gen, xml::Element elem, xml::Element children) {
        if (verbose)
            std::cout << "Parsing commands" << '\n';

        ItemInserter inserter{ commands };
        for (const auto &commandElement : xml::vulkanElements(children, "command")) {
            inserter.insert(gen, commandElement);
        }
        // TODO order with aliased
        commands.prepare();
        inserter.addAliases([&](auto &a) {
            const auto &name    = a.first;
            const auto &alias   = a.second;
            auto       &command = commands[alias];
            commands.items.emplace_back(gen, command, name);
        });
        commands.prepare();

        commands.addTypes(types);

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

    void Registry::assignCommands(Generator &gen) {
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
                // auto &handle = findHandle("Vk" + className);
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
                auto        superclass = handle.superclass;
                std::string name       = handle.name;
                strStripPrefix(name, superclass);

                if (last->isArrayOut() && (command.nameCat == Command::NameCategory::CREATE || command.nameCat == Command::NameCategory::ALLOCATE ||
                                           command.nameCat == Command::NameCategory::ENUMERATE))
                {
                    // std::cout << ">> VECTOR " << handle.name << ":" << command.name << '\n';
                    handle.vectorVariant = true;
                    handle.vectorCmds.emplace_back(&gen, &handle, command);
                } else {
                    handle.ctorCmds.emplace_back(&gen, &handle, command);
                    //                std::cout << ">> " << handle.name << ": " <<
                    //                command.name << '\n';
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
                std::cerr << "warning: can't assign destructor: "
                          << " (from " << command.name << "): " << e.what() << '\n';
            }
        };

        const auto assignDestruct = [&](Command &command) {
            if (command.name.starts_with("destroy")) {
                assignDestruct2(command, Handle::CreationCategory::CREATE);
            } else if (command.name.starts_with("free")) {
                assignDestruct2(command, Handle::CreationCategory::ALLOCATE);
            }
        };

        for (auto &command : commands) {
            if (assignGetProc(instance, command) || assignGetProc(device, command)) {
                // TODO top
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
                loader.addCommand(gen, command);
                command.top = &loader;
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

        if (verbose)
            std::cout << "Assign commands done" << '\n';
    }

    void Registry::orderStructs() {
        DependencySorter<Struct> sorter(structs);

        sorter.sort("structs", [&](DependencySorter<Struct>::Item &i) {
            for (auto &m : i.data->members) {
                if (!m->isPointer() && m->isStructOrUnion()) {
                    i.addDependency(sorter, m->original.type());
                }
            }
        });
    }

    void Registry::orderHandles() {
        DependencySorter<Handle> sorter(handles);
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

        sorter.sort("handles", [&](DependencySorter<Handle>::Item &i) {
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
        if (!xml::isVulkan(elem)) {
            unsupportedFeatures.push_back(elem);
            return;
        }

        if (verbose)
            std::cout << "Parsing feature" << '\n';

        const auto name   = elem["name"];
        const auto number = elem["number"];

        // spec++;
        for (const auto &require : xml::elements(children, "require")) {
            for (const auto &entry : xml::View(require.firstChild())) {
                const std::string_view value = entry->Value();
                if (value == "enum") {
                    parseEnumExtend(entry, nullptr, "", number.data());
                } else if (value == "command") {
                    commands[entry["name"]].version = number.data();
                } else if (value == "type") {
                    auto *type = find(entry["name"]);
                    if (type) {
                        type->version = number.data();
                    }
                }
            }
        }
        if (verbose)
            std::cout << "Parsing feature done" << '\n';
    }

    void Registry::parseExtensions(Generator &gen, xml::Element elem, xml::Element children) {
        const auto disableTypes = [&](xml::Element e) {
            for (const auto &entry : xml::View(e.firstChild())) {
                const std::string_view value = entry->Value();
                if (value == "type" || value == "command") {
                    const auto name = entry["name"];
                    if (name.find("FlagBits") != std::string::npos) {
                        std::string tmp = std::string{ name };
                        tmp             = std::regex_replace(tmp, std::regex("FlagBits"), "Flags");
                        auto *type      = find(tmp);
                        if (type) {
                            type->setUnsuppored();
                        }
                    }
                    if (name.find("Flags") != std::string::npos) {
                        std::string tmp = std::string{ name };
                        tmp             = std::regex_replace(tmp, std::regex("Flags"), "FlagBits");
                        auto *type      = find(tmp);
                        if (type) {
                            type->setUnsuppored();
                        }
                    }

                    auto *type = find(name);
                    if (type) {
                        type->setUnsuppored();
                    }
                    //        else {
                    //          std::cout << "not found: " << name << "\n";
                    //        }
                }
            }
        };

        if (verbose)
            std::cout << "Parsing extensions" << '\n';

        std::vector<xml::Element> nodes;

        // iterate contents of <extensions>, filter only <extension> children
        for (const auto &extension : xml::elements(children, "extension")) {
            if (!xml::isVulkanExtension(extension)) {
                for (const auto &require : xml::elements(extension.firstChild(), "require")) {
                    disableTypes(require);
                }
                continue;
            }

            const auto name = extension["name"];
            // std::cout << "Extension: " << name << '\n';
            // if (supported) {
            Platform  *platform       = nullptr;
            const auto platformAttrib = extension.optional("platform");
            if (platformAttrib) {
                platform = &platforms[platformAttrib.value()];
            }

            bool enabled = defaultWhitelistOption;
            // std::cout << "add ext: " << name << '\n';
            auto &ext   = extensions.items.emplace_back(std::string{ name }, platform, true, enabled);
            ext.version = no_ver;
            //            std::cout << "ext: " << ext << '\n';
            //            std::cout << "> n: " << ext->name.original << '\n';

            nodes.push_back(extension);
            // }
        }
        extensions.prepare();

        for (const auto &extension : nodes) {
            // for (XMLElement *extension : Elements(node) | ValueFilter("extension")) {
            const auto name = extension["name"];

            Extension *ext = &extensions[name];

            // iterate contents of <extension>, filter only <require> children
            for (const auto &require : xml::elements(extension.firstChild(), "require")) {
                if (!xml::isVulkan(require)) {
                    disableTypes(require);
                    continue;
                }

                // iterate contents of <require>
                for (const auto &entry : xml::View(require.firstChild()) | xml::filter_vulkan()) {
                    const std::string_view value = entry->Value();

                    auto protect = entry.optional("protect").value_or("");
                    if (value == "command") {
                        const auto name    = entry["name"];
                        auto      &command = commands[name];
                        if (!isInContainter(ext->commands, &command)) {
                            ext->commands.push_back(&command);
                        }
                        command.bind(ext, protect);

                        //          if (command.vulkanSpec == 0) {
                        //            command.vulkanSpec = 255;
                        //          }

                    } else if (value == "type" /*&& ext && !ext->protect.empty()*/) {
                        std::string name = std::string(entry["name"]);
                        if (!name.starts_with("Vk")) {
                            continue;
                        }

                        auto *type = find(name);
                        if (!type) {  // TODO redundant?
                            name = std::regex_replace(name, std::regex("Flags"), "FlagBits");
                            type = find(name);
                        }
                        if (type) {
                            if (!isInContainter(ext->types, type)) {
                                ext->types.push_back(type);
                            }
                            type->bind(ext, protect);
                            //              if (type->vulkanSpec == 0) {
                            //                type->vulkanSpec = 255;
                            //              }
                        }
                    } else if (value == "enum") {
                        parseEnumExtend(entry, ext, protect, no_ver);
                    }
                }
            }
        }
        if (verbose)
            std::cout << "Parsing extensions done" << '\n';
    }

    void Registry::parseEnumExtend(const xml::Element &elem, Extension *ext, const std::string_view optProtect, const char *spec) {
        const auto name = elem.optional("name");
        if (!name) {
            std::cerr << "Enum ext no name"
                      << "\n";
            return;
        }
        const auto extends = elem.optional("extends");
        // const char *bitpos = node.Attribute("bitpos");
        const auto alias = elem.optional("alias");
        if (extends && !alias) {
            auto &e = enums[extends.value()];
            auto *m = e.find(name.value());
            if (m) {
                m->bind(ext, optProtect);
                m->version = spec;
            } else {
                std::string cpp = enumConvertCamel(e.name, std::string(name.value()), e.isBitmask);
                String      v{ cpp };
                v.original = name.value();
                vkr::EnumValue data{ v, "", alias ? true : false };
                data.bind(ext, optProtect);
                data.version = spec;
                e.members.push_back(data);
            }
        } else {
            auto *type = find(name.value());
            if (type) {
                type->bind(ext, optProtect);
            }
        }
    }

    void Registry::createErrorClasses() {
        auto &e = enums["VkResult"];
        for (const auto &m : e.members) {
            if (!m.isAlias && m.name.starts_with("eError")) {
                errorClasses.emplace_back(m);
            }
        }
    }

    void Registry::disableUnsupportedFeatures() {
        for (const auto &e : unsupportedFeatures) {
            for (const auto &require : xml::elements(e.firstChild(), "require")) {
                for (const auto &entry : xml::View(require.firstChild())) {
                    const auto name = entry.optional("name");
                    if (!name) {
                        continue;
                    }
                    if (name.value() == "VkPipelineCacheCreateFlagBits") {
                        continue;
                    }
                    const std::string_view value = entry->Value();
                    if (value == "type" || value == "command") {
                        auto *type = find(name.value());
                        if (type) {
                            type->setUnsuppored();
                        }
                    }
                }
            }
        }
        unsupportedFeatures.clear();
    }

    void Registry::buildDependencies() {
        if (verbose)
            std::cout << "Building dependencies information" << '\n';

        //  std::map<const std::string_view, BaseType *> deps;
        //  for (auto &d : enums.items) {
        //    deps.emplace(d.name.original, &d);
        //  }
        //  for (auto &d : structs.items) {
        //    deps.emplace(d.name.original, &d);
        //  }
        //  for (auto &d : handles.items) {
        //    deps.emplace(d.name.original, &d);
        //  }
        //
        //  const auto find = [&](const std::string &type) -> BaseType * {
        //    if (!type.starts_with("Vk")) {
        //      return nullptr;
        //    }
        //    auto it = deps.find(type);
        //    if (it != deps.end()) {
        //      return it->second;
        //    }
        //    auto it2 = enums.find(type);
        //    if (it2 != enums.end()) {
        //      return &(*it2);
        //    }
        //    return nullptr;
        //  };

        for (auto &s : structs) {
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
            for (const auto &m : command._params) {
                const std::string &type = m->original.type();
                auto              *d    = find(type);
                if (d) {
                    command.dependencies.insert(d);
                }
            }
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
        if (isLoaded()) {
            unload();
        }

        if (!loadXML(xmlPath)) {
            unload();
            return false;
        }

        parseXML(gen);

        createErrorClasses();
        disableUnsupportedFeatures();

        buildDependencies();

        assignCommands(gen);
        orderStructs();
        orderHandles();

        /* old debug print
        //    std::cout << "commands: {" << '\n';
        //    for (auto &c : commands.items) {
        //        std::cout << "  " << c.name << '\n';
        //        for (auto &p : c.outParams) {
        //            std::cout << "    out: " << p.get().original.type() << '\n';
        //        }
        //    }
        //    std::cout << "}" << '\n';
        if (false) {

          //        std::cout << "Static commands: " << staticCommands.size() << " { "
          //                  << '\n';
          //
          //        for (CommandData &c : staticCommands) {
          //            std::cout << "  " << c.name << '\n';
          //        }
          //        std::cout << "}" << '\n';

          bool verbose = false;
          for (const Handle &h : handles.ordered) {
            std::cout << "Handle: " << h.name;
            // std::cout  << " (" << h.name.original << ")";
            if (h.uniqueVariant()) {
              std::cout << " (unique)";
            }
            if (h.vectorVariant) {
              std::cout << " (vector)";
            }
            std::cout << '\n';
            std::cout << "  super: " << h.superclass;
            if (h.parent) {
              std::cout << ", parent: " << h.parent->name;
            }
            std::cout << '\n';
            if (h.dtorCmd) {
              //                std::cout << "  DTOR: " << h.dtorCmd->name << " (" << h.dtorCmd->name.original
              //                          << ")" << '\n';
            }

            for (const auto &c : h.ctorCmds) {
              std::cout << "    ctor: " << c.name.original << std::endl;
            }
            //            if (!h.members.empty()) {
            //                std::cout << "  commands: " << h.members.size();
            //                if (verbose) {
            //                    std::cout << " {" << '\n';
            //                    for (const auto &c : h.members) {
            //                        std::cout << "  " << c.name.original << " ";
            //                        if (c.raiiOnly) {
            //                            std::cout << "R";
            //                        }
            //                        std::cout << '\n';
            //                    }
            //                    std::cout << "}";
            //                }
            //                std::cout << '\n';
            //            }
            std::cout << '\n';
          }
        }
         */

        const auto lockDependency = [&](const std::string &name) {
            auto *type = find(name);
            if (type) {
                type->forceRequired = true;
            } else {
                std::cerr << "Can't find element: " << name << '\n';
            }
        };

        lockDependency("VkStructureType");
        lockDependency("VkResult");
        lockDependency("VkObjectType");
        lockDependency("VkDebugReportObjectTypeEXT");

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
            if (d.isSuppored()) {
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

        types.clear();
        headerVersion.clear();
        platforms.clear();
        tags.clear();
        enums.clear();
        handles.clear();
        structs.clear();
        extensions.clear();
        staticCommands.clear();
        commands.clear();
        errorClasses.clear();
        loader.clear();
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

}  // namespace vkgen

namespace vkgen::vkr
{

    ClassCommand::operator BaseType() const {
        return *src;
    }

    ClassCommand::ClassCommand(const Generator *gen, const Handle *cls, Command &o)
      : cls(cls), src(&o), name(gen->convertCommandName(o.name.original, cls->name), false) {
        name.original = o.name.original;

        if (cls->name.original == "VkCommandBuffer" && name.starts_with("cmd")) {
            auto old = name;
            name     = strFirstLower(name.substr(3));
            // std::cout << "convert name: " << old << " -> " << name << std::endl;
        }
    }

    Handle::Handle(Generator &gen, xml::Element elem)
      : BaseType(MetaType::Handle, elem.getNested("name"), true)
      , superclass(std::string{ elem.optional("parent").value_or("") })
      , vkhandle(VariableDataInfo{ .vktype     = this->name.original,
                                   .identifier = "m_" + strFirstLower(this->name),
                                   .assigment  = " = {}",
                                   .ns         = Namespace::VK,
                                   .flag       = VariableData::Flags::CLASS_VAR_VK | VariableData::Flags::CLASS_VAR_RAII,
                                   .metaType   = MetaType::Handle }) {
        isSubclass = name != "Instance" && name != "Device";
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
            std::string const s = gen.genOptional(m, [&](std::string &output) {
                effectiveMembers++;
                if (order) {
                    stage.emplace(m.name.original, &m);
                } else {
                    filteredMembers.push_back(&m);
                }
            });
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
//            if (gen.getConfig().gen.expApi) {
//                ownerUnique->setAssignment("");
//                ownerUnique->setReference(true);
//            }
//            else {
//                ownerUnique->setAssignment(" = {nullptr}");
//                ownerUnique->setReference(false);
//            }
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

    bool Enum::containsValue(const std::string &value) const {
        return std::ranges::any_of(members, [&](const auto &m) { return m.name == value; });
    }

    EnumValue *Enum::find(const std::string_view value) noexcept {
        for (auto &m : members) {
            if (m.name.original == value) {
                return &m;
            }
        }
        return nullptr;
    }

    void Command::init() {
        _params.bind();

        initParams();

        bool hasHandle    = false;
        bool canTransform = false;
        for (auto &p : _params) {
            if (p->isOutParam()) {
                if (p->getArrayVars().empty()) {
                    outParams.push_back(std::ref(*p));
                    if (p->isHandle()) {
                        hasHandle = true;
                    }
                }
                canTransform = true;
            }

            auto sizeVar = p->getLengthVar();
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
        if (canTransform) {
            setFlagBit(CommandFlags::CPP_VARIANT, true);
        }

        prepared = true;
    }

    Command::Command(const Generator &gen, xml::Element elem  //, const std::string &className
                     ) noexcept
      : BaseType(MetaType::Command) {
        // iterate contents of <command>
        std::string dbg;
        std::string name;
        // add more space to prevent reallocating later
        _params.reserve(8);
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

        init();
    }

    Command::Command(const Registry &reg, const vkr::Command &o, const std::string_view alias) : BaseType(MetaType::Command) {
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

        init();
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

    Struct::Struct(Generator &gen, const std::string_view name, MetaType::Value type, const xml::Element &e) : BaseType(type, name, true) {
        auto returnedonly = e.optional("returnedonly");
        if (returnedonly.value_or("") == "true") {
            this->returnedonly = true;
        }

        // iterate contents of <type>, filter only <member> children
        for (const auto &member : xml::vulkanElements(e.firstChild(), "member")) {
            auto &v = members.emplace_back(std::move(std::make_unique<VariableData>(gen, member)));

            const std::string &type = v->type();
            const std::string &name = v->identifier();

            if (const char *values = member->ToElement()->Attribute("values")) {
                std::string value = gen.enumConvertCamel(type, values);
                v->setAssignment(" = " + type + "::" + value);
                if (v->original.type() == "VkStructureType") {  // save sType information for structType
                    structTypeValue = value;
                }
            }
        }
        members.bind();
    }

}  // namespace vkgen::vkr
