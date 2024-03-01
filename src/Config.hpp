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

#ifndef GENERATOR_CONFIG_HPP
#define GENERATOR_CONFIG_HPP

#include "Registry.hpp"

namespace vkgen
{

    class Generator;

    template <typename T>
    struct ConfigWrapper
    {
        using underlying_type = T;

        std::string name;
        T           data;
        T           _default;

        ConfigWrapper(std::string name, T &&data) : name(name), data(data), _default(data) {}

        operator T &() {
            return data;
        }

        operator const T &() const {
            return data;
        }

        T *operator->() {
            return &data;
        }

        const T *operator->() const {
            return &data;
        }

        void xmlExport(tinyxml2::XMLElement *elem) const;

        bool xmlImport(tinyxml2::XMLElement *elem) const;

        bool isDifferent() const {
            return data != _default;
        }

        void reset() const {
            const_cast<ConfigWrapper *>(this)->data = _default;
        }
    };

    struct ConfigGroup
    {
        std::string name;
    };

    struct ConfigGroupWhitelist : public ConfigGroup
    {
        ConfigGroupWhitelist() : ConfigGroup{ "whitelist" } {}

        ConfigWrapper<bool> flag{ "flag", false };

        [[nodiscard]] auto reflect() const {
            return std::tie(flag);
        }
    };

    struct ConfigGroupMacro : public ConfigGroup
    {
        ConfigGroupMacro() : ConfigGroup{ "macro" } {
            // mNamespaceRAII.data.parent = &mNamespace.data;
        }

        Macro mNamespaceSTD{ "", "std", false };
        ConfigWrapper<Macro> mNamespace{ "namespace", { "VULKAN_HPP_NAMESPACE", "vk", true} };
        ConfigWrapper<Macro> mNamespaceRAII{ "namespace_raii", { "VULKAN_HPP_RAII_NAMESPACE", "raii", true } };
        Macro mConstexpr{ "VULKAN_HPP_CONSTEXPR", "constexpr", true };
        Macro mConstexpr14{ "VULKAN_HPP_CONSTEXPR_14", "constexpr", true };
        Macro mInline{ "VULKAN_HPP_INLINE", "inline", true };
        Macro mNoexcept{ "VULKAN_HPP_NOEXCEPT", "noexcept", true };
        Macro mExplicit{ "VULKAN_HPP_TYPESAFE_EXPLICIT", "explicit", true };
        Macro mDispatch{ "VULKAN_HPP_DEFAULT_DISPATCHER_ASSIGNMENT", "", true };
        Macro mDispatchType{ "VULKAN_HPP_DEFAULT_DISPATCHER_TYPE", "DispatchLoaderStatic", true };

        [[nodiscard]] auto reflect() const {
            return std::tie(mNamespace, mNamespaceRAII);
        }
    };

    struct ConfigGroupRAII : public ConfigGroup
    {
        ConfigGroupRAII() : ConfigGroup{ "raii" } {}

        ConfigWrapper<bool> enabled{ "enabled", { true } };
        ConfigWrapper<bool> interop{ "interop", { false } };
        ConfigWrapper<bool> staticInstancePFN{ "static_instance_pfn", { false } };
        ConfigWrapper<bool> staticDevicePFN{ "static_device_pfn", { false } };

        [[nodiscard]] auto reflect() const {
            return std::tie(interop, staticInstancePFN, staticDevicePFN);
        }
    };

    struct ConfigGroupGen : public ConfigGroup
    {
        ConfigGroupGen() : ConfigGroup{ "gen" } {}

        ConfigWrapper<bool> cppModules{ "modules", false };
        ConfigWrapper<bool> cppFiles{ "cpp_files", false };  // WIP
        ConfigWrapper<bool> expApi{ "exp_api", false };

        ConfigWrapper<bool> internalFunctions{ "internal_functions", false };
        ConfigWrapper<bool> internalVkResult{ "internal_vkresult", true };

        ConfigWrapper<bool> smartHandles{ "smart_handles", true };

        ConfigWrapper<bool> dispatchParam{ "dispatch_param", true };
        ConfigWrapper<bool> dispatchTemplate{ "dispatch_template", true };
        ConfigWrapper<bool> dispatchLoaderStatic{ "dispatch_loader_static", true };
        ConfigWrapper<bool> useStaticCommands{ "static_link_commands", false };  // move
        ConfigWrapper<bool> allocatorParam{ "allocator_param", true };
        ConfigWrapper<bool> resultValueType{ "use_result_value_type", true };
        ConfigWrapper<bool> dispatchTableAsUnique{ "dispatch_table_as_unique", { false } };

        ConfigWrapper<bool> functionsVecAndArray{ "functions_vec_array", { false } };

        ConfigWrapper<NDefine> structConstructors{ "struct_constructors", { "VULKAN_HPP_NO_STRUCT_CONSTRUCTORS", Define::COND_ENABLED } };
        ConfigWrapper<NDefine> structSetters{ "struct_setters", { "VULKAN_HPP_NO_STRUCT_SETTERS", Define::COND_ENABLED } };
        ConfigWrapper<NDefine> structCompare{ "struct_compare", { "VULKAN_HPP_NO_STRUCT_COMPARE", Define::ENABLED } };
        ConfigWrapper<bool>    spaceshipOperator{ "spaceship_operator", true };
        ConfigWrapper<bool>    branchHint{ "branch_hint", false };
        ConfigWrapper<bool>    importStdMacro{ "import_std_macro", false };
        ConfigWrapper<bool>    integrateVma{ "integrate_vma", false };

        ConfigWrapper<Define> structReflect{ "struct_reflect", { "VULKAN_HPP_USE_REFLECT", Define::COND_ENABLED } };

        ConfigWrapper<NDefine> unionConstructors{ "union_constructors", { "VULKAN_HPP_NO_UNION_CONSTRUCTORS", Define::COND_ENABLED } };
        ConfigWrapper<NDefine> unionSetters{ "union_setters", { "VULKAN_HPP_NO_UNION_SETTERS", Define::COND_ENABLED } };

        ConfigWrapper<NDefine> handleConstructors{ "handles_constructors", { "VULKAN_HPP_NO_HANDLES_CONSTRUCTORS", Define::COND_ENABLED } };

        ConfigWrapper<NDefine> handleTemplates{ "handle_templates", { "VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES", Define::ENABLED } };

        ConfigWrapper<std::string> contextClassName{ "context_class_name", { "Context" } };

        ConfigWrapper<int> classMethods{ "class_methods", { 1 } };
        ConfigWrapper<int> cppStd{ "cpp_standard", 11 };

        ConfigGroupRAII raii;

        [[nodiscard]] auto reflect() const {
            return std::tie(cppModules,
                            cppFiles,
                            expApi,
                            cppStd,
                            // internalFunctions,
                            functionsVecAndArray,
                            structConstructors,
                            structSetters,
                            structCompare,
                            spaceshipOperator,
                            branchHint,
                            importStdMacro,
                            integrateVma,
                            structReflect,
                            unionConstructors,
                            unionSetters,
                            handleConstructors,
                            handleTemplates,
                            contextClassName,
                            classMethods,
                            raii);
        }
    };

    struct Config : public ConfigGroup
    {
        ConfigGroupGen       gen{};
        ConfigGroupWhitelist whitelist{};
        ConfigGroupMacro     macro{};

        struct
        {
            ConfigWrapper<bool> methodTags{ "dbg_command_tags", { false } };
        } dbg;

        Config() : ConfigGroup{ "config" } {}

        //        Config(Config&&) = delete;
        //        Config(const Config&) = delete;
        //
        //        Config& operator=(Config&&) = delete;
        //        Config& operator=(const Config&) = delete;

        void save(Generator &, const std::string &filename);

        void load(Generator &, const std::string &filename);

        void reset() {
            macro = {};
            gen   = {};
            dbg   = {};
        }

        [[nodiscard]] auto reflect() const {
            return std::tie(gen, macro);
        }
    };

    class WhitelistBuilder
    {
        std::string text;
        bool        hasDisabledElement = false;

      public:
        template <typename T>
        void add(const T &t) {
            if constexpr (is_reference_wrapper<T>::value) {
                append(t.get());
            } else if constexpr (std::is_pointer<T>::value) {
                append(*t);
            } else {
                append(t);
            }
        }

        void append(const BaseType &t) {
            if (t.isEnabled() || t.isRequired()) {
                auto name = t.name.original;
                text += "            " + name + "\n";
            } else if (t.isSuppored()) {
                hasDisabledElement = true;
                // std::cout << "disabled elem: " << t.name << " " << t.metaTypeString() << std::endl;
            }
        }

        void insertToParent(tinyxml2::XMLElement *parent, const std::string &name, const std::string &comment) {
            if (!hasDisabledElement) {
                //                XMLElement *reg = parent->GetDocument()->NewElement("regex");
                //                reg->SetText(".*");
                //                elem->InsertEndChild(reg);
                return;
            } else {
                if (!text.empty()) {
                    text = "\n" + text + "        ";
                }
                auto *elem = parent->GetDocument()->NewElement(name.c_str());
                elem->SetText(text.c_str());
                if (!comment.empty()) {
                    elem->SetAttribute("comment", comment.c_str());
                }
                parent->InsertEndChild(elem);
            }
        }
    };

    template <typename T>
    void configBuildList(const std::string &name, const std::map<std::string, T> &from, tinyxml2::XMLElement *parent, const std::string &comment = "");

    template <typename T>
    void configBuildList(const std::string &name, const std::vector<T> &from, tinyxml2::XMLElement *parent, const std::string &comment = "");

    struct AbstractWhitelistBinding
    {
        std::string                     name;
        std::vector<std::string>        ordered;
        std::unordered_set<std::string> filter;
        std::vector<std::regex>         regexes;

        virtual ~AbstractWhitelistBinding() {}

        bool add(const std::string &str) {
            auto [it, inserted] = filter.insert(str);
            if (!inserted) {
                return false;
            }
            ordered.push_back(str);
            return true;
        }

        void add(const std::regex &rgx) {
            regexes.emplace_back(rgx);
        }

        virtual void apply() = 0;

        virtual void prepare() = 0;
    };

    template <typename T>
    struct WhitelistBinding : public AbstractWhitelistBinding
    {
        std::vector<T> *dst;

        WhitelistBinding(std::vector<T> *dst, std::string name) : dst(dst) {
            this->name = name;
        }

        void apply() noexcept override;

        void prepare() noexcept override;
    };

}  // namespace vkgen

#endif  // GENERATOR_CONFIG_HPP
