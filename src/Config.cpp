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
#include "Config.hpp"

#include <cmath>
#include "Generator.hpp"

namespace vkgen
{

    template <size_t I = 0, typename... T>
    void saveConfigParam(tinyxml2::XMLElement *parent, const std::tuple<T...> &t) {
        const auto &data = std::get<I>(t);
        saveConfigParam(parent, data);
        // unroll tuple
        if constexpr (I + 1 != sizeof...(T)) {
            saveConfigParam<I + 1>(parent, t);
        }
    }

    template <typename T>
    void saveConfigParam(tinyxml2::XMLElement *parent, const T &data) {
        if constexpr (std::is_base_of<ConfigGroup, T>::value) {
            // std::cout << "export: " << data.name << '\n';
            auto *elem = parent->GetDocument()->NewElement(data.name.c_str());
            saveConfigParam(elem, data.reflect());
            if (elem->NoChildren()) {
                parent->GetDocument()->DeleteNode(elem);
            }
            else {
                parent->InsertEndChild(elem);
            }
        } else {
            // std::cout << "export:   * leaf" << '\n';
            // export
            if (data.isDirty()) {
                auto *elem = parent->GetDocument()->NewElement("");
                elem->SetAttribute("name", data.name.c_str());
                data.xmlExport(elem);
                parent->InsertEndChild(elem);
            }
        }
    }

    template <typename... Tp>
    static void loadConfig(tinyxml2::XMLElement *parent, const std::tuple<Tp...> &t);

    template <typename T>
    static void loadConfigParam(tinyxml2::XMLElement *parent, std::map<std::string, tinyxml2::XMLElement *> &nodes, T &data) {
        if constexpr (std::is_base_of<vkgen::ConfigGroup, T>::value) {
            // std::cout << "load: " << data.name << '\n';
            if (parent) {
                auto it = nodes.find(data.name);
                if (it != nodes.end()) {
                    tinyxml2::XMLElement *elem = it->second;
                    nodes.erase(it);
                    if (data.name != "whitelist") {
                        loadConfig(elem, data.reflect());
                    }
                }
            }
        } else {
            // import
            data.reset();
            if (parent) {
                for (tinyxml2::XMLElement &elem : vkgen::Elements(parent)) {
                    const char *name = elem.Attribute("name");
                    if (name && std::string_view{ name } == data.name) {
                        // std::cout << "load:   * leaf: " << data.name << '\n';
                        data.xmlImport(&elem);
                    }
                }
            }
        }
    }

    template <size_t I = 0, typename... Tp>
    static void loadConfigParam(tinyxml2::XMLElement *parent, std::map<std::string, tinyxml2::XMLElement *> &nodes, const std::tuple<Tp...> &t) {
        auto &data = std::get<I>(t);
        // import
        loadConfigParam(parent, nodes, data);
        // unroll tuple
        if constexpr (I + 1 != sizeof...(Tp)) {
            loadConfigParam<I + 1>(parent, nodes, t);
        }
    }

    template <typename... Tp>
    static void loadConfig(tinyxml2::XMLElement *parent, const std::tuple<Tp...> &t) {
        std::map<std::string, tinyxml2::XMLElement *> nodes;
        for (auto it = parent->FirstChildElement(); it != nullptr; it = it->NextSiblingElement()) {
            auto name = it->Attribute("name");
            if (name) {
                continue;
            }
            nodes.emplace(std::string(it->Value()), it);
        }

        loadConfigParam(parent, nodes, t);

        for (const auto &n : nodes) {
            std::cerr << "config load: unknown element: ";
            auto name = n.second->Attribute("name");
            if (name) {
                std::cerr << " {" << name << "}";
            }
            std::cerr << std::endl;
        }
    }

    //    void mockTestConfigs() {
    //        Config const conf = {};
    //        mockConfig(conf, cfg.reflect(), "[R]", static_cast<size_t>(0));
    //    }

    template <size_t I, typename... T>
    void mockConfig(const Config &cfg, const std::tuple<T...> &t, std::string dbg, size_t i) {
        Config      copy = cfg;
        const auto &data = std::get<I>(t);
        bool        v    = (1 << I) & i;
        mockConfig(copy, data, dbg, v);
        // unroll tuple
        if constexpr (I + 1 != sizeof...(T)) {
            mockConfig<I + 1>(copy, t, dbg, i);
        }
        // std::cout << "save" << std::endl;
    }

    template <typename T>
    void mockConfig(const Config &cfg, const T &data, std::string dbg, bool v) {
        // group of nodes
        if constexpr (std::is_base_of<ConfigGroup, T>::value) {
            auto s = std::tuple_size_v<decltype(data.reflect())>;
            auto n = std::pow(2, s);
            for (size_t i = 0; i < n; i++) {
                std::string d = dbg + "[" + std::to_string(i) + "]";
                std::cout << "export " << d << ": " << data.name << '\n';
                mockConfig(cfg, data.reflect(), d, i);
            }
        }
        // leaf node
        else
        {
            if constexpr (std::is_same<typename T::underlying_type, bool>::value) {
                std::cout << "export:   * " << data.name << " " << v << '\n';
            }
        }
    }

    void Config::save(Generator &gen, const std::string &filename) {
        using namespace tinyxml2;

        XMLDocument doc;

        XMLElement *root = doc.NewElement("config");
        root->SetAttribute("vk_version", gen.headerVersion.c_str());

        XMLElement *whitelist = doc.NewElement("whitelist");

        configBuildList("platforms", gen.platforms.items, whitelist);
        configBuildList("extensions", gen.extensions.items, whitelist);
        configBuildList("features", gen.features.items, whitelist);
        configBuildList("structs", gen.structs.items, whitelist);
        configBuildList("enums", gen.enums.items, whitelist);
        configBuildList("handles", gen.handles.items, whitelist);
        if (!gen.orderedCommands.empty()) {
            configBuildList("commands", gen.orderedCommands, whitelist);
        } else {
            configBuildList("commands", gen.commands.items, whitelist);
        }
        // std::cout << "WL children: " << whitelist->NoChildren() << "\n";

        saveConfigParam(root, gen.cfg.reflect());
        if (!whitelist->NoChildren()) {
            root->InsertEndChild(whitelist);
        }

        doc.InsertFirstChild(root);
        XMLError const e = doc.SaveFile(filename.c_str());
        if (e == XMLError::XML_SUCCESS) {
            std::cout << "Saved config file: " << filename << '\n';
        }
    }

    void Config::load(Generator &gen, const std::string &filename) {
        using namespace tinyxml2;

        XMLDocument doc;
        if (XMLError const e = doc.LoadFile(filename.c_str()); e != XML_SUCCESS) {
            auto msg = "XML config load failed: " + std::to_string(e) + " " + std::string{ doc.ErrorStr() } + " (file: " + filename + ")";
            // std::cerr << msg << '\n';
            throw std::runtime_error(msg);
        }

        XMLElement *root = doc.RootElement();
        if (!root) {
            throw std::runtime_error("XML config load failed: file is empty");
        }

        if (std::string_view(root->Value()) != gen.cfg.name) {
            throw std::runtime_error("XML config load failed: wrong XML structure");
        }

        loadConfig(root, gen.cfg.reflect());

        auto bEnums   = WhitelistBinding{ &gen.enums.items, "enums" };
        auto bPlats   = WhitelistBinding{ &gen.platforms.items, "platforms" };
        auto bExts    = WhitelistBinding{ &gen.extensions.items, "extensions" };
        auto bFeatures = WhitelistBinding{ &gen.features.items, "features" };
        auto bStructs = WhitelistBinding{ &gen.structs.items, "structs" };
        auto bCmds    = WhitelistBinding{ &gen.commands.items, "commands" };
        auto bHandles = WhitelistBinding{ &gen.handles.items, "handles" };
        auto bindings = { dynamic_cast<AbstractWhitelistBinding *>(&bPlats),   dynamic_cast<AbstractWhitelistBinding *>(&bExts),
                           dynamic_cast<AbstractWhitelistBinding *>(&bFeatures),
                          dynamic_cast<AbstractWhitelistBinding *>(&bEnums),   dynamic_cast<AbstractWhitelistBinding *>(&bStructs),
                          dynamic_cast<AbstractWhitelistBinding *>(&bHandles), dynamic_cast<AbstractWhitelistBinding *>(&bCmds) };

        class ConfigVisitor : public XMLVisitor
        {
            AbstractWhitelistBinding *parent;

          public:
            explicit ConfigVisitor(AbstractWhitelistBinding *parent) : parent(parent) {}

            bool Visit(const XMLText &text) override {
                std::string_view const tag   = text.Parent()->Value();
                std::string_view const value = text.Value();
                // std::cout << "tag: " << tag << '\n';
                if (tag == parent->name) {
                    for (auto line : split2(value, "\n")) {
                        std::string const t = regex_replace(std::string{ line }, std::regex("(^\\s*)|(\\s*$)"), "");
                        if (!t.empty()) {
                            if (t == "*") {
                                parent->all = true;
                            }
                            else if (!parent->add(t)) {
                                std::cerr << "[Config load] Duplicate: " << t << '\n';
                            }
                        }
                    }
                } else if (tag == "regex") {
                    // std::cout << "RGX: " << value << '\n';
                    try {
                        parent->add(std::regex{ std::string{ value } });
                    }
                    catch (const std::regex_error &err) {
                        std::cerr << "[Config load]: regex error: " << err.what() << '\n';
                    }
                }
                return true;
            }
        };

        XMLElement *whitelist = root->FirstChildElement("whitelist");
        if (whitelist) {
            for (XMLNode const &n : Elements(whitelist)) {
                bool accepted = false;
                for (const auto &b : bindings) {
                    if (b->name != n.Value()) {
                        continue;
                    }
                    accepted = true;

                    ConfigVisitor prt(b);
                    n.Accept(&prt);
                    b->found = true;
                }
                if (!accepted) {
                    std::cerr << "[Config load] Warning: unknown element: " << n.Value() << " at line " << n.GetLineNum() << '\n';
                }
            }

            // std::cout << "[Config load] whitelist built" << '\n';

            for (const auto &b : bindings) {
                b->prepare();
            }

            for (const auto &b : bindings) {
                b->apply();
            }

            gen.orderedCommands.clear();
            gen.orderedCommands.reserve(bCmds.ordered.size());
            for (auto &c : bCmds.ordered) {
                auto it = gen.commands.find(c);
                if (it != gen.commands.end()) {
                    gen.orderedCommands.emplace_back(*it);
                }
            }

            if (bExts.found) {
                for (auto &e : gen.extensions) {
                    if (e.isEnabled()) {
                        for (auto &c : e.commands) {
                            c.get().setEnabled(true);
                        }
                        for (auto &s : e.structs) {
                            s.get().setEnabled(true);
                        }
                        for (auto &e : e.enums) {
                            e.get().setEnabled(true);
                        }
                    }
                }
            }

            // if (!bCmds.found) {
                for (auto &f : gen.features) {
                    // std::cout << "F: " << f.isEnabled() << "\n";
                    if (f.isEnabled()) {
                        for (auto &c : f.commands) {
                            c.get().setEnabled(true);
                        }
                        for (auto &s : f.structs) {
                            s.get().setEnabled(true);
                        }
                        for (auto &e : f.enums) {
                            e.get().setEnabled(true);
                        }
                        for (auto &t : f.promotedTypes) {
                            t.get().setEnabled(true);
                        }
                    }
                }
            // }

            // std::cout << "[Config load] whitelist applied" << '\n';
        }

        std::cout << "[Config load] Loaded: " << filename << '\n';
    }

    template <typename T>
    void configBuildList(const std::string &name, const std::map<std::string, T> &from, tinyxml2::XMLElement *parent, const std::string &comment) {
        WhitelistBuilder out;
        for (const auto &p : from) {
            out.add(p.second);
        }
        out.insertToParent(parent, name, comment);
    }

    template <typename T>
    void configBuildList(const std::string &name, const std::vector<T> &from, tinyxml2::XMLElement *parent, const std::string &comment) {
        WhitelistBuilder out;
        for (const auto &p : from) {
            out.add(p);
        }
        out.insertToParent(parent, name, comment);
    }

    template <typename T>
    void WhitelistBinding<T>::prepare() noexcept {
        for (auto &e : *dst) {
            e.setEnabled(false);
        }
    }

    template <typename T>
    void WhitelistBinding<T>::apply() noexcept {
        if (all) {
            for (auto &e : *dst) {
                e.setEnabled(true);
            }
        }
        for (auto &e : *dst) {
            bool match = false;
            auto it    = filter.find(e.name.original);
            if (it != filter.end()) {
                match = true;
                filter.erase(it);
            }
            if (!match) {
                for (const auto &r : regexes) {
                    if (std::regex_match(e.name.original, r)) {
                        match = true;
                        break;
                    }
                }
            }
            if (match) {
                e.setEnabled(true);
            }
            // std::cout << "enable: " << e.name << " " << e.typeString() << std::endl;
        }
        for (auto &f : filter) {
            std::cerr << "[Config load] Not found: " << f << " (" << name << ")" << '\n';
        }
    }

    template <>
    void ConfigWrapper<Macro>::xmlExport(tinyxml2::XMLElement *elem) const {
        // std::cout << "export macro:" << name << '\n';
        elem->SetName("macro");
        elem->SetAttribute("define", data.define.c_str());
        elem->SetAttribute("value", data.value.c_str());
        elem->SetAttribute("usesDefine", data.usesDefine ? "true" : "false");
    }

    template <>
    void ConfigWrapper<Define>::xmlExport(tinyxml2::XMLElement *elem) const {
        // std::cout << "export macro:" << name << '\n';
        elem->SetName("define");
        elem->SetAttribute("define", data.define.c_str());
        elem->SetAttribute("value", std::to_string(data.state).c_str());
    }

//    template <>
//    void ConfigWrapper<NDefine>::xmlExport(tinyxml2::XMLElement *elem) const {
//        // std::cout << "export macro:" << name << '\n';
//        elem->SetName("ndefine");
//        elem->SetAttribute("define", data.define.c_str());
//        elem->SetAttribute("value", std::to_string(data.state).c_str());
//    }

    template <>
    void ConfigWrapper<std::string>::xmlExport(tinyxml2::XMLElement *elem) const {
        // std::cout << "export: " << name << '\n';
        elem->SetName("string");
        elem->SetAttribute("value", data.c_str());
    }

    template <>
    void ConfigWrapper<int>::xmlExport(tinyxml2::XMLElement *elem) const {
        // std::cout << "export: " << name << '\n';
        elem->SetName("int");
        elem->SetAttribute("value", data);
    }

    template <>
    void ConfigWrapper<bool>::xmlExport(tinyxml2::XMLElement *elem) const {
        // std::cout << "export: " << name << '\n';
        elem->SetName("bool");
        elem->SetAttribute("value", data ? "true" : "false");
    }

    static bool checkValue(tinyxml2::XMLElement *elem, const std::string_view value) {
        if (std::string_view{ elem->Value() } != value) {
            std::cerr << "[config import] node value mismatch at line " << elem->GetLineNum() << ", expected " << value << '\n';
            return false;
        }
        return true;
    }

    template <>
    bool ConfigWrapper<Macro>::xmlImport(tinyxml2::XMLElement *elem) const {
        // std::cout << "import macro:" << name << '\n';
        if (!checkValue(elem, "macro")) {
            return false;
        }

        const char *v = elem->Attribute("usesDefine");
        if (v) {
            if (std::string_view{ v } == "true") {
                const_cast<ConfigWrapper<Macro> *>(this)->data.usesDefine = true;
            } else if (std::string_view{ v } == "false") {
                const_cast<ConfigWrapper<Macro> *>(this)->data.usesDefine = false;
            } else {
                std::cerr << "[config import] unknown value" << '\n';
                return false;
            }
        }
        v = elem->Attribute("define");
        if (v) {
            const_cast<ConfigWrapper<Macro> *>(this)->data.define = v;
        }
        v = elem->Attribute("value");
        if (v) {
            const_cast<ConfigWrapper<Macro> *>(this)->data.value = v;
        }

        return true;
    }

    template <>
    bool ConfigWrapper<Define>::xmlImport(tinyxml2::XMLElement *elem) const {
        if (!checkValue(elem, "define")) {
            return false;
        }
        const char *v = elem->Attribute("define");
        if (v) {
            const_cast<ConfigWrapper<Define> *>(this)->data.define = v;
        }
        v = elem->Attribute("value");
        if (v) {
            int state = toInt(v);
            if (state < 0 || state > Define::COND_ENABLED) {
                std::cerr << "[config import] invalid value" << '\n';
                return false;
            }
            const_cast<ConfigWrapper<Define> *>(this)->data.state = static_cast<Define::State>(state);
        }

        return true;
    }

//    template <>
//    bool ConfigWrapper<NDefine>::xmlImport(tinyxml2::XMLElement *elem) const {
//        if (std::string_view{ elem->Value() } != "ndefine") {
//            std::cerr << "[config import] node value mismatch" << '\n';
//            return false;
//        }
//        const char *v = elem->Attribute("define");
//        if (v) {
//            const_cast<ConfigWrapper<NDefine> *>(this)->data.define = v;
//        }
//        v = elem->Attribute("value");
//        if (v) {
//            int state = toInt(v);
//            if (state < 0 || state > Define::COND_ENABLED) {
//                std::cerr << "[config import] invalid value" << '\n';
//                return false;
//            }
//            const_cast<ConfigWrapper<NDefine> *>(this)->data.state = static_cast<Define::State>(state);
//        }
//
//        return true;
//    }

    template <>
    bool ConfigWrapper<std::string>::xmlImport(tinyxml2::XMLElement *elem) const {
        if (!checkValue(elem, "string")) {
            return false;
        }

        const char *v = elem->Attribute("value");
        if (v) {
            const_cast<ConfigWrapper<std::string> *>(this)->data = v;
        }
        return true;
    }

    template <>
    bool ConfigWrapper<int>::xmlImport(tinyxml2::XMLElement *elem) const {
        // std::cout << "import: " << name << '\n';
        if (!checkValue(elem, "int")) {
            return false;
        }

        const char *v = elem->Attribute("value");
        if (v) {
            try {
                int value                                    = toInt(v);
                const_cast<ConfigWrapper<int> *>(this)->data = value;
            }
            catch (std::runtime_error &) {
                std::cerr << "[config import] unknown value: " << v << '\n';
                return false;
            }
        }
        return true;
    }

    template <>
    bool ConfigWrapper<bool>::xmlImport(tinyxml2::XMLElement *elem) const {
        // std::cout << "import: " << name << '\n';
        if (!checkValue(elem, "bool")) {
            return false;
        }

        const char *v = elem->Attribute("value");
        if (v) {
            if (std::string_view{ v } == "true") {
                const_cast<ConfigWrapper<bool> *>(this)->data = true;
            } else if (std::string_view{ v } == "false") {
                const_cast<ConfigWrapper<bool> *>(this)->data = false;
            } else {
                std::cerr << "[config import] unknown value" << '\n';
                return false;
            }
        }
        return true;
    }
}  // namespace vkgen