#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include "XMLUtils.hpp"
#include "XMLVariableParser.hpp"
#include "tinyxml2.h"
#include <filesystem>
#include <forward_list>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <set>
#include <string_view>
#include <unordered_set>
#include <vector>

template<typename T>
struct is_reference_wrapper : std::false_type {};

template<typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T> class EnumFlag {
    using Type = typename std::underlying_type<T>::type;
    Type m_flags;

  public:
    EnumFlag() = default;

    EnumFlag(const T &flags) : m_flags(static_cast<Type>(flags)) {}

    bool operator&(const T &rhs) const {
        return (m_flags & static_cast<Type>(rhs)) != 0;
    }

    operator const T() const { return m_flags; }

    EnumFlag<T> &operator|=(const T &b) {
        m_flags |= static_cast<Type>(b);
        return *this;
    }

    EnumFlag<T> &operator&=(const T &b) {
        m_flags &= static_cast<Type>(b);
        return *this;
    }

    const T operator~() const { return static_cast<T>(~m_flags); }
};

template <template <class...> class TContainer, class T, class A, class E>
static bool isInContainter(const TContainer<T, A> &array, const E& entry) {
    return std::find(std::begin(array), std::end(array), entry) !=
           std::end(array);
}

using namespace tinyxml2;

static String convertName(const std::string &name, const std::string &cls) {
    String out{name, false};
    out = std::regex_replace(out, std::regex(cls, std::regex_constants::icase),
                             "");
    return out; // filtered prototype name
}

class Generator {

  public:
    struct Macro {
        std::string define;
        std::string value;
        bool usesDefine;

        Macro(const std::string &define, const std::string &value, bool usesDefine)
            : define(define), value(value), usesDefine(usesDefine)
        {
        }

        std::string get() const { return usesDefine ? define : value; }

        auto operator<=>(Macro const & ) const = default;
    };

    template <typename T>
    struct ConfigWrapper {
        std::string name;
        T data;
        T _default;

        ConfigWrapper(std::string name, T&& data) : name(name), data(data), _default(data)
        {
        }

        operator T&() {
            return data;
        }

        operator const T&() const {
            return data;
        }

        T* operator->() {
            return &data;
        }

        const T* operator->() const {
            return &data;
        }

        void xmlExport(XMLElement *elem) const;

        bool xmlImport(XMLElement *elem) const;

        bool isDifferent() const {
            return data != _default;
        }

        void reset() const {
            const_cast<ConfigWrapper*>(this)->data = _default;
            //data = _default;
        }

    };

    struct ConfigGroup {
        std::string name;
    };

    struct ConfigGroupMacro : public ConfigGroup {
        ConfigGroupMacro() : ConfigGroup{"macro"} {}

        Macro mNamespaceSTD{"", "std", false};
        ConfigWrapper<Macro> mNamespace{"m_namespace", {"VULKAN_HPP_NAMESPACE", "vk20", true}};
        ConfigWrapper<Macro> mNamespaceRAII{"m_namespace_raii", {"VULKAN_HPP_NAMESPACE_RAII", "vk20r", true}};
        ConfigWrapper<Macro> mConstexpr{"m_constexpr", {"VULKAN_HPP_CONSTEXPR", "constexpr", true}};
        ConfigWrapper<Macro> mConstexpr14{"m_constexpr14", {"VULKAN_HPP_CONSTEXPR_14", "constexpr", true}};
        ConfigWrapper<Macro> mInline{"m_inline", {"VULKAN_HPP_INLINE", "inline", true}};
        ConfigWrapper<Macro> mNoexcept{"m_noexcept", {"VULKAN_HPP_NOEXCEPT", "noexcept", true}};
        ConfigWrapper<Macro> mExplicit{"m_explicit", {"VULKAN_HPP_TYPESAFE_EXPLICIT", "explicit", true}};
        ConfigWrapper<Macro> mDispatch{"m_dispatch_assignment", {"VULKAN_HPP_DEFAULT_DISPATCHER_ASSIGNMENT", "", true}};
        ConfigWrapper<Macro> mDispatchType{"m_dispatch_type", {"VULKAN_HPP_DEFAULT_DISPATCHER_TYPE", "DispatchLoaderStatic", true}};

        auto reflect() const {
            return std::tie(
                mNamespace,
                mNamespaceRAII,
                mConstexpr,
                mConstexpr14,
                mInline,
                mNoexcept,
                mExplicit,
                mDispatch,
                mDispatchType
            );
        }
    };

    struct ConfigGroupGen : public ConfigGroup {
        ConfigGroupGen() : ConfigGroup{"gen"} {}

        ConfigWrapper<bool> cppModules{"modules", false};
        bool structNoinit{false};
        ConfigWrapper<bool> vulkanCommands{"vk_commands", {true}};
        ConfigWrapper<bool> vulkanEnums{"vk_enums", {true}};
        ConfigWrapper<bool> vulkanStructs{"vk_structs", {true}};
        ConfigWrapper<bool> vulkanHandles{"vk_handles", {true}};

        ConfigWrapper<bool> vulkanCommandsRAII{"vk_raii_commands", {true}};
        ConfigWrapper<bool> dispatchParam{"dispatch_param", {true}};
        ConfigWrapper<bool> dispatchTemplate{"dispatch_template", {true}};
        ConfigWrapper<bool> dispatchLoaderStatic{"dispatch_loader_static", {true}};
        ConfigWrapper<bool> useStaticCommands{"static_link_commands", {false}}; // move
        ConfigWrapper<bool> allocatorParam{"allocator_param", {false}};
        ConfigWrapper<bool> smartHandles{"smart_handles", {true}};
        ConfigWrapper<bool> exceptions{"exceptions", {true}};
        ConfigWrapper<bool> resultValueType{"use_result_value_type", {false}};
        ConfigWrapper<bool> inlineCommands{"inline_commands", {true}};
        ConfigWrapper<bool> orderCommands{"order_command_pointers", {false}};
        ConfigWrapper<bool> structConstructor{"struct_constructor", {true}};
        ConfigWrapper<bool> structSetters{"struct_setters", {true}};
        ConfigWrapper<bool> structSettersProxy{"struct_setters_proxy", {true}};
        ConfigWrapper<bool> structReflect{"struct_reflect", {true}};

        auto reflect() const {
            return std::tie(
                cppModules,
                vulkanEnums,
                vulkanStructs,
                vulkanHandles,
                vulkanCommands,
                vulkanCommandsRAII,
                dispatchParam,
                dispatchTemplate,
                dispatchLoaderStatic,
                useStaticCommands,
                allocatorParam,
                smartHandles,
                exceptions,
                resultValueType,
                inlineCommands,
                orderCommands,
                structConstructor,
                structSetters,
                structSettersProxy,
                structReflect
            );
        }
    };


    struct Config {                
        ConfigGroupMacro macro {};
        ConfigGroupGen gen {};
        struct {
            ConfigWrapper<bool> methodTags{"dbg_command_tags", {false}};
        } dbg;

        Config() = default;

        auto reflect() const {
            return std::tie(
                macro,
                gen,
                dbg.methodTags
            );
        }

    };    

    enum class PFNReturnCategory {
        OTHER,
        VOID,
        VK_RESULT
    };

    enum class ArraySizeArgument { INVALID, COUNT, SIZE, CONST_COUNT };

    enum class MemberNameCategory {
        UNKNOWN,
        GET,
        ALLOCATE,
        ACQUIRE,
        CREATE,
        ENUMERATE,
        WRITE,
        DESTROY,
        FREE
    };

    enum class HandleCreationCategory { NONE, ALLOCATE, CREATE };

    enum class CommandFlags : uint8_t {
        NONE,
        ALIAS = 1,
        INDIRECT = 2,
        RAII_ONLY = 4
    };

    struct HandleData;


    struct ExtensionData;

    struct BaseType {
        String name = {""};
        ExtensionData *ext = {};
        std::unordered_set<BaseType *> dependencies;
        std::unordered_set<BaseType *> subscribers;
        bool forceRequired = {};

        std::string_view getProtect() const {
            if (ext) {
                return ext->protect;
            }
            return "";
        }

        void setUnsuppored() {
            supported = false;
        }

        bool isEnabled() const {
            return enabled && supported;
        }

        bool isSuppored() const {
            return supported;
        }

        bool isRequired() const {
            return !subscribers.empty() || forceRequired;
        }

        bool canGenerate() const {
            return supported && (enabled || isRequired());
        }

        void setEnabled(bool value) {
            if (enabled == value || !supported) {
                return;
            }
            enabled = value;
            if (enabled) {
                // std::cout << "> enable: " << name << std::endl;
                for (auto &d : dependencies) {
                    if (!subscribers.contains(d)) {
                        d->subscribe(this);
                    }
                }
            }
            else {
                // std::cout << "> disable: " << name << std::endl;
                for (auto &d : dependencies) {
                    if (!subscribers.contains(d)) {
                        d->unsubscribe(this);
                    }
                }
            }
        }

        void subscribe(BaseType *s) {
            if (!subscribers.contains(s)) {
                bool empty = subscribers.empty();
                subscribers.emplace(s);
                // std::cout << "  subscribed to: " << name << std::endl;
                if (empty) {
                    setEnabled(true);
                }
            }
        }

        void unsubscribe(BaseType *s) {
            auto it = subscribers.find(s);
            if (it != subscribers.end()) {
                subscribers.erase(it);
                // std::cout << "  unsubscribed to: " << name << std::endl;
                if (subscribers.empty()) {
                    setEnabled(false);
                }
            }
        }

    protected:
        bool enabled = false;
        bool supported = true;
    };

    struct PlatformData : public BaseType {
        // std::string name;
        std::string_view protect;
        //bool enabled = {};

        PlatformData(const std::string &name, std::string_view protect, bool enabled) :
            protect(protect)
        {
            BaseType::name.reset(name);
            BaseType::enabled = enabled;
        }

//        void setEnabled(bool value) {
//            enabled = value;
//        }

//        bool isEnabled() const {
//            return enabled;
//        }

//        bool isRequired() const {
//            return enabled;
//        }
    };

    using GenFunction = std::function<void(XMLNode *)>;
    using OrderPair = std::pair<std::string, GenFunction>;

    using Variables = std::vector<std::shared_ptr<VariableData>>;
    void bindVars(Variables &vars) const;

    struct CommandData : public BaseType {
        std::string type; // return type
        Variables params; // list of arguments
        std::vector<std::string> successCodes;
        MemberNameCategory nameCat;
        PFNReturnCategory pfnReturn;
        EnumFlag<CommandFlags> flags{};

        void setFlagBit(CommandFlags bit, bool enabled) {
            if (enabled) {
                flags |= bit;
            } else {
                flags &= ~EnumFlag<CommandFlags>{bit};
                ;
            }
        }

        bool isAlias() const {
            bool r = flags & CommandFlags::ALIAS;
            return r;
        }

        bool getsObject() const {
            switch (nameCat) {
            case MemberNameCategory::ACQUIRE:
            case MemberNameCategory::GET:
                return true;
            default:
                return false;
            }
        }

        bool createsObject() const {
            switch (nameCat) {
            case MemberNameCategory::ALLOCATE:
            case MemberNameCategory::CREATE:
                return true;
            default:
                return false;
            }
        }

        bool destroysObject() const {
            switch (nameCat) {
            case MemberNameCategory::DESTROY:
            case MemberNameCategory::FREE:
                return true;
            default:
                return false;
            }
        }

        bool isIndirectCandidate(const std::string_view &type) const {
            try {
                if (getsObject() || createsObject()) {
                    const auto &var = getLastPointerVar();
                    return var->original.type() != type;
                } else if (destroysObject()) {
                    const auto &var = getLastHandleVar();
                    return var->original.type() != type;
                }
            } catch (std::runtime_error) {
            }
            return true;
        }

        void setName(const Generator &gen, const std::string &name) {
            this->name.convert(name);
            pfnReturn = gen.evaluatePFNReturn(type);
            gen.evalCommand(*this);
        }

        bool containsPointerVariable() {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (it->get()->original.isPointer()) {
                    return true;
                }
            }
            return false;
        }

        std::shared_ptr<VariableData> getLastVar() {
            return *std::prev(params.end());
        }

        std::shared_ptr<VariableData> getLastVisibleVar() {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (!it->get()->getIgnoreFlag()) {
                    return *it;
                }
            }
            throw std::runtime_error("can't get param (last visible)");
        }

        std::shared_ptr<VariableData> getLastPointerVar() const {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (it->get()->original.isPointer()) {
                    return *it;
                }
            }
            throw std::runtime_error("can't get param (last pointer)");
        }

        std::shared_ptr<VariableData> getLastHandleVar() const {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (it->get()->isHandle()) {
                    return *it;
                }
            }
            throw std::runtime_error("can't get param (last handle)");
        }
    };

    // holds information about class member (function)
    struct ClassCommandData {

        const Generator *gen = {};
        const CommandData *src = {};
        HandleData *cls = {};
        String name = {""};
        bool raiiOnly = {};

        ClassCommandData(const Generator *gen, HandleData *cls,
                         const CommandData &o)
            : gen(gen), cls(cls), src(&o),
              name(convertName(o.name.original, cls->name), false)
        {
            name.original = o.name.original;
        }

        ClassCommandData &operator=(ClassCommandData &o) noexcept = default;

        bool valid() const { return !name.empty(); }
    };

    struct EnumValue : public BaseType {
        bool isAlias = {};
        bool supported = {};

        EnumValue(const String &name, bool isAlias = false, bool enabled = true) : isAlias(isAlias) {
            this->name = name;
            this->enabled = true;
        }
    };

    struct EnumData : public BaseType {
        std::vector<String> aliases;
        std::vector<EnumValue> members;
        // std::string flagbits;
        bool isBitmask = {};

        EnumData(std::string name) { this->name = String(name, true); }

        bool containsValue(const std::string &value) const;
    };    
\
    template<typename T>
    struct Container {
        static_assert(std::is_base_of<BaseType, T>::value, "T must derive from BaseType");

        std::vector<T> items;
        std::unordered_map<std::string, T&> map;

        bool contains(const std::string &name) const {
            for (const auto &i : items) {
                if (i.name.original == name) {
                    return true;
                }
            }
            return false;
        }

//        typename std::vector<T>::iterator find(const std::string &name, bool dbg = false) {
//            return find(std::string_view{name}, dbg);
//        }

        typename std::vector<T>::iterator find(const std::string_view &name, bool dbg = false) {
            typename std::vector<T>::iterator it;
            for (it = items.begin(); it != items.end(); ++it) {
                if (it->name.original == name) {
                    break;
                }
            }
            if (dbg) {
                std::cout << "#error: " << name << " not found" << std::endl;
                for (it = items.begin(); it != items.end(); ++it) {
                    std::cout << "  " << it->name.original << " == " << name << std::endl;
                }
            }
            return it;
        }

        typename std::vector<T>::iterator end() {
            return items.end();
        }

        typename std::vector<T>::iterator begin() {
            return items.begin();
        }

        typename std::vector<T>::const_iterator end() const {
            return items.cend();
        }

        typename std::vector<T>::const_iterator begin() const {
            return items.cbegin();
        }

        void add(const T& item, bool dbg = false) {
            items.push_back(item);
            T& ref = *items.rbegin();
            if (dbg) {
                std::cout << "  added: " << item.name << "(" << item.name.original << ") ptr: " << &ref << std::endl;
                // std::cout << "> " << ref.name.original << std::endl;
            }
            // map.emplace(item)
            // return ref;
        }

        void clear() {
            items.clear();
        }

        size_t size() const {
            return items.size();
        }

    };


    struct StructData;

    struct ExtensionData : public BaseType {
        // std::string name;
        std::string protect;
        PlatformData *platform;
        std::vector<CommandData *> commands;
        std::vector<BaseType *> types;
//        bool supported = true;
//        bool enabled = {};

        ExtensionData(const std::string &name, PlatformData *platform, bool supported, bool enabled) :
            platform(platform)
        {
            BaseType::name.reset(name);
            BaseType::enabled = enabled;
            BaseType::supported = supported;
            if (platform) {
                protect = platform->protect;
            }
        }

//        void setEnabled(bool value) {
//            enabled = value;
//        }

//        bool isEnabled() const {
//            return enabled;
//        }

//        bool isRequired() const {
//            return enabled;
//        }

    };

    struct GenOutputClass {
        std::string sPublic;
        std::string sPrivate;
        std::string sProtected;
        std::string inherits;
    };

    class UnorderedOutput {
        const Generator &g;
        std::map<std::string, std::string> segments;

      public:
        UnorderedOutput(const Generator &gen) : g(gen) {}

        std::string get() const {
            std::string out;
            for (const auto &k : segments) {
                // std::cout << ">> uo[" << k.first << "]" << std::endl;
                out += g.genWithProtect(k.second, k.first);
            }
            return out;
        }

        void clear() {
            segments.clear();
        }

        void add(const BaseType &type, std::function<void(std::string &)> function, bool bypass = false) {
            auto [code, protect] = g.genCodeAndProtect(type, function, bypass);
            segments[protect] += code;
        }

        void addString(const std::string &code) {
            segments[""] += code;
        }
    };

    Config cfg;
    bool verbose;
    XMLDocument doc;
    XMLElement *root;
    std::string registryPath;

    std::string outputFilePath;
    UnorderedOutput outputFuncs;
    UnorderedOutput outputFuncsRAII;

    std::string headerVersion;

    bool dispatchLoaderBaseGenerated;

    std::vector<const EnumValue *> errorClasses;

    using Commands = Container<CommandData>;
    using Platforms = Container<PlatformData>;
    using Extensions = Container<ExtensionData>;
    using Tags = std::unordered_set<std::string>;

    Platforms platforms; // maps platform name to protect (#if defined PROTECT)
    Extensions extensions;
    Tags tags; // list of tags from <tags>
    std::unordered_map<Namespace, Macro *> namespaces;

    Commands commands;
    std::vector<std::reference_wrapper<CommandData>> staticCommands;
    std::vector<std::reference_wrapper<CommandData>> orderedCommands;

    struct StructData : BaseType {
        enum VkStructType { VK_STRUCT, VK_UNION } type;
        // XMLNode *node;
        std::string structTypeValue;
        std::vector<String> aliases;
        Variables members;
        bool returnedonly = false;

        std::string getType() const {
            return type == StructData::VK_STRUCT ? "struct" : "union";
        }
    };

    Container<StructData> structs;

    bool isStructOrUnion(const std::string &name) const;

    Container<EnumData> enums;

    template <class... Args>
    std::string format(const std::string &format, const Args... args) const;

    bool defaultWhitelistOption = true;

    void parsePlatforms(XMLNode *node);

    void parseFeature(XMLNode *node);

    void parseExtensions(XMLNode *node);

    void parseTags(XMLNode *node);

    std::string genWithProtect(const std::string &code, const std::string &protect) const;

    std::pair<std::string, std::string> genCodeAndProtect(const BaseType &type,
                                                          std::function<void(std::string &)> function, bool bypass = false) const;

    std::string genOptional(const BaseType &type,
                            std::function<void(std::string &)> function, bool bypass = false) const;

    std::string strRemoveTag(std::string &str) const;

    std::string strWithoutTag(const std::string &str) const;

    std::pair<std::string, std::string> snakeToCamelPair(std::string str) const;

    std::string snakeToCamel(const std::string &str) const;

    std::string enumConvertCamel(const std::string &enumName, std::string value,
                                 bool isBitmask = false) const;

    std::string genNamespaceMacro(const Macro &m);

    std::string generateHeader();

    struct GenOutput {
        std::string enums;
        std::string handles;
        std::string structs;
        std::string raii;
    };

    void generateFiles(std::filesystem::path path);

    std::string generateMainFile(GenOutput&);

    std::string generateModuleImpl();

    Variables parseStructMembers(XMLElement *node, std::string &structType,
                                 std::string &structTypeValue);

    void parseEnumExtend(XMLElement &node, ExtensionData *ext);

    std::string generateEnum(const EnumData &data);

    std::string generateEnums();

    std::string genFlagTraits(const EnumData &data, std::string name);

    std::string generateDispatch();

    std::string generateErrorClasses();

    std::string generateDispatchLoaderBase();

    std::string generateDispatchLoaderStatic();

    bool useDispatchLoader() const {
        const auto &cfg = getConfig();
        return cfg.gen.dispatchLoaderStatic && !cfg.gen.useStaticCommands;
    }

    std::string getDispatchArgument(bool assignment) const {
        if (!cfg.gen.dispatchParam) {
            return "";
        }
        std::string out =
            format("::{NAMESPACE}::DispatchLoaderStatic const &d");
        if (assignment) {
            out += " " + cfg.macro.mDispatch->get();
        }
        return out;
    }

    std::string getDispatchType() const {
        if (cfg.gen.dispatchTemplate) {
            return "Dispatch";
        }
        if (cfg.macro.mDispatchType->usesDefine) {
            return cfg.macro.mDispatchType->define;
        }
        return format(cfg.macro.mDispatchType->value);
    }

    Argument getDispatchArgument() const {
        if (!cfg.gen.dispatchParam) {
            return Argument("", "");
        }
        std::string assignment = cfg.macro.mDispatch->get();
        if (!assignment.empty()) {
            assignment = " " + assignment;
        }
        auto type = format("::{NAMESPACE}::DispatchLoaderStatic const &");
        return Argument(type, "d", assignment);
    }

    std::string getDispatchCall(const std::string &var = "d.") const {
        if (cfg.gen.dispatchParam) {
            return var;
        }
        return "::";
    }

    struct MemberContext : CommandData {
        using Var = const std::shared_ptr<VariableData>;

        const Generator &gen;
        HandleData *cls;
        Namespace ns;
        std::string pfnSourceOverride;
        std::string pfnNameOverride;        
        bool raiiOnly = {};
        bool useThis = {};
        bool isStatic = {};
        bool inUnique = {};
        bool constructor = {};
        bool generateInline = {};
        bool disableSubstitution = {};
        bool disableDispatch = {};

        MemberContext(const ClassCommandData &m, Namespace ns,
                      bool constructor = false)
            : gen(*m.gen), ns(ns), constructor(constructor),
              CommandData(*m.src)
        {
            name = m.name;
            cls = m.cls;
            raiiOnly = m.raiiOnly;

            initParams(m.src->params);
        }

        MemberContext(Generator &gen, HandleData *cls, const StructData &s, Namespace ns,
                      bool constructor = false)
            : gen(gen), ns(ns), constructor(constructor)
        {
            *reinterpret_cast<BaseType*>(this) = s;            
            this->cls = cls;
            // name = s.name;
            initParams(s.members);
        }

        MemberContext(const MemberContext &o) : gen(o.gen), CommandData(o) {
            cls = o.cls;
            ns = o.ns;
            pfnSourceOverride = o.pfnSourceOverride;
            pfnNameOverride = o.pfnNameOverride;
            raiiOnly = o.raiiOnly;
            constructor = o.constructor;
            useThis = o.useThis;
            isStatic = o.isStatic;
            inUnique = o.inUnique;
            generateInline = o.generateInline;
            disableSubstitution = o.disableSubstitution;
            disableDispatch = o.disableDispatch;

            const auto &p = o.params;
            params.clear();
            params.reserve(p.size());
            std::transform(p.begin(), p.end(), std::back_inserter(params),
                           [](const std::shared_ptr<VariableData> &var) {
                               return std::make_shared<VariableData>(
                                   *var.get());
                           });
            gen.bindVars(params);
        }

        bool isRaiiOnly() const { return raiiOnly; }

        bool isIndirect() const { return cls->isSubclass; }

        std::string createProtoArguments(bool useOriginal = false, bool declaration = false) const {
            return createArguments(
                filterProto,
                [&](Var &v) {
                    if (useOriginal) {
                        return v->originalToString();
                    }
                    if (declaration) {
                        return v->toStringWithAssignment();
                    }
                    return v->toString();
                },
                true);
        }

        std::string createPFNArguments(bool useOriginal = false) const {
            return createArguments(
                filterPFN, [&](Var &v) { return v->toArgument(useOriginal); });
        }

        std::string createPassArguments(bool hasAllocVar) const {
            return createArguments(filterPass,
                                   [&](Var &v) { return v->identifier(); },
                                   false);
        }

        Variables getFilteredProtoVars() const {
            Variables out;
            for (const auto &p : params) {
                if (filterProto(p, p->original.type() == cls->name.original)) {
                    out.push_back(p);
                }
            }
            return out;
        }

      private:
        void initParams(const Variables &p) {
            params.clear();
            params.reserve(p.size());
            std::transform(p.begin(), p.end(), std::back_inserter(params),
                           [](const std::shared_ptr<VariableData> &var) {
                               return std::make_shared<VariableData>(
                                   *var.get());
                           });

            gen.bindVars(params);
        }

        static bool filterProto(Var &v, bool same) {
            if (v->getIgnoreFlag()) {
                return false;
            }
            return v->hasLengthVar() || !same;
        }

        static bool filterPass(Var &v, bool same) {
            return !v->getIgnoreFlag();
        }

        static bool filterPFN(Var &v, bool same) { return !v->getIgnorePFN(); }

        std::string createArguments(std::function<bool(Var &, bool)> filter,
                                    std::function<std::string(Var &)> function,
                                    bool proto = false) const {
            std::string out;
            for (const auto &p : params) {
                bool sameType = p->original.type() == cls->name.original && !p->original.type().empty();

                if (ns == Namespace::RAII && cls->isSubclass && !constructor &&
                    p->original.type() == cls->superclass.original) {
                    continue;
                }

                if (!filter(p, sameType)) {
                    continue;
                }
                if (!disableSubstitution && !proto && p->type() == "AllocationCallbacks" &&
                    !gen.getConfig().gen.allocatorParam) {
                    out += "nullptr";
                } else if (!p->hasLengthVar() && sameType) {
                    if (!useThis && p->original.isPointer()) {
                        out += "&";
                    } else if (useThis) {
                        out += "*";
                    }
                    out += useThis ? "this" : cls->vkhandle;
                } else {
                    bool match = false;
                    if (inUnique && !proto && !disableSubstitution && !p->hasLengthVar()) {
                        for (const auto &v : cls->uniqueVars) {
                            if (p->type() == v.type()) {
                                out += matchTypePointers(v.suffix(), p->original.suffix());
                                out += v.identifier();
                                match = true;
                                break;
                            }
                        }
                    }
                    if (!match) {
                        out += function(p);
                    }
                }
                out += ", ";
            }
            strStripSuffix(out, ", ");
            return out;
        }
    };

    struct HandleData : public BaseType {

        String superclass;
        std::string vkhandle;
        std::string ownerhandle;
        std::string alias;
        HandleData *parent;
        HandleCreationCategory creationCat;

        std::optional<ClassCommandData> getAddrCmd;
        std::vector<ClassCommandData> members;
        std::vector<ClassCommandData*> filteredMembers;
        std::vector<ClassCommandData> ctorCmds;
        std::vector<CommandData *> dtorCmds;
        std::vector<ClassCommandData> vectorCmds;

        std::vector<MemberContext> generated;

        std::vector<VariableData> uniqueVars;
        std::vector<VariableData> raiiVars;

        int effectiveMembers;
        bool isSubclass;
        bool vectorVariant;

        HandleData(const std::string &name, bool isSubclass = false)
            : superclass(""), isSubclass(isSubclass) {
            this->name = String(name, true);
            vkhandle = "m_" + strFirstLower(this->name);
            parent = nullptr;
            effectiveMembers = 0;
            creationCat = HandleCreationCategory::NONE;
        }

        void clear() {            
            generated.clear();
            uniqueVars.clear();
            raiiVars.clear();
        }

        void init(const Generator &gen,
                       const std::string &loaderClassName);

        void addCommand(const Generator &gen, const CommandData &cmd,
                        bool raiiOnly = false);

        bool hasPFNs() const { return effectiveMembers > 0 && !isSubclass; }

        bool uniqueVariant() const {
            return creationCat != HandleCreationCategory::NONE;
        }
    };

    using Handles = std::map<std::string, HandleData>;
    Handles handles;
    HandleData loader;

    bool isHandle(const std::string &name) const {
        return handles.find(name) != handles.end() || name == loader.name.original;
    }

    HandleData &findHandle(const std::string &name) {
        if (name == loader.name.original) {
            return loader;
        }
        const auto &handle = handles.find(name);
        if (handle == handles.end()) {
            throw std::runtime_error("Handle not found: " + std::string(name));
        }
        return handle->second;
    }

    const HandleData &findHandle(const std::string &name) const {
        if (name == loader.name.original) {
            return loader;
        }
        const auto &handle = handles.find(name);
        if (handle == handles.end()) {
            throw std::runtime_error("Handle not found: " + std::string(name));
        }
        return handle->second;
    }    

    BaseType *findType(const std::string &name) {
        // TODO refactor
        auto s = structs.find(name);
        if (s != structs.end()) {
            return s.base();
        }
//        for (auto &c : structs) {
//            if (c.second.name.original == name) {
//                return &c.second;
//            }
//        }
        auto e = enums.find(name);
        if (e != enums.end()) {
            return e.base();
        }
//        for (auto &c : enumMap) {
//            if (c.second->name.original == name) {
//                return c.second;
//            }
//        }
        for (auto &c : handles) {
            if (c.second.name.original == name) {
                return &c.second;
            }
        }
        return nullptr;
    }

    void parseTypes(XMLNode *node);

    void parseEnums(XMLNode *node);

    std::string generateStructDecl(const StructData &d);

    std::string generateClassDecl(const HandleData &data, bool allowUnique);

    std::string generateClassString(const std::string &className,
                                    const GenOutputClass &from);

    std::string generateHandles();

    std::string generateStructs();

    std::string generateStruct(const StructData &data);

    std::string generateRAII();

    CommandData parseClassMember(XMLElement *command,
                                 const std::string &className) {
        CommandData m;

        // iterate contents of <command>
        std::string dbg;
        std::string name;
        for (XMLElement &child : Elements(command)) {
            // <proto> section
            dbg += std::string(child.Value()) + "\n";
            if (std::string_view(child.Value()) == "proto") {
                // get <name> field in proto
                XMLElement *nameElement = child.FirstChildElement("name");
                if (nameElement) {
                    name = nameElement->GetText();
                }
                // get <type> field in proto
                XMLElement *typeElement = child.FirstChildElement("type");
                if (typeElement) {
                    m.type = typeElement->GetText();
                }
            }
            // <param> section
            else if (std::string_view(child.Value()) == "param") {
                // parse inside of param
                const std::shared_ptr<XMLVariableParser> &parser =
                    std::make_shared<XMLVariableParser>(&child, *this);
                // add proto data to list
                m.params.push_back(parser);
            }
        }
        if (name.empty()) {
            std::cerr << "Command has no name" << std::endl;
        }

        const char *successcodes = command->Attribute("successcodes");
        if (successcodes) {
            for (const auto &str : split(std::string(successcodes), ",")) {
                m.successCodes.push_back(str);
            }
        }

        bindVars(m.params);
        m.setName(*this, name);
        return m;
    }

    void evalCommand(CommandData &ctx) const;

    static MemberNameCategory evalNameCategory(const std::string &name);

    static bool isTypePointer(const VariableData &m) {
        return strContains(m.suffix(), "*");
    }

    ArraySizeArgument evalArraySizeArgument(const VariableData &m) {
        if (m.identifier().ends_with("Count")) {
            return isTypePointer(m) ? ArraySizeArgument::COUNT
                                    : ArraySizeArgument::CONST_COUNT;
        }
        if (m.identifier().ends_with("Size")) {
            return ArraySizeArgument::SIZE;
        }
        return ArraySizeArgument::INVALID;
    }

    class MemberResolverBase {

      protected:
        MemberContext ctx;
        VariableData resultVar;
        std::string returnType;
        std::string returnValue;
        std::string initializer;
        bool specifierInline;
        bool specifierExplicit;
        bool specifierConst;
        bool specifierConstexpr;
        bool specifierConstexpr14;
        bool disableCheck;        
        std::shared_ptr<VariableData> allocatorVar;
        std::vector<std::string> usedTemplates;

        std::string declareReturnVar(const std::string &assignment = "") {
            if (!resultVar.isInvalid()) {
                return "";
            }
            resultVar.setSpecialType(VariableData::TYPE_DEFAULT);
            resultVar.setIdentifier("result");
            resultVar.setFullType("", "Result", "");

            std::string out = resultVar.toString();
            if (!assignment.empty()) {
                out += " = " + assignment;
            }
            return out += ";\n";
        }

        virtual std::string generateMemberBody() = 0;

        std::string castTo(const std::string &type,
                           const std::string &src) const {
            if (type != ctx.type) {
                return "static_cast<" + type + ">(" + src + ")";
            }
            return src;
        }

        bool useDispatchLoader() const {
            return ctx.ns == Namespace::VK && ctx.gen.useDispatchLoader();
        }

        std::string generatePFNcall(bool immediateReturn = false) {
            std::string call = ctx.pfnSourceOverride;
            if (call.empty()) {
                if (ctx.ns == Namespace::RAII) {
                    call += "m_";
                } else {
                    call += ctx.gen.getDispatchCall();
                }
            }
            if (ctx.pfnNameOverride.empty()) {
                call += ctx.name.original;
            } else {
                call += ctx.pfnNameOverride;
            }
            call += "(" + ctx.createPFNArguments() + ")";

            switch (ctx.pfnReturn) {
            case PFNReturnCategory::VK_RESULT:
                call = castTo("Result", call);
                if (!immediateReturn) {
                    return assignToResult(call);
                }
                break;
            case PFNReturnCategory::OTHER:
                call = castTo(returnType, call);
                break;
            case PFNReturnCategory::VOID:
                return call + ";";
            default:
                break;
            }
            if (immediateReturn) {
                call = "return " + call;
            }
            return call + ";";
        }

        std::string assignToResult(const std::string &assignment) {
            if (resultVar.isInvalid()) {
                return declareReturnVar(assignment);
            } else {
                return resultVar.identifier() + " = " + assignment + ";";
            }
        }

        std::string generateReturnValue(const std::string &identifier) {
            if (resultVar.isInvalid() || !usesResultValueType()) {
                return identifier;
            }

            std::string out = "createResultValueType";
            //            if (returnType != "void" && returnType != "Result") {
            //                out += "<" + returnType + ">";
            //            }
            out += "(";
            if (resultVar.identifier() != identifier) {
                out += resultVar.identifier() + ", ";
            }
            out += identifier;
            out += ")";
            return out;
        }

        std::string generateCheck() {
            if (ctx.pfnReturn != PFNReturnCategory::VK_RESULT ||
                resultVar.isInvalid()) {
                return "";
            }
            const auto &macros = ctx.gen.getConfig().macro;
            const auto &ns = ctx.ns == Namespace::VK ? macros.mNamespace
                                                     : macros.mNamespaceRAII;
            std::string message;
            if (ns->usesDefine) {
                message = ns->define + "_STRING \"";
            } else {
                message = "\"" + ns->value;
            }
            if (!ctx.cls->name.empty()) {
                message += "::" + ctx.cls->name;
            }
            message += "::" + ctx.name + "\"";

            std::string codes;
            if (ctx.successCodes.size() > 1) {
                codes += ",\n                { ";
                for (const auto &c : ctx.successCodes) {
                    codes +=
                        "Result::" + ctx.gen.enumConvertCamel("Result", c) +
                        ",\n                  ";
                }
                strStripSuffix(codes, ",\n                  ");
                codes += "}";
            }

            std::string output =
                ctx.gen.format(R"(
    resultCheck({0},
                {1}{2});
)",
                               resultVar.identifier(), message, codes);

            return output;
        }

        bool usesResultValueType() const {
            const auto &cfg = ctx.gen.getConfig();
            if (!cfg.gen.resultValueType) {
                return false;
            }
            return !returnType.empty() && returnType != "Result" &&
                   ctx.pfnReturn == PFNReturnCategory::VK_RESULT;
        }

        std::string generateReturnType() {
            if (usesResultValueType()) {
                return "ResultValueType<" + returnType + ">::type";
            }
            return returnType;
        }

        std::string generateNodiscard() {
            if (!returnType.empty() && returnType != "void") {
                return "VULKAN_HPP_NODISCARD ";
            }
            return "";
        }

        std::string getSpecifiers(bool decl) {
            std::string output;
            const auto &cfg = ctx.gen.getConfig();
            if (cfg.gen.inlineCommands && specifierInline && !decl) {
                output += cfg.macro.mInline->get() + " ";
            }
            if (specifierExplicit && decl) {
                output += cfg.macro.mExplicit->get() + " ";
            }
            if (specifierConstexpr) {
                output += cfg.macro.mConstexpr->get() + " ";
            }
            else if (specifierConstexpr14) {
                output += cfg.macro.mConstexpr14->get() + " ";
            }
            return output;
        }

        std::string getProto(const std::string &indent, bool declaration) {
            std::string tag = getDbgtag();
            std::string output;
            if (!tag.empty()) {
                output += indent + tag;
            }

            std::string temp;
            for (const auto &p : ctx.params) {
                const std::string &str = p->getTemplate();
                if (!str.empty()) {
                    temp += "typename " + str;
                    if (declaration && str == "Dispatch") {
                        temp += " = VULKAN_HPP_DEFAULT_DISPATCHER_TYPE";
                    }
                    temp += ", ";
                }
            }
            strStripSuffix(temp, ", ");
            if (!temp.empty()) {
                output += indent + "template <" + temp + ">\n";
            }

            std::string spec = getSpecifiers(declaration);
            std::string ret = generateReturnType();
            output += indent;
            if (!declaration) {
                output += generateNodiscard();
            }
            if (!spec.empty()) {
                output += spec;
            }
            if (!ret.empty()) {
                output += ret + " ";
            }
            if (!declaration && !ctx.isStatic) {
                output += ctx.cls->name + "::";
            }  

            output += ctx.name + "(" + createProtoArguments(declaration) + ")";
            if (ctx.constructor && !initializer.empty()) {
                output += initializer;
            }
            if (specifierConst && !ctx.isStatic && !ctx.constructor) {
                output += " const";
            }
            return output;
        }

        std::string getDbgtag();

        std::string createProtoArguments(bool declaration = false) {
            std::string args = ctx.createProtoArguments(false, declaration);
            return args;
        }

        std::string createPassArguments() {
//            bool allocVar = true;
//            if (allocatorVar && allocatorVar->getIgnoreFlag()) {
//                allocVar = false;
//            }
            std::string args = ctx.createPassArguments(true);
            return args;
        }

        std::string generateArrayCode(const MemberContext::Var &var,
                                      bool useOriginal = false) {
            disableCheck = true;
            if (useOriginal) {
                for (const auto &p : ctx.params) {
                    if (p->isArray() ||
                        p->original.type() != ctx.cls->name.original) {
                        p->original.set(1, p->get(1));
                    }
                }
            }

            bool convert = var->getNamespace() == Namespace::RAII;
            if (convert) {
                var->setNamespace(Namespace::VK);
            }

            std::string output;

            const auto &lenVar = var->getLengthVar();
            const std::string &id = var->identifier();
            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                output += "    " + declareReturnVar();
            }

            std::string size = lenVar->identifier();
            std::string call = generatePFNcall();
            if (lenVar->original.isPointer() && !lenVar->original.isConst()) {
                output += "    " + var->declaration() + ";\n";
                output += "    " + lenVar->declaration() + ";\n";

                var->setAltPFN("nullptr");
                std::string callNullptr = generatePFNcall();

                if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                    output += ctx.gen.format(R"(
    do {
      {0}
      if (result == Result::eSuccess && {2}) {
        {3}.resize({2});
        {1}
        //VULKAN_HPP_ASSERT({2} <= {3}.size());
      }
    } while (result == Result::eIncomplete);
)",
                                             callNullptr, call, size, id);
                } else {
                    output += ctx.gen.format(R"(
    {0}
    {3}.resize({2});
    {1}
    //VULKAN_HPP_ASSERT({2} <= {3}.size());
)",
                                             callNullptr, call, size, id);
                }
                output += generateCheck();
                output += ctx.gen.format(R"(
    if ({0} < {1}.size()) {
      {1}.resize({0});
    }
)",
                                         size, id);
            } else {
                if (lenVar->getIgnoreFlag()) {
                    // size = lenVar->getArrayVar()->identifier() + ".size()";
                    size = id + ".size()";
                } else if (var->isLenAttribIndirect()) {
                    if (lenVar->isPointer()) {
                        size += "->";
                    } else {
                        size += ".";
                    }
                    size += var->getLenAttribRhs();
                }
                output += "    " + var->declaration() + "(" + size + ");\n";
                output += "    " + call + "\n";
                output += generateCheck();
            }
            if (!returnType.empty()) {
                returnValue = generateReturnValue(id);
            } else {
                std::string parent = strFirstLower(ctx.cls->superclass);
                std::string iter = id;
                strStripSuffix(iter, "s");
                output += ctx.gen.format(R"(
    this->reserve({0});
    for (auto const &{1} : {2}) {
      this->emplace_back({3}, {1});
    }
)",
                            size, iter, id, parent);
            }
            return output;
        }

        void transformToProxy(const std::shared_ptr<VariableData> &var) {
                if (!var->hasLengthVar()) {
                    return;
                }

                const auto &sizeVar = var->getLengthVar();

//                if (!sizeVar->hasArrayVar()) {
//                    sizeVar->bindArrayVar(var);
//                }

                if (!var->isLenAttribIndirect()) {
                    sizeVar->setIgnoreFlag(true);
                }

                if (var->original.type() == "void" && !var->original.isConstSuffix()) {
                    bool isPointer = sizeVar->original.isPointer();
                    if (isPointer) {
                        var->setFullType("", "uint8_t", "");
                    } else {
                        std::string templ = "DataType";
                        if (isInContainter(usedTemplates, templ)) {
                            std::cerr << "Warning: " << "same templates used in " << ctx.name << std::endl;
                        }
                        usedTemplates.push_back(templ);
                        var->setFullType("", templ, "");
                        var->setTemplate(templ);
                        sizeVar->setIgnoreFlag(false);
                    }
                }

                var->convertToArrayProxy();
            };


        void transformToProxyNoTemporaries(const std::shared_ptr<VariableData> &var) {
            if (!var->hasLengthVar()) {
                return;
            }
            transformToProxy(var);
            var->setSpecialType(VariableData::TYPE_ARRAY_PROXY_NO_TEMPORARIES);
        }

      public:
        std::string dbgtag;

        MemberResolverBase(MemberContext &refCtx)
            : ctx(refCtx), resultVar(refCtx.gen, VariableData::TYPE_INVALID)
        {
            returnType = strStripVk(std::string(ctx.type));
            dbgtag = "default";
            specifierInline = true;
            specifierExplicit = false;
            specifierConst = true;            
            specifierConstexpr = false;
            specifierConstexpr14 = false;
            disableCheck = false;            

            const auto &cfg = ctx.gen.getConfig();

            if (cfg.gen.dispatchParam && ctx.ns == Namespace::VK && !ctx.disableDispatch) {
                std::string type = ctx.gen.getDispatchType();
                VariableData var(ctx.gen);
                var.setFullType("", type, " const &");
                var.setIdentifier("d");
                var.setIgnorePFN(true);
                if (cfg.gen.dispatchTemplate) {
                    var.setTemplate(type);
                }
                std::string assignment = cfg.macro.mDispatch->get();
                if (!assignment.empty()) {
                    var.setAssignment(" " + assignment);
                }
                ctx.params.push_back(std::make_shared<VariableData>(var));
            }
        }

        virtual ~MemberResolverBase() {}

        virtual void generate(std::string &decl, UnorderedOutput &def);

        std::string generateDeclaration();

        std::string generateDefinition(bool genInline, bool bypass = false);

        bool compareSignature(const MemberResolverBase &o) const {
            const auto removeWhitespace = [](std::string str) {
                return std::regex_replace(str, std::regex("\\s+"), "");
            };

            const auto getType = [&](const MemberContext::Var &var) {
                std::string type = removeWhitespace(var->type());
                std::string suf = removeWhitespace(var->suffix());
                return type + " " + suf;
            };

            Variables lhs = ctx.getFilteredProtoVars();
            Variables rhs = o.ctx.getFilteredProtoVars();
            if (lhs.size() == rhs.size()) {
                for (size_t i = 0; i < lhs.size(); i++) {
                    std::string l = getType(lhs[i]);
                    std::string r = getType(rhs[i]);
                    if (l != r) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }
    };

    class MemberResolver : public MemberResolverBase {        

      private:
        void transformMemberArguments() {
            auto &params = ctx.params;

//            const auto transformToProxy =
//                [&](const std::shared_ptr<VariableData> &var) {
//                    if (var->hasLengthVar()) {

//                        const auto &sizeVar = var->getLengthVar();

//                        if (!sizeVar->hasArrayVar()) {
//                            sizeVar->bindArrayVar(var);
//                        }

//                        bool isPointer = sizeVar->original.isPointer();
//                        if (!var->isLenAttribIndirect()) {
//                            sizeVar->setIgnoreFlag(true);
//                        }

//                        if (var->original.type() == "void") {
//                            if (isPointer) {
//                                var->setFullType("", "uint8_t", "");
//                            } else {
//                                var->setFullType("", "DataType", "");
//                                var->setTemplate("DataType");
//                                sizeVar->setIgnoreFlag(false);
//                            }
//                        }

//                        var->convertToArrayProxy();
//                    }
//                };

            const auto convertName =
                [](const std::shared_ptr<VariableData> &var) {
                    const std::string &id = var->identifier();
                    if (id.size() >= 2 && id[0] == 'p' && std::isupper(id[1])) {
                        var->setIdentifier(strFirstLower(id.substr(1)));
                    }
                };

            for (const auto &p : params) {
                transformToProxy(p);
            }

            for (const auto &p : params) {
                const std::string &type = p->original.type();
                if (ctx.ns == Namespace::RAII &&
                    (ctx.nameCat == MemberNameCategory::CREATE ||
                     ctx.nameCat == MemberNameCategory::ALLOCATE ||
                     ctx.constructor)) {
                    if (ctx.gen.isHandle(type)) {
                        std::string ns =
                            ctx.gen.getConfig().macro.mNamespaceRAII->get();
                        p->toRAII();
                        convertName(p);
                    }
                }

                if (p->isArray()) {
                    convertName(p);
                    continue;
                }
                if (ctx.gen.isStructOrUnion(type)) {
                    p->convertToReference();
                    p->setConst(true);
                    convertName(p);
                }
            }
        }

        void finalizeArguments() {        
            bool hasAssignment = true;
            for (auto it = ctx.params.rbegin(); it != ctx.params.rend(); it++) {
                if (it->get()->getIgnoreFlag()) {
                    continue;
                }
                if (hasAssignment && it->get()->assignment().empty()) {
                    hasAssignment = false;
                }
                if (it->get()->type() == "AllocationCallbacks") {
                    allocatorVar = *it;
                    if (ctx.gen.getConfig().gen.allocatorParam) {
                        allocatorVar->convertToOptional();
                        if (hasAssignment) {
                            allocatorVar->setAssignment(" = nullptr");
                        }
                    } else {
                        allocatorVar->setIgnoreFlag(true);
                    }
                }
            }
        }

      protected:
        virtual std::string generateMemberBody() override {
            std::string output;
            bool immediate = ctx.pfnReturn != PFNReturnCategory::VK_RESULT &&
                             ctx.successCodes.size() <= 1;
            output += "    " + generatePFNcall(immediate) + "\n";
            if (!immediate) {
                returnValue = generateReturnValue(resultVar.identifier());
            }
            return output;
        }

      public:
        MemberResolver(MemberContext &refCtx) : MemberResolverBase(refCtx) {

            transformMemberArguments();

            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT &&
                ctx.successCodes.size() <= 1) {
                returnType = "void";
            }
        }

        virtual ~MemberResolver() {}

        virtual void generate(std::string &decl, UnorderedOutput &def) override {
            finalizeArguments();
            MemberResolverBase::generate(decl, def);
        }
    };

    class MemberResolverDbg : public MemberResolver {
      public:
        MemberResolverDbg(MemberContext &refCtx) : MemberResolver(refCtx) {
            dbgtag = "disabled";
        }

        virtual void generate(std::string &decl, UnorderedOutput &def) override {
            decl += "/*\n";
            decl += generateDeclaration();
            decl += "*/\n";
        }
    };

    class MemberResolverEmpty
        : public MemberResolver { // used only for debug markup
        std::string comment;

      public:
        MemberResolverEmpty(MemberContext &refCtx, std::string comment)
            : MemberResolver(refCtx), comment(comment) {
            dbgtag = "dbg";
        }

        virtual std::string generateMemberBody() override {
            return comment.empty() ? "" : "      // " + comment + "\n";
        }
    };

    class MemberResolverPass : public MemberResolverBase {

      public:
        MemberResolverPass(MemberContext &refCtx)
            : MemberResolverBase(refCtx) {
            ctx.disableSubstitution = true;
        }

        virtual std::string generateMemberBody() override {
            return "    " + generatePFNcall(true) + "\n";
        }
    };

    class MemberResolverVectorRAII : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> parent;
        std::shared_ptr<VariableData> last;
        bool ownerInParent;

      public:
        MemberResolverVectorRAII(MemberContext &refCtx, bool term = true)
            : MemberResolver(refCtx) {
            if (term) {
                last = *ctx.params.rbegin();
                last->setIgnoreFlag(true);
                last->setIgnorePFN(true);
                if (last->hasLengthVar() && !last->isLenAttribIndirect()) {
                    last->getLengthVar()->setIgnorePFN(true);
                }
                last->toRAII();
                ctx.pfnReturn = PFNReturnCategory::VOID;

                if (last->hasLengthVar() && last->isArray()) {
                    if (last.get()->original.type() == "void") {
                        if (last->isArrayIn()) {
                            last->getLengthVar()->setIgnoreFlag(false);
                        }
                    }

                    last->getLengthVar()->removeLastAsterisk();

                    last->convertToStdVector();
                } else {
                    last->convertToReturn();
                }
                ctx.useThis = true;

                returnType = last->fullType();
            }

            parent = *ctx.params.begin();
            parent->toRAII();
            ownerInParent = false;
            if (parent->original.type() != ctx.cls->superclass.original) {
                ownerInParent = true;
            } else {
                parent->setIgnorePFN(true);
            }

            std::string id = parent->identifier();

            ctx.pfnSourceOverride = id;
            if (ownerInParent) {
                ctx.pfnSourceOverride += ".get" + ctx.cls->superclass + "()";
            }

            ctx.params.rbegin()->get()->setIgnoreFlag(true);

            dbgtag = "RAII vector";
        }

        virtual std::string generateMemberBody() override {
            std::string args = createPassArguments();
            return "    return " + last->namespaceString() + last->type() +
                   "s(" + args + ");\n";
        }
    };

    class MemberResolverStructCtor : public MemberResolverBase {

    public:
      MemberResolverStructCtor(MemberContext &refCtx, bool transform) : MemberResolverBase(refCtx) {
            InitializerBuilder init{"      "};
            auto pNext = ctx.params.end();
            for (auto it = ctx.params.begin(); it != ctx.params.end(); ++it) {
                auto &p = *it;

                p->setAssignment(transform? "" : " = {}");
                const std::string &id = p->identifier();
                if (id == "pNext") {
                    p->setAssignment(" = nullptr");
                    pNext = it;
                }
                p->setIdentifier(id + "_");
//                if (p->hasArrayLength()) {
//                    p->setSpecialType(VariableData::TYPE_ARRAY);
//                    // std::cout << "set to array:" << p->identifier() << std::endl;
//                }
            }
            for (const auto &p : ctx.params) {
                if (p->type() == "StructureType") {
                    p->setIgnoreFlag(true);
                    continue;
                }
                bool toProxy = transform && p->hasLengthVar();
                if (toProxy) {
                    transformToProxyNoTemporaries(p);
                    p->getLengthVar()->setIgnoreFlag(true);
                }

                std::string lhs = p->original.identifier();
                if (toProxy) {
                    init.append(lhs, p->toArrayProxyData());
                }
                else {
                    std::string rhs = p->identifier();
                    const auto &vars = p->getArrayVars();
                    if (!vars.empty() && transform) {
                        rhs = "static_cast<uint32_t>(";
                        // std::cout << "> array vars: " << vars.size() << std::endl;
                        for (size_t i = 0; i < vars.size(); ++i) {
                            const auto &v = vars[i];
                            const std::string &id = v->identifier();
                            if (i != vars.size() - 1) {
                                rhs += " !" + id + ".empty()? " + id + ".size() :\n";
                            }
                            else {
                                rhs += id + ".size()";
                            }
                        }
                        rhs += ")";
                    }
                    init.append(lhs, rhs);
                }
            }

            if (pNext != ctx.params.end()) {
                auto var = *pNext;
                ctx.params.erase(pNext);
                ctx.params.push_back(var);
            }

            specifierConstexpr = !transform;
            initializer = init.string();
      }

      virtual std::string generateMemberBody() override {
        return "";
      }
    };

    class MemberResolverCtor : public MemberResolverVectorRAII {
      protected:
        String name;

      public:
        MemberResolverCtor(MemberContext &refCtx)
            : MemberResolverVectorRAII(refCtx, false), name("") {

            name = convertName(ctx.name.original, ctx.cls->superclass);
            ctx.name = std::string(ctx.cls->name);

            ctx.pfnNameOverride = name;
            if (ownerInParent) {
                ctx.pfnSourceOverride += "->";
            } else {
                ctx.pfnSourceOverride += ".";
            }

            returnType = "";

            specifierInline = true;
            specifierExplicit = true;
            specifierConst = false;

            dbgtag = "constructor";
            disableCheck = true;
        }

        virtual std::string generateMemberBody() override {
            std::string output;

            for (const auto &p : ctx.params) {
                if (p->original.type() != ctx.cls->name.original) {
                    p->original.set(1, p->get(1));
                }
            }

            const std::string &args = ctx.createPFNArguments(true);
            const std::string &owner = ctx.cls->ownerhandle;
            std::string id = parent->identifier();
            std::string src = id;
            if (ownerInParent) {
                src += ".get" + ctx.cls->superclass + "()";
            }

            if (!owner.empty()) {
                output += "    " + owner + " = ";
                if (!ownerInParent) {
                    output += "&";
                }
                output += src;
                output += ";\n";
            }

            std::string call = generatePFNcall(false);

            output += "    " + call + "\n";
            output += generateCheck();

            if (ctx.cls->hasPFNs()) {                
                output += "    loadPFNs(";
                if (ownerInParent) {
                    output += "*";
                }
                output += src + ");\n";
            }

            return output;
        }

        bool checkMethod() const {            
            if (!ctx.gen.isHandle(parent->original.type())) {
//                 std::cout << "checkMethod: " << name.original
//                          << " not a handle (" << parent->original.type() << ")" << std::endl;
                return false;
            }
            bool blacklisted = true;
            ctx.gen.genOptional(ctx,
                                [&](std::string &output) { // REFACTOR
                                    blacklisted = false;
                                });
            if (blacklisted) {
//                    std::cout << "checkMethod: " << name.original
//                              << " blacklisted" << std::endl;
                return false;
            }
            const auto &handle =
                    ctx.gen.findHandle(parent->original.type());
            for (const auto &m : handle.members) {
                if (m.name.original == name.original) {
                    return true;
                }
            }
//            std::cout << "checkMethod: " << name.original << " not in "
//                      << handle.name << " members" << std::endl;
            return false;
        }

        std::string getName() const { return name; }
    };

    class MemberResolverUniqueCtor : public MemberResolver {

    public:
        MemberResolverUniqueCtor(MemberContext &refCtx)
            : MemberResolver(refCtx)
        {
            ctx.name = "Unique" + ctx.cls->name;

            returnType = "";

            specifierInline = false;
            specifierExplicit = true;
            specifierConst = false;

            InitializerBuilder init("        ");
            for (const auto &p : ctx.getFilteredProtoVars()) {
                if (p->type() == ctx.cls->name) {
                    init.append(ctx.cls->name, p->identifier());
                    continue;
                }
                for (const auto &v : ctx.cls->uniqueVars) {
                    if (p->type() == v.type()) {
                        std::string m = matchTypePointers(p->suffix(), v.suffix());
                        init.append(v.identifier(), m + p->identifier());
                        break;
                    }
                }
            }

            initializer = init.string();

            dbgtag = "constructor";
        }

        virtual std::string generateMemberBody() override {
            return "";
        }

    };

    class MemberResolverVectorCtor : public MemberResolverCtor {

        std::shared_ptr<VariableData> last;

      public:
        MemberResolverVectorCtor(MemberContext &refCtx)
            : MemberResolverCtor(refCtx) {

            last = ctx.getLastPointerVar();

            if (last->hasLengthVar() && last->isArray()) {                
                last->getLengthVar()->removeLastAsterisk();
                last->convertToStdVector();
            } else {
                std::cerr << "MemberResolverVectorCtor: can't create"
                          << std::endl;
            }

            last->setIgnoreFlag(true);
            last->setNamespace(Namespace::VK);

            specifierExplicit = false;
        }

        virtual std::string generateMemberBody() override {
            std::string output = generateArrayCode(last, true);
            return output;
        }
    };

    class MemberResolverCreateHandle : public MemberResolver {
        std::shared_ptr<VariableData> returnVar;

      public:
        MemberResolverCreateHandle(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            returnVar = ctx.getLastPointerVar();
            returnVar->convertToReturn();
            returnType = returnVar->type();            
            dbgtag = "create handle";
        }

        virtual std::string generateMemberBody() override {
            const std::string &id = returnVar->identifier();
            const std::string &call = generatePFNcall();
            return ctx.gen.format(R"(
    {0} {1};
    {2}
    {3}
)",
                                  returnType, id, call,
                                  generateReturnValue(id));
        }
    };

    class MemberResolverCreate : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;

      public:
        MemberResolverCreate(MemberContext &refCtx) : MemberResolver(refCtx) {

            last = ctx.getLastPointerVar();

            if (last->isArray()) {
                last->convertToStdVector();
            } else {
                last->convertToReturn();
            }

            last->setIgnoreFlag(true);

            last->setConst(false);
            returnType = last->fullType();
            disableCheck = false;
            dbgtag = ctx.nameCat == MemberNameCategory::ALLOCATE ? "allocate"
                                                                 : "create";
            ctx.useThis = true;
        }

        virtual std::string generateMemberBody() override {
            std::string output;
            if (last->isArray()) {
                output += generateArrayCode(last);
            } else {
                if (ctx.ns == Namespace::RAII) {
                    last->setIgnorePFN(true);
                    const auto &first = ctx.params.begin()->get();
                    if (first->original.type() == ctx.cls->name.original) {
                        first->setIgnorePFN(true);
                    }
                    std::string args = createPassArguments();
                    output +=
                        "    return " + last->fullType() + "(" + args + ");\n";
                } else {
                    const std::string &call = generatePFNcall();
                    std::string id = last->identifier();
                    output += "    " + last->fullType() + " " + id + ";\n";
                    output += "    " + call + "\n";
                    returnValue = generateReturnValue(id);
                }
            }
            return output;
        }
    };

    class MemberResolverCreateUnique : public MemberResolverCreate {        
        std::string name;
        bool isSubclass = {};

      public:
        MemberResolverCreateUnique(MemberContext &refCtx)
            : MemberResolverCreate(refCtx) {

            returnType = "Unique" + last->type();            

            if (last->isHandle()) {
                const auto &handle = ctx.gen.findHandle(last->original.type());
                isSubclass = handle.isSubclass;
            }
            name = ctx.name;
            ctx.name += "Unique";
            dbgtag = "create unique";
        }

        virtual std::string generateMemberBody() override {            
            std::string args = last->identifier();
            if (isSubclass) {
                args += ", *this";
            }
            if (ctx.gen.getConfig().gen.allocatorParam) {
                args += ", allocator";
            }
            if (ctx.gen.getConfig().gen.dispatchParam) {
                args += ", d";
            }

            std::string output = MemberResolverCreate::generateMemberBody();            
            returnValue = generateReturnValue(returnType + "(" + args + ")");            
            return output;
        }
    };

    class MemberResolverGet : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;

        std::string generateSingle() {
            std::string output;            
            const std::string id = last->identifier();
            output += "    " + returnType + " " + id + ";\n";
            output += "    " + generatePFNcall() + "\n";
            returnValue = generateReturnValue(id);
            return output;
        }

      public:
        MemberResolverGet(MemberContext &refCtx) : MemberResolver(refCtx) {
            last = ctx.getLastPointerVar();

            if (last->hasLengthVar() && last->isArray()) {
                if (last.get()->original.type() == "void") {
                    if (last->isArrayIn()) {
                        last->getLengthVar()->setIgnoreFlag(false);
                    }
                }

                last->getLengthVar()->removeLastAsterisk();

                last->convertToStdVector();
            } else {
                last->convertToReturn();
            }

            last->setIgnoreFlag(true);
            last->setConst(false);
            returnType = last->fullType();
            dbgtag = "get";
        }

        virtual std::string generateMemberBody() override {
            if (last->isArray()) {
                return generateArrayCode(last);                
            }
            return generateSingle();
        }
    };

    class MemberResolverEnumerate : public MemberResolverGet {
      public:
        MemberResolverEnumerate(MemberContext &refCtx)
            : MemberResolverGet(refCtx) {
            dbgtag = "enumerate";
        }
    };

    PFNReturnCategory evaluatePFNReturn(const std::string &type) const {
        if (type == "void") {
            return PFNReturnCategory::VOID;
        } else if (type == "VkResult") {
            return PFNReturnCategory::VK_RESULT;
        }
        return PFNReturnCategory::OTHER;
    }

    bool getLastTwo(MemberContext &ctx,
                    std::pair<Variables::reverse_iterator,
                              Variables::reverse_iterator> &data) { // rename ?
        Variables::reverse_iterator last = ctx.params.rbegin();
        if (last != ctx.params.rend()) {
            Variables::reverse_iterator prevlast = std::next(last);
            data.first = last;
            data.second = prevlast;
            return true;
        }
        return false;
    }

    bool isPointerToCArray(const std::string &id) {
        if (id.size() >= 2) {
            if (id[0] == 'p' && std::isupper(id[1])) {
                return true;
            }
        }
        return false;
    }

    std::string stripStartingP(const std::string &str) {
        std::string out = str;
        if (isPointerToCArray(str)) {
            out = std::tolower(str[1]);
            out += str.substr(2);
        }
        return out;
    }

    template <typename T>
    void
    createOverload(MemberContext &ctx, const std::string &name,
                   std::vector<std::unique_ptr<MemberResolver>> &secondary);

    void generateClassMember(MemberContext &ctx, GenOutputClass &out,
                             UnorderedOutput &funcs);

    void generateClassMembers(HandleData &data, GenOutputClass &out,
                              UnorderedOutput &funcs, Namespace ns);

    void generateClassConstructors(const HandleData &data, GenOutputClass &out);

    void generateClassConstructorsRAII(const HandleData &data,
                                       GenOutputClass &out, UnorderedOutput &funcs);

    std::string generateUniqueClass(HandleData &data, UnorderedOutput &funcs);

    String getHandleSuperclass(const HandleData &data);

    std::string generateClass(const std::string &name, HandleData data,
                              UnorderedOutput &funcs);

    std::string generateClassRAII(const std::string &name, HandleData data,
                                UnorderedOutput &funcs);

    void parseCommands(XMLNode *node);

    void assignCommands();

    std::string generatePFNs(const HandleData &data, GenOutputClass &out);

    std::string generateLoader();

    std::string genMacro(const Macro &m);

    void initLoaderName();

    std::string beginNamespace(Namespace ns = Namespace::VK) const;

    std::string endNamespace(Namespace ns = Namespace::VK) const;

    void loadFinished();

    class WhitelistBuilder {
        std::string text;
        bool hasDisabledElement = false;

    public:        

        template<typename T>
        void add(const T &t) {
            if constexpr (is_reference_wrapper<T>::value) {
                append(t.get());
            }
            else if constexpr (std::is_pointer<T>::value) {
                append(*t);
            }
            else {
                append(t);
            }
        }

        void append(const BaseType &t) {
            if (t.isEnabled() || t.isRequired()) {
                auto name = t.name.original;
                text += "            " + name + "\n";
            }
            else if(t.isSuppored()) {
                hasDisabledElement = true;
            }
        }

        void insertToParent(XMLElement *parent, const std::string &name, const std::string &comment) {            
            if (!text.empty()) {
                text = "\n" + text + "        ";
            }
            XMLElement *elem = parent->GetDocument()->NewElement(name.c_str());
            if (!comment.empty()) {
                elem->SetAttribute("comment", comment.c_str());
            }
            if (!hasDisabledElement) {
                XMLElement *reg = parent->GetDocument()->NewElement("regex");
                reg->SetText(".*");
                elem->InsertEndChild(reg);
            }
            else {
                elem->SetText(text.c_str());
            }
            parent->InsertEndChild(elem);
        }

    };

    template<typename T>
    void configBuildList(const std::string &name, const std::map<std::string, T> &from, XMLElement *parent, const std::string &comment = "");

    template<typename T>
    void configBuildList(const std::string &name, const std::vector<T> &from, XMLElement *parent, const std::string &comment = "");

    struct AbstractWhitelistBinding {
        std::string name;
        std::vector<std::string> ordered;
        std::unordered_set<std::string> filter;
        std::vector<std::regex> regexes;

        virtual ~AbstractWhitelistBinding() {
        }

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
    };

    template<typename T>
    struct WhitelistBinding : public AbstractWhitelistBinding {
        std::vector<T> *dst;

        WhitelistBinding(std::vector<T> *dst, std::string name)
         : dst(dst)
        {
            this->name = name;
        }

        virtual void apply() noexcept override;
    };

  public:
    Generator();

    static std::string findDefaultRegistryPath();

    void resetConfig();

    void loadConfigPreset();

    void bindGUI(const std::function<void(void)> &onLoad);

    bool isLoaded() const { return root != nullptr; }

    void setOutputFilePath(const std::string &path);

    bool isOuputFilepathValid() const {
        return std::filesystem::is_directory(outputFilePath);
    }

    std::string getOutputFilePath() const { return outputFilePath; }

    std::string getRegistryPath() const { return registryPath; }

    void load(const std::string &xmlPath);

    void unload();

    void generate();

    Platforms &getPlatforms() { return platforms; };

    Extensions &getExtensions() { return extensions; };

    auto& getCommands() { return commands; }

    auto& getStructs() { return structs; }

    auto &getEnums() { return enums; }

    Config &getConfig() { return cfg; }

    const Config &getConfig() const { return cfg; }

    bool isInNamespace(const std::string &str) const;

    std::string getNamespace(Namespace ns, bool colons = true) const;

    std::function<void(void)> onLoadCallback;

    void saveConfigFile(const std::string &filename);

    template<typename T>
    void saveConfigParam(XMLElement *parent, const T& t);

    template<size_t I = 0, typename... Tp>
    void saveConfigParam(XMLElement *parent, const std::tuple<Tp...>& t);

    template<typename T>
    void loadConfigParam(XMLElement *parent, T& t);

    template<size_t I = 0, typename... Tp>
    void loadConfigParam(XMLElement *parent, const std::tuple<Tp...>& t);

    void loadConfigFile(const std::string &filename);
};

#endif // GENERATOR_HPP
