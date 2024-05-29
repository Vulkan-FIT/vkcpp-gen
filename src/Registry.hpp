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

#ifndef GENERATOR_REGISTRY_HPP
#define GENERATOR_REGISTRY_HPP

#include "Utils.hpp"
#include "Variable.hpp"

#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace vkgen
{

    using namespace Utils;

    template <typename T>
    struct is_reference_wrapper : std::false_type
    {
    };

    template <typename T>
    struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type
    {
    };

    template <typename T>
    class EnumFlag
    {
        using Type = typename std::underlying_type<T>::type;
        Type m_flags;

      public:
        EnumFlag() = default;

        EnumFlag(const T &flags) : m_flags(static_cast<Type>(flags)) {}

        bool operator&(const T &rhs) const {
            return (m_flags & static_cast<Type>(rhs)) != 0;
        }

        operator const T() const {
            return m_flags;
        }

        EnumFlag<T> &operator|=(const T &b) {
            m_flags |= static_cast<Type>(b);
            return *this;
        }

        EnumFlag<T> &operator&=(const T &b) {
            m_flags &= static_cast<Type>(b);
            return *this;
        }

        const T operator~() const {
            return static_cast<T>(~m_flags);
        }
    };

    template <template <class...> class TContainer, class T, class A, class E>
    static bool isInContainter(const TContainer<T, A> &array, const E &entry) {
        return std::find(std::begin(array), std::end(array), entry) != std::end(array);
    }

#ifdef GENERATOR_GUI
    struct SelectableGUI
    {
        bool selected = {};
        bool hovered  = {};
        bool filtered = true;

        // virtual void setEnabled(bool, bool ifSelected = false) {}

        virtual void setEnabledChildren(bool value, bool ifSelected = false) {}

        void setSelected(bool value) {
            selected = value;
        }

        bool isSelected() const {
            return selected;
        }
    };

    // template <typename T> struct SelectableData : public Selectable {
    //     T *data;
    //
    //     SelectableData(T &data) : data(&data) {}
    //
    //     virtual void setEnabled(bool value, bool ifSelected = false) override {
    //         if (ifSelected && !selected) {
    //             return;
    //         }
    //         std::cout << "set enabled: " << (int)value << std::endl;
    //         data->setEnabled(value);
    //     };
    //
    //     virtual void setSelected(bool value) override { selected = value; }
    // };

#endif

    class Generator;

    class Variables : public std::vector<std::unique_ptr<VariableData>>
    {
      public:
        void bind() {
            const auto findVar = [&](const std::string &id) -> VariableData * {
                for (auto &p : *this) {
                    if (p->original.identifier() == id) {
                        return p.get();
                    }
                }
                std::cerr << "can't find param (" + id + ")" << '\n';
                return nullptr;
            };

            for (auto &p : *this) {
                if (!p->getArrayVars().empty()) {
                    std::cerr << "Array vars not empty" << '\n';
                    p->getArrayVars().clear();
                }
            }

            for (auto &p : *this) {
                const std::string &len = p->getLenAttribIdentifier();
                if (!len.empty() && p->getAltlenAttrib().empty()) {
                    const auto &var = findVar(len);
                    if (var) {
                        p->bindLengthVar(*var);
                        var->bindArrayVar(p.get());
                    }
                }
#ifndef NDEBUG
                if (p->bound) {
                    std::cerr << "Already bound variable" << '\n';
                }
                p->bound = true;
#endif
            }
            for (auto &p : *this) {
                p->evalFlags();
                p->save();
            }
        }
    };

    namespace vkr
    {
        struct Extension;
        struct Handle;
        struct Command;
    }  // namespace vkr

    static constexpr auto temp = "x";

    struct BaseType
      : public MetaType
#ifdef GENERATOR_GUI
      , public SelectableGUI
#endif
    {
      private:
        vkr::Extension *ext = {};

      public:
        String name = { "" };

        std::set<BaseType *> dependencies;
        std::set<BaseType *> subscribers;
        std::vector<String>  aliases;
        std::string_view     protect;
        const char          *version       = temp;
        bool                 forceRequired = {};

        BaseType() = default;

        explicit BaseType(MetaType::Value type) noexcept : MetaType(type) {}

        BaseType(MetaType::Value type, std::string_view name, bool firstCapital = false) : name(std::string{ name }, firstCapital), MetaType(type) {}

        std::string_view getProtect() const;

        void setProtect(const std::string_view);

        bool hasProtect() const {
            return !protect.empty();
        }

        void setExtension(vkr::Extension *ext);

        void bind(vkr::Extension *ext, const std::string_view protect);

        void setUnsuppored() {
            supported = false;
            version   = nullptr;
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

        void addAlias(const std::string_view alias) {
            aliases.emplace_back(std::string{ alias }, true);
        }

        void setEnabled(bool value) {
            if (enabled == value || !supported) {
                return;
            }
            enabled = value;
            if (enabled) {
                // std::cout << "> enable: " << name << '\n';
            } else {
                // std::cout << "> disable: " << name << '\n';
            }

            if (enabled) {
                // std::cout << "    enable: " << name << '\n';
                for (auto &d : dependencies) {
                    if (!subscribers.contains(d)) {
                        d->subscribe(this);
                    }
                }
            } else {
                // std::cout << "     disable: " << name << '\n';
                for (auto &d : dependencies) {
                    if (!subscribers.contains(d)) {
                        d->unsubscribe(this);
                    }
                }
            }
        }

      protected:
        void subscribe(BaseType *s) {
            if (!subscribers.contains(s)) {
                bool empty = subscribers.empty();
                subscribers.emplace(s);
                // std::cout << "  subscribed to: " << name << '\n';
                if (empty) {
                    setEnabled(true);
                }
            }
        }

        void unsubscribe(BaseType *s) {
            auto it = subscribers.find(s);
            if (it != subscribers.end()) {
                subscribers.erase(it);
                // std::cout << "  unsubscribed to: " << name << '\n';
                if (subscribers.empty()) {
                    setEnabled(false);
                }
            }
        }

        bool enabled   = false;
        bool supported = true;
    };

    namespace vkr
    {

        // holds information about class member (function)
        struct ClassCommand
        {
            enum class PrivateFlags
            {
                NONE = 0
            };  // TODO refactor enum class

            class Flags : public Enums::Flags<PrivateFlags>
            {
              public:
                using enum PrivateFlags;
            };

            const Handle *cls      = {};
            Command      *src      = {};
            String        name     = { "" };
            bool          raiiOnly = {};
            Flags         flags    = {};

            ClassCommand(const Generator *gen, const Handle *cls, Command &o);

            ClassCommand &operator=(ClassCommand &o) noexcept = default;

            bool valid() const {
                return !name.empty();
            }

            operator BaseType() const;
        };

        struct Handle : public BaseType
        {
            enum class CreationCategory
            {
                NONE,
                ALLOCATE,
                CREATE
            };

            String                        superclass;
            VariableData                  vkhandle;
            std::string                   ownerhandle;
            std::unique_ptr<VariableData> ownerRaii;
            std::unique_ptr<VariableData> ownerUnique;
            std::unique_ptr<VariableData> secondOwner;
            // std::string alias;
            Handle          *parent      = {};
            CreationCategory creationCat = CreationCategory::NONE;

            std::optional<ClassCommand> getAddrCmd;
            std::vector<ClassCommand>   members;
            std::vector<ClassCommand *> filteredMembers;
            std::vector<ClassCommand>   ctorCmds;
            vkr::Command               *dtorCmd = {};
            std::vector<ClassCommand>   vectorCmds;

            std::vector<std::reference_wrapper<const VariableData>> vars;
            bool poolFlag = false;

            void foreachVars(VariableData::Flags flags, std::function<void(const VariableData &var)> f) const {
                for (const VariableData &v : vars) {
                    if (!hasFlag(v.getFlags(), flags)) {
                        continue;
                    }
                    f(v);
                }
            }

            int  effectiveMembers = 0;
            bool isSubclass       = false;
            bool vectorVariant    = false;

            explicit Handle(Generator &gen) : BaseType(MetaType::Handle), superclass(""), vkhandle(VariableData::TYPE_INVALID) {}

            Handle(Generator &gen, xml::Element elem);

            static Handle &empty(Generator &gen) {
                static Handle h{ gen };
                return h;
            }

            void clear() {}

            void init(Generator &gen);

            void prepare(const Generator &gen);

            void setParent(const Registry &reg, Handle *h);

            void addCommand(const Generator &gen, vkr::Command &cmd, bool raiiOnly = false);

            void setDestroyCommand(const Generator &gen, vkr::Command &cmd);

            bool hasPFNs() const {
                return effectiveMembers > 0 && !isSubclass;
            }

            bool uniqueVariant() const {
                return creationCat != CreationCategory::NONE;
            }
        };

        struct Struct;

        struct Command : public BaseType
        {
          private:
            void init(const Registry &reg);

            void initParams() {
                if (params.size() != _params.size()) {
                    params.clear();
                    params.reserve(_params.size());
                    for (auto &p : _params) {
                        params.push_back(std::ref(*p));
                    }
                    // std::cout << "cmd prepare clean " << this << '\n';
                } else {
                    for (size_t i = 0; i < _params.size(); ++i) {
                        params[i] = std::ref(*_params[i]);
                    }
                }
            }

          public:
            enum class NameCategory
            {
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

            enum class CommandFlags : uint8_t
            {
                NONE     = 0,
                ALIAS    = 1,
                INDIRECT = 1 << 1,
                // RAII_ONLY = 4,
                CREATES_HANDLE     = 1 << 3,
                CPP_VARIANT        = 1 << 4,
                OVERLOADED_DESTROY = 1 << 5,
            };

            enum class PFNReturnCategory
            {
                OTHER,
                VOID,
                VK_RESULT
            };

            Variables _params;  // command arguments

            std::vector<std::reference_wrapper<VariableData>> params;
            std::vector<std::reference_wrapper<VariableData>> outParams;
            std::string                                       type;  // return type
            // std::string alias;
            std::vector<std::string> successCodes;
            NameCategory             nameCat;
            PFNReturnCategory        pfnReturn;
            EnumFlag<CommandFlags>   flags{};
            vkr::Handle             *top{};
            const vkr::Struct             *structChain{};

            //  Command(const Generator &gen, xml::Element elem,
            //          const std::string &className) noexcept;
            Command(const Generator &gen, xml::Element elem) noexcept;

            Command(const Registry &reg, const Command &o, std::string_view alias);

            // copy constuctor
            Command(const Command &) = delete;
            // move constuctor
            Command(Command &&o) noexcept = default;

            void check() {
                bool ok;
                check(ok);
            }

            void check(bool &ok) {
                const auto find = [&](VariableData *v) {
                    for (auto &p : _params) {
                        if (v == p.get()) {
                            return true;
                        }
                    }
                    std::cerr << "INTEGRITY CHECK: var " << v << " not found" << '\n';
                    return false;
                };

                std::stringstream s;
                ok = true;
                for (auto &p : _params) {
                    s << "  p: " << p.get() << '\n';
                    auto v = p->getLengthVar();
                    if (v) {
                        s << "    l: " << v;
                        bool f = find(v);
                        if (!f) {
                            s << " <--";
                        }
                        s << '\n';
                        ok &= f;
                    }
                }

                for (VariableData &v : params) {
                    if (v.getSpecialType() != VariableData::TYPE_DEFAULT) {
                        continue;
                    }
                    s << "  &p: " << &v;
                    bool f = find(&v);
                    if (!f) {
                        s << " <--";
                    }
                    s << '\n';
                    ok &= f;

                    auto l = v.getLengthVar();
                    if (l) {
                        s << "     l: " << l;
                        bool f = find(l);
                        if (!f) {
                            s << " <--";
                        }
                        s << '\n';
                        ok &= f;
                    }
                }

                if (!ok) {
                    std::cout << "chk command: " << name << ", " << _params.size() << ", " << params.size() << " params" << '\n';
                    std::cerr << s.str() << '\n';
                }
            }

            void prepare() {
                if (prepared) {
                    // return;
                }
                // std::cerr << "  >cmd prepare  " << this << '\n';
                for (auto &v : _params) {
                    v->restore();
                }
                initParams();

                //            if (params.size() != _params.size()) {
                //                params.clear();
                //                params.reserve(_params.size());
                //                for (auto &p : _params) {
                //                    params.push_back(std::ref(*p));
                //                }
                //            }
                //            else {
                //                params.clear();
                //                params.reserve(_params.size());
                //                for (auto &p : _params) {
                //                    params.push_back(std::ref(*p));
                //                }
                //            }
                // std::cout << "cmd prepare " << this << '\n';
#ifndef NDEBUG
                check();
#endif
                prepared = true;
            }

            void setFlagBit(CommandFlags bit, bool enabled) {
                if (enabled) {
                    flags |= bit;
                } else {
                    flags &= ~EnumFlag<CommandFlags>{ bit };
                }
            }

            bool isIndirect() const {
                return flags & CommandFlags::INDIRECT;
            }

            bool canTransform() const {
                return flags & CommandFlags::CPP_VARIANT;
            }

            bool hasOverloadedDestroy() const {
                return flags & CommandFlags::OVERLOADED_DESTROY;
            }

            bool isAlias() const {
                return flags & CommandFlags::ALIAS;
            }

            bool createsHandle() const {
                return flags & CommandFlags::CREATES_HANDLE;
            }

            bool isStructChain() const {
                return structChain;
            }

            bool getsObject() const {
                switch (nameCat) {
                    case NameCategory::ACQUIRE:
                    case NameCategory::GET: return true;
                    default: return false;
                }
            }

            bool destroysObject() const {
                switch (nameCat) {
                    case NameCategory::DESTROY:
                    case NameCategory::FREE: return true;
                    default: return false;
                }
            }

            bool returnsVector() const {
                for (const VariableData &v : outParams) {
                    if (v.isArray()) {
                        return true;
                    }
                }
                return false;
            }

            bool hasParams() const {
                return !_params.empty();
            }

            // TODO deprecated
            bool isIndirectCandidate(const std::string_view &type) const {
                try {
                    if (getsObject() || createsHandle()) {
                        const auto var = getLastPointerVar();
                        if (!var) {
                            return false;
                        }
                        if (nameCat != NameCategory::GET) {
                            return !var->isArray();
                        }
                        return var->original.type() != type;
                    } else if (destroysObject()) {
                        const auto var = getLastHandleVar();
                        if (!var) {
                            return false;
                        }
                        return var->original.type() != type;
                    }
                }
                catch (std::runtime_error) {
                }
                return true;
            }

            vkr::Handle *secondIndirectCandidate(Generator &gen) const;

            void setName(const Registry &reg, const std::string &name);

            bool containsPointerVariable() {
                for (const VariableData &v : params) {
                    if (v.original.isPointer()) {
                        return true;
                    }
                }
                return false;
            }

            VariableData *getVar(size_t index) const {
                if (params.size() >= index) {
                    return nullptr;
                }
                return &params[index].get();
            }

            VariableData *getLastVar() const {
                if (params.empty()) {
                    return nullptr;
                }
                return &params.rbegin()->get();
            }

            VariableData *getFirstVar() const {
                if (params.empty()) {
                    return nullptr;
                }
                return &params.begin()->get();
            }

            VariableData *getLastVisibleVar() const {
                for (VariableData &v : params) {
                    if (!v.getIgnoreFlag()) {
                        return &v;
                    }
                }
                throw std::runtime_error("can't get param (last visible)");
            }

            VariableData *getLastPointerVar() const {
                for (VariableData &v : params) {
                    if (v.original.isPointer()) {
                        return &v;
                    }
                }
                throw std::runtime_error("can't get param (last pointer)");
            }

            VariableData *getLastHandleVar() const {
                for (auto it = params.rbegin(); it != params.rend(); ++it) {
                    if (it->get().isHandle()) {
                        return &it->get();
                    }
                }
                return nullptr;
                //            for (VariableData &v : std::ranges::reverse_view(params)) {
                //                if (v.isHandle()) {
                //                    return &v;
                //                }
                //            }
                // throw std::runtime_error("can't get param (last handle)");
            }

            VariableData *getFirstHandleVar() const {
                for (VariableData &v : params) {
                    if (v.isHandle()) {
                        return &v;
                    }
                }
                throw std::runtime_error("can't get param (first handle)");
            }

            // private:
            bool prepared = false;
        };

        struct Platform : public BaseType
        {
            std::string_view protect;

            Platform(const std::string_view name, const std::string_view protect, bool enabled) : protect(protect) {
                BaseType::name.reset(std::string{ name });
                BaseType::enabled = enabled;
            }
        };

        struct Extension : public BaseType
        {
            std::string                 protect;
            Platform                   *platform;
            std::vector<vkr::Command *> commands;
            std::vector<BaseType *>     types;

            Extension(const std::string &name, Platform *platform, bool supported, bool enabled) : platform(platform) {
                BaseType::name.reset(name);
                BaseType::enabled   = enabled;
                BaseType::supported = supported;
                if (platform) {
                    protect = platform->protect;
                }
            }
        };

        struct EnumValue : public BaseType
        {
            std::string value;
            bool        isAlias = {};

            EnumValue(const String &name, const std::string &value, bool isAlias = false, bool enabled = true) : value(value), isAlias(isAlias) {
                this->name    = name;
                this->enabled = true;
            }
        };

        struct EnumValueType : public EnumValue
        {
            std::string type;

            EnumValueType(const String &name, const std::string &value, const std::string &type, bool isAlias = false, bool enabled = true)
              : EnumValue(name, value, isAlias, enabled), type(type) {}
        };

        struct Enum : public BaseType
        {
            std::vector<EnumValue> members;
            bool                   isBitmask = {};

            Enum(const std::string_view name, bool isBitmask = false) : BaseType(MetaType::Enum, name, true), isBitmask(isBitmask) {}

            bool containsValue(const std::string &value) const;

            EnumValue *find(const std::string_view value) noexcept;
        };

        struct Struct : BaseType
        {
            // XMLNode *node;
            std::string           structTypeValue;
            std::vector<Struct *> extends;
            Variables             members;
            bool                  returnedonly = false;

            Struct(Generator &gen, const std::string_view name, MetaType::Value type, const xml::Element &e);

            Struct(const Struct &o) = delete;

            Struct(Struct &&o) noexcept {
                BaseType::operator=(o);
                structTypeValue = std::move(o.structTypeValue);
                // aliases = std::move(o.aliases);
                extends      = std::move(o.extends);
                members      = std::move(o.members);
                returnedonly = o.returnedonly;
            }

            Struct &operator=(const Struct &o) = delete;

            Struct &operator=(Struct &&o) noexcept {
                BaseType::operator=(o);
                structTypeValue = std::move(o.structTypeValue);
                // aliases = std::move(o.aliases);
                extends      = std::move(o.extends);
                members      = std::move(o.members);
                returnedonly = o.returnedonly;
                return *this;
            }
        };

    }  // namespace vkr

    struct Define
    {
        std::string define;

        enum State
        {
            DISABLED,
            ENABLED,
            COND_ENABLED
        } state = {};

        bool enabled() const {
            return state != State::DISABLED;
        }
    };

    struct NDefine : public Define
    {
    };

    struct Macro
    {
        std::string define;
        std::string value;
        //        Macro      *parent     = {};
        bool usesDefine = {};

        Macro(const std::string &define, const std::string &value, bool usesDefine) : define(define), value(value), usesDefine(usesDefine) {}

        const std::string &getDefine() const {
            return usesDefine ? define : value;
        }

        std::string get() const {
            //            if (parent) {
            //                return parent->get() + "::" + getDefine();
            //            }
            return getDefine();
        }

        auto operator<=>(Macro const &) const = default;
    };

    struct Signature
    {
        std::string name;
        std::string args;

        bool operator==(const Signature &o) const {
            if (name != o.name) {
                return false;
            }
            if (args != o.args) {
                return false;
            }
            return true;
        }
    };

    class Registry
    {
      public:
        static std::string to_string(vkr::Command::PFNReturnCategory);

        enum class ArraySizeArgument
        {
            INVALID,
            COUNT,
            SIZE,
            CONST_COUNT
        };

        static std::string to_string(vkr::Command::NameCategory);

        using Types = std::unordered_map<std::string, BaseType *>;

        template <typename T>
        class Container
        {
            static_assert(std::is_base_of<BaseType, T>::value, "T must derive from BaseType");

            std::map<std::string, size_t, std::less<>> map;

          public:
            using iterator       = std::vector<T>::iterator;
            using const_iterator = std::vector<T>::const_iterator;

            std::vector<T>                         items;
            std::vector<std::reference_wrapper<T>> ordered;

            bool contains(const std::string_view name) const {
                return find(name) != end();
            }

            void addAlias(const std::string_view alias, const std::string_view to) {
                auto i = map.find(to);
                if (i == map.end()) {
                    std::cerr << "can't add alias: " << to << " not found" << '\n';
                    return;
                }
                map.emplace(alias, i->second);
            }

            void prepare() {
                map.clear();
                ordered.clear();
                ordered.reserve(items.size());
                for (size_t i = 0; i < items.size(); ++i) {
                    map.emplace(items[i].name.original, i);
                    map.emplace(items[i].name, i);
                    ordered.emplace_back(std::ref(items[i]));
                }
                // std::cout << "prepared container" << '\n';
            }

            void addTypes(Registry::Types &types) {
                for (const auto &k : map) {
                    types.emplace(k.first, &items[k.second]);
                }
            }

            const_iterator find(const std::string_view name, bool dbg = false) const {
                auto it = map.find(std::string{ name });
                if (it == map.end()) {
                    if (dbg)
                        std::cerr << ". " << std::string{ name } << " not found in Container<" << std::string{ typeid(T).name() } << ">\n";
                    return items.end();
                }
                return items.begin() + it->second;
            }

            iterator find(const std::string_view name, bool dbg = false) {
                auto it = map.find(std::string{ name });
                if (it == map.end()) {
                    if (dbg)
                        std::cerr << ". " << std::string{ name } << " not found in Container<" << std::string{ typeid(T).name() } << ">\n";
                    return items.end();
                }
                return items.begin() + it->second;
            }

            T &operator[](const std::string_view name) {
                auto it = map.find(name);
                if (it == map.end()) {
                    throw std::runtime_error("Error: " + std::string{ name } + " not found in Container<" + std::string{ typeid(T).name() } + ">");
                }
                return items[it->second];
            }

            const T &operator[](const std::string_view name) const {
                auto it = map.find(name);
                if (it == map.end()) {
                    throw std::runtime_error("Error: " + std::string{ name } + " not found in Container<" + std::string{ typeid(T).name() } + ">");
                }
                return items[it->second];
            }

            iterator end() {
                return items.end();
            }

            iterator begin() {
                return items.begin();
            }

            const_iterator end() const {
                return items.cend();
            }

            const_iterator begin() const {
                return items.cbegin();
            }

            void clear() {
                items.clear();
                ordered.clear();
                map.clear();
            }

            size_t size() const {
                return items.size();
            }

            void foreach (std::function<void(const String &, T &)> func) const {
                for (auto &item : items) {
                    func(item.name, item);
                    for (const auto &alias : item.aliased) {
                        func(alias, item);
                    }
                }
            }
        };

        template <typename T>
        class DependencySorter
        {
          public:
            struct Item
            {
                T *data = {};
                // Item *parent = {};
                std::vector<Item *>   children;
                std::vector<Item *>   deps;
                std::set<std::string> plats;
                bool                  inserted = {};

                void addDependency(DependencySorter<T> &sorter, const std::string &dep) {
                    auto item = sorter.find(dep);
                    if (!item) {
                        // should not happen
                        std::cerr << "Error: dep not found: " << dep << '\n';
                        return;
                    }
                    bool unique = true;
                    for (const auto &d : deps) {
                        if (d->data->name.original == dep) {
                            unique = false;
                            break;
                        }
                    }
                    if (unique) {
                        deps.push_back(item);
                    }
                    item->add(this);
                }

                void add(Item *d) {
                    if (d->data == data) {
                        return;
                    }
                    for (auto &t : children) {
                        if (t == d) {
                            return;
                        }
                    }
                    children.push_back(d);
                }

                bool hasDepsInserted() const {
                    for (const auto &d : deps) {
                        if (!d->inserted) {
                            return false;
                        }
                    }
                    return true;
                };
            };

            explicit DependencySorter(Container<T> &source) : source(source) {}

            void sort(const std::string &msg, std::function<void(Item &i)> getDependencies) {
                source.ordered.clear();
                source.ordered.reserve(source.size());

                items.reserve(source.size());
                for (auto &i : source.items) {
                    items.push_back(Item{ .data = &i });  // TODO emplace
                }

                for (auto &i : items) {
                    getDependencies(i);
                    /*
                    std::cout << "S: " << i.data->name << "\n";
                    for (auto &d : i.deps) {
                        std::cout << "     " << d->data->name << "\n";
                    }
                    */
                }

                bool empty = false;
                while (!empty) {
                    empty      = true;
                    bool stuck = true;
                    for (auto &i : items) {
                        if (!i.inserted) {
                            if (i.hasDepsInserted()) {
                                source.ordered.push_back(std::ref(*i.data));
                                i.inserted = true;
                                stuck      = false;
                            }
                            empty = false;
                        }
                    }
                    if (!empty && stuck) {
                        std::cerr << "dependcy sort: infinite loop detected" << std::endl;
                        for (auto &i : items) {
                            if (!i.inserted) {
                                std::cout << i.data->name << "\n";
                                for (auto &d : i.deps) {
                                    std::cout << "  " << d->data->name;
                                }
                                std::cout << "\n";
                            }
                        }
                        break;
                    }
                }
            }

          private:
            std::vector<Item> items;
            Container<T>     &source;

            Item *find(const std::string &name) {
                for (auto &i : items) {
                    if (i.data->name.original == name) {
                        return &i;
                    }
                }
                return nullptr;
            };
        };

        using Enum         = vkr::Enum;
        using Struct       = vkr::Struct;
        using Handle       = vkr::Handle;
        using Command      = vkr::Command;
        using Extension    = vkr::Extension;
        using Platform     = vkr::Platform;
        using ClassCommand = vkr::ClassCommand;

        using Commands   = Container<vkr::Command>;
        using Platforms  = Container<vkr::Platform>;
        using Extensions = Container<vkr::Extension>;
        using Tags       = std::unordered_set<std::string>;
        using Handles    = Container<vkr::Handle>;
        using Structs    = Container<vkr::Struct>;
        using Enums      = Container<vkr::Enum>;

      protected:
        struct ErrorClass
        {
            std::string           name;
            const vkr::EnumValue &value;

            ErrorClass(const vkr::EnumValue &value) : name(value.name), value(value) {
                strStripPrefix(name, "eError");
                name += "Error";
            }
        };

        bool defaultWhitelistOption = true;
        bool verbose                = false;
        // int spec = 0;

      public:
        std::string registryPath;
        std::string headerVersion;

        std::vector<ErrorClass> errorClasses;

        Types types;

        Platforms  platforms;  // maps platform name to protect (#if defined PROTECT)
        Extensions extensions;
        Tags       tags;  // list of tags from <tags>

        Commands                                          commands;
        std::vector<std::reference_wrapper<vkr::Command>> staticCommands;

        Structs                         structs;
        Enums                           enums;
        std::vector<vkr::EnumValueType> apiConstants;

        Handles                                          handles;
        std::vector<std::reference_wrapper<vkr::Handle>> topLevelHandles;

        std::vector<xml::Element> unsupportedFeatures;

        std::vector<std::reference_wrapper<vkr::Command>> orderedCommands;
        vkr::Handle                                       loader;

        Registry(Generator &gen);

        // returns filtered prototype name in class context
        String convertCommandName(const std::string &name, const std::string &cls) const {
            String      out{ name, false };
            std::string cname = cls;
            std::string tag   = strRemoveTag(cname);
            if (!cls.empty()) {
                out = std::regex_replace(out, std::regex(cname, std::regex_constants::icase), "");
            }
            if (!tag.empty()) {
                strStripSuffix(out, tag);
            }
            return out;
        }

        std::string strRemoveTag(std::string &str) const;

        std::string strWithoutTag(const std::string &str) const;

        bool strEndsWithTag(const std::string_view str) const;

        std::pair<std::string, std::string> snakeToCamelPair(std::string str) const;

        std::string snakeToCamel(const std::string &str) const;

        std::string enumConvertCamel(const std::string &enumName, std::string value, bool isBitmask = false) const;

        // bool isStructOrUnion(const std::string &name) const;

        bool containsFuncPointer(const vkr::Struct &data) const;

        vkr::Handle &findHandle(const std::string &name) {
            const auto &handle = handles.find(name);
            if (handle == handles.end()) {
                if (name == loader.name.original) {
                    return loader;
                }
                throw std::runtime_error("Handle not found: " + std::string(name));
            }
            return *handle;
        }

        const vkr::Handle &findHandle(const std::string &name) const {
            const auto &handle = handles.find(name);
            if (handle == handles.end()) {
                if (name == loader.name.original) {
                    return loader;
                }
                throw std::runtime_error("Handle not found: " + std::string(name));
            }
            return *handle;
        }

        String getHandleSuperclass(const Handle &data);

      private:
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLElement *root = {};

        std::function<void(void)> onLoadCallback;

        void parsePlatforms(Generator &gen, xml::Element elem, xml::Element children);

        void parseTags(Generator &gen, xml::Element elem, xml::Element children);

        void parseTypes(Generator &gen, xml::Element elem, xml::Element children);

        void parseEnums(Generator &gen, xml::Element elem, xml::Element children);

        void parseApiConstants(Generator &gen, xml::Element elem);

        void parseCommands(Generator &gen, xml::Element elem, xml::Element children);

        void assignCommands(Generator &gen);

        void parseFeature(Generator &gen, xml::Element elem, xml::Element children);

        void parseExtensions(Generator &gen, xml::Element elem, xml::Element children);

        void parseEnumExtend(const xml::Element &elem, Extension *ext, const std::string_view optProtect, const char *spec = {});

        void loadFinished();

        bool loadXML(const std::string &xmlPath);

        void parseXML(Generator &gen);

        void createErrorClasses();

        void disableUnsupportedFeatures();

        void buildDependencies();

      public:
        BaseType &get(const std::string &name) const {
            auto it = types.find(name);
            if (it == types.end()) {
                throw std::runtime_error("Error: " + std::string{ name } + " not found in reg");
            }
            return *it->second;
        }

        BaseType &get(const std::string_view name) const {
            return get(std::string{ name });
        }

        BaseType *find(const std::string &name) const noexcept {
            auto it = types.find(name);
            return it != types.end() ? it->second : nullptr;
        }

        BaseType *find(const std::string_view name) const noexcept {
            return find(std::string{ name });
        }

        void orderStructs();

        void orderHandles();

        static std::string findSystemRegistryPath();
        static std::string findLocalRegistryPath();

        static std::string findDefaultRegistryPath();

        bool isLoaded() const {
            return root != nullptr;
        }

        std::string getRegistryPath() const {
            return registryPath;
        }

        bool load(Generator &gen, const std::string &xmlPath);

        void unload();

        void bindGUI(const std::function<void(void)> &onLoad);
    };

}  // namespace vkgen

#endif  // GENERATOR_REGISTRY_HPP
