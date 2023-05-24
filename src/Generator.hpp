// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//                                                           copies of the Software, and to permit persons to whom the Software is
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

#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include "XMLVariableParser.hpp"
#include "tinyxml2.h"
#include <filesystem>
#include <forward_list>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <string>
#include <set>
#include <string_view>
#include <unordered_set>
#include <vector>

using namespace Utils;

static std::unique_ptr<VariableData> v_placeholder{}; // TODO temporary

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

class Generator;

class Registry {
public:
    using GenFunction = std::function<void(Generator &, XMLNode *)>;
    using OrderPair = std::pair<std::string, GenFunction>;

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
        // RAII_ONLY = 4,
        CREATES_HANDLE = 8,
        CPP_VARIANT = 16
    };

    using Variables = std::vector<std::unique_ptr<VariableData>>;

    struct HandleData;
    struct ExtensionData;
    struct CommandData;

    enum class Type {
        Unknown,
        Enum,
        Struct,
        Union,
        Handle,
        Command
    };

    struct BaseType {
        String name = {""};
        Type type = {};
        ExtensionData *ext = {};
        std::set<BaseType *> dependencies;
        std::set<BaseType *> subscribers;
        bool forceRequired = {};

        BaseType() = default;

        explicit BaseType(Type type) noexcept
            : type(type)
        {
        }

        BaseType(Type type, std::string_view name, bool firstCapital = false)
            : name(std::string{name}, firstCapital), type(type)
        {
        }

        std::string typeString() const noexcept {
            switch (type) {
            case Registry::Type::Enum:
                return "enum";
            case Registry::Type::Struct:
                return "struct";
            case Registry::Type::Union:
                return "union";
            case Registry::Type::Handle:
                return "handle";
            case Registry::Type::Command:
                return "command";
            default:
                return "N/A";
            }
        }

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
                // std::cout << "> enable: " << name << '\n';
            }
            else {
                // std::cout << "> disable: " << name << '\n';
            }

            if (enabled) {
                // std::cout << "    enable: " << name << '\n';
                for (auto &d : dependencies) {
                    if (!subscribers.contains(d)) {
                        d->subscribe(this);
                    }
                }
            }
            else {
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

        bool enabled = false;
        bool supported = true;
    };

    template<typename T>
    class Container {
        static_assert(std::is_base_of<BaseType, T>::value, "T must derive from BaseType");

        std::map<std::string, size_t> map;

    public:

        using iterator = std::vector<T>::iterator;
        using const_iterator = std::vector<T>::const_iterator;

        std::vector<T> items;
        std::vector<std::reference_wrapper<T>> ordered;

        bool contains(const std::string &name) const {
            for (const auto &i : items) {
                if (i.name.original == name) {
                    return true;
                }
            }
            return false;
        }

        void addAlias(const std::string &alias, const std::string &to) {
            auto i = map.find(to);
            if (i == map.end()) {
                std::cerr << "can't add alias: " << to << " not found" << '\n';
                return;
            }
            map.emplace(alias, i->second);
        }

        void prepare() {
            map.clear();
            for (size_t i = 0; i < items.size(); ++i) {
                map.emplace(items[i].name.original, i);
            }
            // std::cout << "prepared container" << '\n';
        }

        const_iterator find(const std::string_view &name) const {
            auto it = map.find(std::string{name});
            if (it == map.end()) {
                return items.end();
            }
            return items.begin() + it->second;
        }

        iterator find(const std::string_view &name, bool dbg = false) {
            auto it = map.find(std::string{name});
            if (it == map.end()) {
                return items.end();
            }
            return items.begin() + it->second;
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

        const std::map<std::string, size_t>& getMap() const {
            return map;
        }

    };

    template<typename T>
    class DependencySorter {
    public:
        struct Item {
            T *data = {};
            Item *parent = {};
            std::vector<Item *> children;
            std::vector<Item *> deps;
            std::set<std::string> plats;
            bool root = {};
            bool inserted = {};

            void addDependency(DependencySorter<T> &sorter, const std::string &dep) {
                auto item = sorter.find(dep);
                if (!item) {
                    // should not happen
                    std::cerr << "Error: dep not found" << '\n';
                    return;
                }
                deps.push_back(item);
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

        explicit DependencySorter(Container<T> &source)
            : source(source)
        {}

        void sort2(const std::string &msg, std::function<void(Item &i)> getDependencies) {
            // std::cerr << "Order NEW " << msg << "\n";

//            for (auto &s : source.items) {
//                std::cout << "  " << s.name << std::endl;
//            }

            if (!source.ordered.empty()) {
                // std::cerr << "container.ordered is not clear" << std::endl;
                source.ordered.clear();
            }
            source.ordered.reserve(source.size());

            items.reserve(source.size());
            for (auto &i : source.getMap()) {
                auto &item = source.items[i.second];
                items.push_back(Item{.data = &item, .root = true});
            }

            for (auto &i : items) {
                getDependencies(i);
                if (!i.deps.empty()) {
                    i.root = false;
                }
            }

            bool empty = false;
            while (!empty) {
                empty = true;
                bool stuck = true;
                for (auto &i : items) {
                    if (!i.inserted) {
                        if (i.hasDepsInserted()) {
                            source.ordered.push_back(std::ref(*i.data));
                            i.inserted = true;
                            stuck = false;
                        }
                        empty = false;
                    }
                }
                if (!empty && stuck) {
                    std::cerr << "dependcy sort: infinite loop detected" << std::endl;
                    break;
                }

            }

//            std::cout << "=== ordered: " << std::endl;
//            for (auto &s : source.ordered) {
//                std::cout << "  " << s.get().name << std::endl;
//            }

        }

        void sort(const std::string &msg, std::function<void(Item &i)> getDependencies) {

            // std::cout << "Order " << msg << "\n";

            if (!source.ordered.empty()) {
                std::cerr << "container.ordered is not clear" << std::endl;
                source.ordered.clear();
            }
            source.ordered.reserve(source.size());

            items.reserve(source.size());
            for (auto &i : source.items) {
                items.push_back(Item{.data = &i, .root = true});
            }

            for (auto &i : items) {
                getDependencies(i);
                if (!i.deps.empty()) {
                    i.root = false;
                }
            }

            for (auto &i : items) {
                for (auto &d : i.children) {
                    for (auto &t : d->children) {
                        if (&i == t) {
                            std::cerr << "[warning] cyclic dependency: " << i.data->name
                                      << " and " << d->data->name << '\n';
                        }
                    }
                }
            }

            bool hasRoot = false;
            for (auto &i : items) {
                if (i.root) {
                    hasRoot = true;
                }
            }
            if (!hasRoot) {
                std::cout << "Error: can't order graph ()" << '\n';
                return;
            }

            for (auto &i : items) {
                if (i.root) {
                    dfs(&i, nullptr);
                }
            }

            const auto order = [&]() {
                for (auto &i : items) {
                    if (i.root) {
                        visit(i, nullptr);
                    }
                }
            };

            int c = 0;
            while (true) {
                c++;
                order();
                bool ok = true;
                for (auto &i : items) {
                    if (!i.inserted) {
                        // std::cerr << "item not inserted: " << i.data->name << std::endl;
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    break;
                }
            }

            // std::cout << "Order " << msg << " done";
            if (c > 1) {
               // std::cout << " in " << c << " iterations";
            }
            //std::cout << "\n";
        }
    private:
        std::vector<Item> items;
        Container<T> &source;

        Item* find(const std::string &name) {
            for (auto &i : items) {
                if (i.data->name.original == name) {
                    return &i;
                }
            }
            return nullptr;
        };

        void visit(Item &i, Item *par) {
            //std::cout << "  visit: " << i.data->name << std::endl;
            if (i.parent == par && !i.inserted && i.hasDepsInserted()) {
                //std::cout << "    ^ insert " << std::endl;
                auto p = i.data->getProtect();

                source.ordered.push_back(std::ref(*i.data));
                i.inserted = true;
            }
            //        std::cout << "    next : ";
            //        for (auto &d : i.children) {
            //            std::cout << d->data->name << ", ";
            //        }
            //        std::cout << std::endl;

            for (auto &d : i.children) {
                visit(*d, &i);
            }
        };

        void dfs (Item *i, Item *parent) {
            i->parent = parent;
            for (auto &d : i->children) {
                dfs(d, i);
            }
        };
    };

    struct PlatformData : public BaseType {
        std::string_view protect;

        PlatformData(const std::string &name, std::string_view protect, bool enabled) :
            protect(protect)
        {
            BaseType::name.reset(name);
            BaseType::enabled = enabled;
        }

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
        bool isBitmask = {};

        EnumData(std::string name, bool isBitmask = false) : BaseType(Type::Enum, name, true), isBitmask(isBitmask)
        {}

        bool containsValue(const std::string &value) const;
    };

    struct ExtensionData : public BaseType {
        std::string protect;
        PlatformData *platform;
        std::vector<CommandData *> commands;
        std::vector<BaseType *> types;

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

    };

    struct StructData : BaseType {
        // XMLNode *node;
        std::string structTypeValue;
        std::vector<String> aliases;
        std::vector<std::string> extends;
        Variables members;
        enum VkStructType { VK_STRUCT, VK_UNION } type;
        bool returnedonly = false;

        StructData(Generator &gen, const std::string_view name, VkStructType type, XMLElement *e);

        StructData(const StructData&o) = delete;

        StructData(StructData &&o) noexcept {
            BaseType::operator=(o);
            structTypeValue = std::move(o.structTypeValue);
            aliases = std::move(o.aliases);
            extends = std::move(o.extends);
            members = std::move(o.members);
            returnedonly = o.returnedonly;
            type = o.type;
        }

        StructData& operator=(const StructData &o) = delete;

        StructData& operator=(StructData &&o) noexcept {
            BaseType::operator=(o);
            structTypeValue = std::move(o.structTypeValue);
            aliases = std::move(o.aliases);
            extends = std::move(o.extends);
            members = std::move(o.members);
            returnedonly = o.returnedonly;
            type = o.type;
            return *this;
        }

        std::string getType() const {
            return type == StructData::VK_STRUCT ? "struct" : "union";
        }
    };


    struct CommandData : public BaseType {
    private:

        void init(Generator &gen);

        void initParams() {
            if (params.size() != _params.size()) {
                params.clear();
                params.reserve(_params.size());
                for (auto &p : _params) {
                    params.push_back(std::ref(*p));
                }
                // std::cout << "cmd prepare clean " << this << '\n';
            }
            else {
                for (size_t i = 0; i < _params.size(); ++i) {
                    params[i] = std::ref(*_params[i]);
                }
            }
        }

    public:
        Variables _params; // command arguments

        std::vector<std::reference_wrapper<VariableData>> params;
        std::vector<std::reference_wrapper<VariableData>> outParams;
        std::string type; // return type
        std::string alias;
        std::vector<std::string> successCodes;
        MemberNameCategory nameCat;
        PFNReturnCategory pfnReturn;
        EnumFlag<CommandFlags> flags{};

        CommandData(Generator &gen, XMLElement *command, const std::string &className) noexcept;

        CommandData(Generator &gen, const CommandData& o, std::string_view alias);

        // copy constuctor
        CommandData(const CommandData&) = delete;
        // move constuctor
        CommandData(CommandData &&o) = default;

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
                flags &= ~EnumFlag<CommandFlags>{bit};
            }
        }

        bool isIndirect() const {
            return flags & CommandFlags::INDIRECT;
        }

        bool canTransform() const {
            return flags & CommandFlags::CPP_VARIANT;
        }

        bool isAlias() const {
            return flags & CommandFlags::ALIAS;
        }

        bool createsHandle() const {
            return flags & CommandFlags::CREATES_HANDLE;
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

        bool destroysObject() const {
            switch (nameCat) {
            case MemberNameCategory::DESTROY:
            case MemberNameCategory::FREE:
                return true;
            default:
                return false;
            }
        }

        bool returnsVector() const {
            for (const VariableData&v : outParams) {
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
                    if (nameCat != MemberNameCategory::GET) {
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
            } catch (std::runtime_error) {
            }
            return true;
        }

        HandleData* secondIndirectCandidate(Generator &gen) const;

        void setName(const Generator &gen, const std::string &name);

        bool containsPointerVariable() {
            for (const VariableData &v : params) {
                if (v.original.isPointer()) {
                    return true;
                }
            }
            return false;
        }

        VariableData* getVar(size_t index) const {
            if (params.size() >= index) {
                return nullptr;
            }
            return &params[index].get();
        }

        VariableData* getLastVar() const {
            if (params.empty()) {
                return nullptr;
            }
            return &params.rbegin()->get();
        }

        VariableData* getFirstVar() const {
            if (params.empty()) {
                return nullptr;
            }
            return &params.begin()->get();
        }

        VariableData* getLastVisibleVar() const {
            for (VariableData &v : params) {
                if (!v.getIgnoreFlag()) {
                    return &v;
                }
            }
            throw std::runtime_error("can't get param (last visible)");
        }

        VariableData* getLastPointerVar() const {
            for (VariableData &v : params) {
                if (v.original.isPointer()) {
                    return &v;
                }
            }
            throw std::runtime_error("can't get param (last pointer)");
        }

        VariableData* getLastHandleVar() const {
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
            throw std::runtime_error("can't get param (last handle)");
        }

        VariableData* getFirstHandleVar() const {
            for (VariableData &v : params) {
                if (v.isHandle()) {
                    return &v;
                }
            }
            throw std::runtime_error("can't get param (first handle)");
        }

    //private:
        bool prepared = false;

    };

    enum class ClassCommandFlags {
        NONE = 0
    };

    // holds information about class member (function)
    struct ClassCommandData {
    private:
    public:
        class Flags : public Enums::Flags<ClassCommandFlags> {
        public:
            using enum ClassCommandFlags;
        };

        const HandleData *cls = {};
        CommandData *src = {};
        String name = {""};
        bool raiiOnly = {};
        Flags flags = {};

        ClassCommandData(const Generator *gen, const HandleData *cls, CommandData &o);

        ClassCommandData &operator=(ClassCommandData &o) noexcept = default;

        bool valid() const { return !name.empty(); }

        operator BaseType() const {
            return *src;
        }
    };

    struct MemberContext {
        Namespace ns = {};
        bool useThis = {};
        bool isStatic = {};
        bool inUnique = {};
        bool generateInline = {};
        bool disableSubstitution = {};
        bool disableDispatch = {};
        bool disableArrayVarReplacement = {};
        bool disableAllocatorRemoval = {};
        bool returnSingle = {};
        bool insertClassVar = {};
        bool insertSuperclassVar = {};
    };

    struct HandleData : public BaseType {

        String superclass;
        VariableData vkhandle;
        std::string ownerhandle;
        std::unique_ptr<VariableData> ownerRaii;
        std::unique_ptr<VariableData> ownerUnique;
        std::unique_ptr<VariableData> secondOwner;
        std::string alias;
        HandleData *parent = {};
        HandleCreationCategory creationCat = HandleCreationCategory::NONE;

        std::optional<ClassCommandData> getAddrCmd;
        std::vector<ClassCommandData> members;
        std::vector<ClassCommandData*> filteredMembers;
        std::vector<ClassCommandData> ctorCmds;
        CommandData* dtorCmd = {};
        std::vector<ClassCommandData> vectorCmds;        

        std::vector<std::reference_wrapper<const VariableData>> vars;

        void foreachVars(VariableData::Flags flags, std::function<void(const VariableData&var)> f) const {
            for (const VariableData &v : vars) {
                if (!hasFlag(v.getFlags(), flags)) {
                    continue;
                }
                f(v);
            }
        }

        int effectiveMembers = 0;
        bool isSubclass = false;
        bool vectorVariant = false;

        HandleData(Generator &gen) : BaseType(Type::Handle), superclass(""), vkhandle(gen, VariableData::TYPE_INVALID)
        {}

        HandleData(Generator &gen, const std::string_view name, const std::string_view parent);

//        HandleData(HandleData&&) = delete;
//        HandleData(const HandleData&) = delete;
//
//        HandleData& operator=(HandleData&&) = delete;
//        HandleData& operator=(const HandleData&) = delete;

        static HandleData& empty(Generator &gen) {
            static HandleData h{gen, "", ""};
            return h;
        }



        void clear() {
            // generated.clear();
//            uniqueVars.clear();
//            raiiVars.clear();
        }

        void init(Generator &gen);

        void prepare(const Generator &gen);

        void addCommand(const Generator &gen, CommandData &cmd, bool raiiOnly = false);

        void setDestroyCommand(const Generator &gen, CommandData &cmd);

        bool hasPFNs() const { return effectiveMembers > 0 && !isSubclass; }

        bool uniqueVariant() const {
            return creationCat != HandleCreationCategory::NONE;
        }    

    };

    using Commands = Container<CommandData>;
    using Platforms = Container<PlatformData>;
    using Extensions = Container<ExtensionData>;
    using Tags = std::unordered_set<std::string>;
    using Handles = Container<HandleData>;
    using Structs = Container<StructData>;
    using Enums = Container<EnumData>;

    struct Macro {
        std::string define;
        std::string value;
        Macro *parent = {};
        bool usesDefine = {};

        Macro(const std::string &define, const std::string &value, bool usesDefine)
            : define(define), value(value), usesDefine(usesDefine)
        {
            // std::cout << this << "  Macro()" << std::endl;
        }

//        Macro(Macro&&) = delete;
//        Macro(const Macro&) = delete;
//
//        Macro& operator=(Macro&&) = delete;
//        Macro& operator=(const Macro&) = delete;
/*
        Macro(Macro&&o) {
            define = o.define;
            value = o.value;
            parent = o.parent;
            usesDefine = o.usesDefine;
            std::cout << this << "  Macro(Macro&&)" << std::endl;
        }
        Macro(const Macro& o) {
            define = o.define;
            value = o.value;
            parent = o.parent;
            usesDefine = o.usesDefine;
            std::cout << this << "  Macro(const Macro&)" << std::endl;
        }

        Macro& operator=(Macro&& o) {
            std::cout << this << "  Macro& operator=(Macro&&)" << std::endl;
            define = o.define;
            value = o.value;
            parent = o.parent;
            usesDefine = o.usesDefine;
            return *this;
        }
        Macro& operator=(const Macro& o) {
            std::cout << this << "  Macro& operator=(const Macro&)" << std::endl;
            define = o.define;
            value = o.value;
            parent = o.parent;
            usesDefine = o.usesDefine;
            return *this;
        }
*/
        const std::string& getDefine() const {
            return usesDefine ? define : value;
        }

        std::string get() const {
            if (parent) {
                return parent->get() + "::" + getDefine();
            }
            return getDefine();
        }

        auto operator<=>(Macro const & ) const = default;
    };

protected:

    std::string registryPath;
    std::string headerVersion;

    std::vector<const EnumValue *> errorClasses;

    Platforms platforms; // maps platform name to protect (#if defined PROTECT)
    Extensions extensions;
    Tags tags; // list of tags from <tags>

    Commands commands;
    std::vector<std::reference_wrapper<CommandData>> staticCommands;
    std::vector<std::reference_wrapper<CommandData>> orderedCommands;

    Structs structs;
    Enums enums;

    Handles handles;
    HandleData loader;

    bool defaultWhitelistOption = true;
    bool verbose = false;

    Registry(Generator &gen);

    // returns filtered prototype name in class context
    String convertCommandName(const std::string &name, const std::string &cls) const {        
        String out{name, false};
        std::string cname = cls;
        std::string tag = strRemoveTag(cname);
        if (!cls.empty()) {
            out = std::regex_replace(
                out, std::regex(cname, std::regex_constants::icase), "");
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

    std::string enumConvertCamel(const std::string &enumName, std::string value,
                                 bool isBitmask = false) const;

    bool isStructOrUnion(const std::string &name) const;

    bool containsFuncPointer(const StructData &data) const;

    void bindVars(const Generator &gen, Variables &vars) const;

    HandleData &findHandle(const std::string &name) {
        if (name == loader.name.original) {
            return loader;
        }
        const auto &handle = handles.find(name);
        if (handle == handles.end()) {
            throw std::runtime_error("Handle not found: " + std::string(name));
        }
        return *handle;
    }

    const HandleData &findHandle(const std::string &name) const {
        if (name == loader.name.original) {
            return loader;
        }
        const auto &handle = handles.find(name);
        if (handle == handles.end()) {
            throw std::runtime_error("Handle not found: " + std::string(name));
        }
        return *handle;
    }

    BaseType *findType(const std::string &name) {
        auto s = structs.find(name);
        if (s != structs.end()) {
            return &*s;
        }
//        for (auto &c : structs) {
//            if (c.second.name.original == name) {
//                return &c.second;
//            }
//        }
        auto e = enums.find(name);
        if (e != enums.end()) {
            return &*e;
        }
//        for (auto &c : enumMap) {
//            if (c.second->name.original == name) {
//                return c.second;
//            }
//        }

        auto h = handles.find(name);
        if (h != handles.end()) {
            return &*h;
        }
//        for (auto &c : handles) {
//            if (c.second.name.original == name) {
//                return &c.second;
//            }
//        }
        return nullptr;
    }

    String getHandleSuperclass(const HandleData &data);

private:

    XMLDocument doc;
    XMLElement *root;

    std::function<void(void)> onLoadCallback;

    void parsePlatforms(Generator &gen, XMLNode *node);

    void parseTags(Generator &gen, XMLNode *node);

    void parseTypes(Generator &gen, XMLNode *node);

    void parseEnums(Generator &gen, XMLNode *node);

    void parseCommands(Generator &gen, XMLNode *node);

    void assignCommands(Generator &gen);

    void parseFeature(Generator &gen, XMLNode *node);

    void parseExtensions(Generator &gen, XMLNode *node);

    void parseEnumExtend(XMLElement &node, ExtensionData *ext);

    void loadFinished();

public:

  void orderStructs(bool usenew = false);

  void orderHandles(bool usenew = false);

    static std::string findDefaultRegistryPath();

    bool isLoaded() const { return root != nullptr; }

    std::string getRegistryPath() const { return registryPath; }

    void load(Generator &gen, const std::string &xmlPath);

    void unload();

    void bindGUI(const std::function<void(void)> &onLoad);

    bool isHandle(const std::string &name) const {
        return handles.find(name) != handles.end() || name == loader.name.original;
    }

};

class Generator : public Registry {

  public:

    using Macro = Registry::Macro;

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
        }

    };

    struct ConfigGroup {
        std::string name;
    };

    struct ConfigGroupMacro : public ConfigGroup {
        ConfigGroupMacro() : ConfigGroup{"macro"} {
            mNamespaceRAII->parent = &mNamespace.data;
        }

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
        ConfigWrapper<bool> expApi{"exp_api", false};
        ConfigWrapper<bool> sort2{"sort2", false};
        ConfigWrapper<bool> internalFunctions{"internal_functions", false};
        ConfigWrapper<bool> internalVkResult{"internal_vkresult", true};
        ConfigWrapper<bool> separatePlatforms{"separate_platforms", true};
        ConfigWrapper<bool> separatePlatformsSingle{"single_platforms_file", true};

        ConfigWrapper<bool> vulkanCommands{"vk_commands", {true}};
        ConfigWrapper<bool> vulkanEnums{"vk_enums", {true}};
        ConfigWrapper<bool> vulkanStructs{"vk_structs", {true}};
        ConfigWrapper<bool> vulkanHandles{"vk_handles", {true}};

        ConfigWrapper<bool> vulkanCommandsRAII{"vk_raii_commands", {true}};
        ConfigWrapper<bool> nodiscard{"nodiscard", {true}};
        ConfigWrapper<bool> dispatchParam{"dispatch_param", {true}};
        ConfigWrapper<bool> dispatchTemplate{"dispatch_template", {true}};
        ConfigWrapper<bool> dispatchLoaderStatic{"dispatch_loader_static", {true}};
        ConfigWrapper<bool> useStaticCommands{"static_link_commands", {false}}; // move
        ConfigWrapper<bool> allocatorParam{"allocator_param", {true}};
        ConfigWrapper<bool> smartHandles{"smart_handles", {true}};
        ConfigWrapper<bool> exceptions{"exceptions", {true}};
        ConfigWrapper<bool> resultValueType{"use_result_value_type", {true}};
        ConfigWrapper<bool> inlineCommands{"inline_commands", {true}};
        ConfigWrapper<bool> orderCommands{"order_command_pointers", {false}};
        ConfigWrapper<bool> structChain{"struct_chain", {true}};
        ConfigWrapper<bool> structConstructor{"struct_constructor", {true}};
        ConfigWrapper<bool> structSetters{"struct_setters", {true}};
        ConfigWrapper<bool> structSettersProxy{"struct_setters_proxy", {true}};
        ConfigWrapper<bool> structReflect{"struct_reflect", {true}};
        ConfigWrapper<bool> structCompareOperators{"struct_compare_operators", {true}};
        ConfigWrapper<bool> unionConstructor{"union_constructor", {true}};
        ConfigWrapper<bool> unionSetters{"union_setters", {true}};
        ConfigWrapper<bool> unionSettersProxy{"union_setters_proxy", {true}};
        ConfigWrapper<bool> staticInstancePFN{"static_instance_pfn", {true}};
        ConfigWrapper<bool> staticDevicePFN{"static_device_pfn", {true}};

        ConfigWrapper<bool> raiiInterop{"raii_interop", {false}};

        auto reflect() const {
            return std::tie(
                cppModules,
                expApi,
                sort2,
                internalFunctions,
                vulkanEnums,
                vulkanStructs,
                vulkanHandles,
                vulkanCommands,
                vulkanCommandsRAII,
                nodiscard,
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
                structChain,
                structConstructor,
                structSetters,
                structSettersProxy,
                structReflect,
                structCompareOperators,
                unionConstructor,
                unionSetters,
                unionSettersProxy,
                raiiInterop,
                staticInstancePFN,
                staticDevicePFN
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

        Config(Config&&) = delete;
        Config(const Config&) = delete;

        Config& operator=(Config&&) = delete;
        Config& operator=(const Config&) = delete;

        void reset() {
            std::cout << "cfg reset begin" << std::endl;
            macro = {};
            gen = {};
            dbg = {};
            std::cout << "cfg reset end" << std::endl;
        }

        auto reflect() const {
            return std::tie(
                macro,
                gen,
                dbg.methodTags
            );
        }

    };

    template<typename T = std::string>
    class UnorderedOutput {
        const Generator &g;

      public:
        std::map<std::string, T> segments;

        UnorderedOutput(const Generator &gen) : g(gen) {}

        std::string get(bool onlyNoProtect = false) const {
            if (onlyNoProtect) {
                std::string o = segments.at("");
                return o;
            }
            std::string out;
            for (const auto &k : segments) {
                // std::cout << ">> uo[" << k.first << "]" << '\n';
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

        UnorderedOutput& operator+=(const std::string& str){
            segments[""] += str;
            return *this;
        }
    };


    class UnorderedFunctionOutput {
        const Generator &g;

        struct Segment {
            std::map<std::string, std::string> guards;

            void add(const std::string &guard, const std::string &code) {
                guards[guard] += code;
            }

            std::string get(const Generator &g) const {
                std::string out;
                for (const auto &k : guards) {
                    out += g.genWithProtectNegate(k.second, k.first);
                }
                return out;
            }
        };

      public:
        std::map<std::string, Segment> segments;

        UnorderedFunctionOutput(const Generator &gen) : g(gen) {}

        void add(const BaseType &type, std::function<void(std::string &)> function, const std::string &guard) {
            if (!type.canGenerate()) {
                return;
            }

            auto [code, protect] = g.genCodeAndProtect(type, function, false);

            segments[protect].add(guard, code);
        }

        void clear() {
            segments.clear();
        }

        UnorderedFunctionOutput& operator+=(const std::string& str){
            segments[""].add("", str);
            return *this;
        }

        std::string get(bool onlyNoProtect = false) const {
            if (onlyNoProtect) {
                auto it = segments.find("");
                if (it == segments.end()) {
                    return "";
                }
                return it->second.get(g);
            }
            std::string out;
            for (const auto &k : segments) {
                // std::cout << ">> uo[" << k.first << "]" << '\n';
                out += g.genWithProtect(k.second.get(g), k.first);
            }
            return out;
        }
    };



    struct GenOutputClass {
        GenOutputClass(const Generator &gen)
            : sFuncs(gen),
              sFuncsEnhanced(gen),
              sPublic(gen),
              sPrivate(gen),
              sProtected(gen)
        {}

        UnorderedFunctionOutput sFuncs;
        UnorderedFunctionOutput sFuncsEnhanced;
        UnorderedFunctionOutput sPublic;
        UnorderedFunctionOutput sPrivate;
        UnorderedFunctionOutput sProtected;
        std::string inherits;
    };

    struct OutputFile {
        std::string filename;
        std::string content;
        Namespace ns = {};

        OutputFile(const std::string &filename, Namespace ns)
            : filename(filename), ns(ns)
        {}

        OutputFile& operator+=(const std::string& str){
            content += str;
            return *this;
        }
    };

    struct GenOutput {

        const std::string prefix;
        const std::string extension;
        const std::filesystem::path path;
        std::map<std::string, OutputFile> files;

        GenOutput(const std::string &prefix, const std::string &extension, std::filesystem::path &path)
            : prefix(prefix), extension(extension), path(path)
        {}

        std::string& addFile(const std::string &suffix, Namespace ns = Namespace::NONE) {
            return addFile(suffix, ns, extension);
        }

        std::string& addFile(const std::string &suffix, Namespace ns, const std::string &extension) {
            auto it = files.try_emplace(suffix, createFilename(suffix, extension), ns);
            return it.first->second.content;
        }

        std::string createFilename(const std::string &suffix, const std::string &extension) const {
            return prefix + suffix + extension;
        }

        std::string createFilename(const std::string &suffix) const {
            return createFilename(suffix, extension);
        }

        const std::string& getFilename(const std::string &suffix) const {
            auto it = files.find(suffix);
            if (it == files.end()) {
                throw std::runtime_error("can't get file: " + suffix);
            }
            return it->second.filename;
        }

        void writeFiles(Generator &gen) {
            for (auto &f : files) {
                writeFile(gen, f.second);
            }
        }

        std::string getFileNameProtect(const std::string &name) const {
            std::string out;
            for (const auto & c : name) {
                if (c == '.') {
                    out += '_';
                }
                else {
                    out += std::toupper(c);
                }
            }
            return out;
        };

        void writeFile(Generator &gen, OutputFile &file);

        void writeFile(Generator &gen, const std::string &filename, const std::string &content, bool addProtect, Namespace ns = Namespace::NONE);
    };

    struct ClassVariables {
        VariableData raiiAllocator;
        VariableData raiiInstanceDispatch;
        VariableData raiiDeviceDispatch;
        VariableData uniqueAllocator;
        VariableData uniqueDispatch;
    };


    Config cfg;
    ClassVariables cvars;
    bool dispatchLoaderBaseGenerated;

    std::string outputFilePath;
    UnorderedFunctionOutput outputFuncs;
    UnorderedFunctionOutput outputFuncsEnhanced;
    UnorderedFunctionOutput outputFuncsRAII;
    UnorderedFunctionOutput outputFuncsEnhancedRAII;
    UnorderedFunctionOutput outputFuncsInterop;
    UnorderedOutput<> platformOutput;

    using GeneratedDestroyMembers = std::unordered_set<std::string_view>;
    std::map<std::string_view, GeneratedDestroyMembers> generatedDestroyCommands;

    std::unordered_map<Namespace, Macro *> namespaces;

    std::unordered_map<std::string, std::unordered_set<std::string>> generatedDestroyOverloads;

    struct Signature {
        std::string name;
        std::string args;

        bool operator==(const Signature&o) const {
            if (name != o.name) {
                return false;
            }
            if (args != o.args) {
                return false;
            }
            return true;
        }
    };

    template <class... Args>
    std::string format(const std::string &format, const Args... args) const;

    std::string genWithProtect(const std::string &code, const std::string &protect) const;

    std::string genWithProtectNegate(const std::string &code, const std::string &protect) const;

    std::pair<std::string, std::string> genCodeAndProtect(const BaseType &type,
                                                          std::function<void(std::string &)> function, bool bypass = false) const;

    std::string genOptional(const BaseType &type,
                            std::function<void(std::string &)> function, bool bypass = false) const;

    std::string genNamespaceMacro(const Macro &m);

    std::string generateDefines();

    std::string generateHeader();

    void generateMainFile(std::string &output);

    void generateFiles(std::filesystem::path path);

    std::string generateModuleImpl();

    void generateEnumStr(const EnumData &data, std::string &output, std::string &to_string_output);

    void generateEnum(const EnumData &data, std::string &output, std::string &to_string_output);

    std::string generateToStringInclude() const;

    void generateEnums(std::string &output, std::string &to_string_output);

    std::string genFlagTraits(const EnumData &data, std::string name, std::string &to_string_output);

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

    std::string generateStructDecl(const StructData &d);

    std::string generateClassDecl(const HandleData &data);

    std::string generateClassDecl(const HandleData &data, const std::string &name);

    std::string generateClassString(const std::string &className,
                                    const GenOutputClass &from, Namespace ns);

    std::string generateForwardInclude(GenOutput &out) const;

    void generateHandles(std::string &output, std::string &output_smart, std::string &output_forward, GenOutput &out);

    void generateUniqueHandles(std::string &output);

    std::string generateStructsInclude() const;

    std::string generateStructs();

    void generateStructChains(std::string &output);

    bool generateStructConstructor(const StructData &data, bool transform, std::string &output);

    std::string generateStruct(const StructData &data);

    std::string generateIncludeRAII(GenOutput &out) const;

    void generateRAII(std::string &output, std::string &output_forward, GenOutput &out);

    void generateFuncsRAII(std::string &output);

    void generateDispatchRAII(std::string &output);

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

    class MemberResolver {

      public: // TODO protected
        using Var = VariableData;

        const Generator &gen;
        const HandleData *cls = {};
        CommandData *cmd = {};
        String name;
        std::string clsname;
        std::string pfnSourceOverride;
        std::string pfnNameOverride;
        std::map<std::string, std::string> varSubstitution;
        std::vector<std::unique_ptr<VariableData>> tempVars;

        std::string returnType;
        std::string returnValue;
        std::string initializer;
        VariableData resultVar;
        VariableData* last = {};
        VariableData* allocatorVar = {};
        std::vector<std::string> usedTemplates;

        std::string guard;

        MemberContext ctx;
        bool constructor = {};
        bool constructorInterop = {};
        bool specifierInline = {};
        bool specifierExplicit = {};
        bool specifierConst = {};
        bool specifierConstexpr = {};
        bool specifierConstexpr14 = {};

        static bool filterProto(const VariableData &v, bool same) {
            return !v.getIgnoreProto();
        }

        static bool filterPass(const VariableData &v, bool same) {
            return !v.getIgnorePass();
        }

        static bool filterRAII(const VariableData &v, bool same) {
            return !v.getIgnorePass() && (!v.getIgnoreFlag() || same);
        }

        static bool filterIngnoreFlag(const VariableData &v, bool same) {
            return !v.getIgnoreFlag();
        }

        bool filterSuperclass(const VariableData &v, bool same) const {
            return v.original.type() == cls->superclass.original;
        }

        static bool filterPFN(const VariableData &v, bool same) { return !v.getIgnorePFN(); }

        std::string createArguments(std::function<bool(const VariableData &, bool)> filter,
                                    std::function<std::string(const VariableData &)> function,
                                    bool proto,
                                    bool pfn) const;

        std::string createArgument(std::function<bool(const VariableData &, bool)> filter,
                                   std::function<std::string (const VariableData &)> function,
                                   bool proto,
                                   bool pfn,
                                   const VariableData &var) const;

        std::string createArgument(const VariableData &var, bool sameType, bool pfn) const;

        void reset() {
            resultVar.setSpecialType(VariableData::TYPE_INVALID);
            usedTemplates.clear();
        }

        std::string declareReturnVar(const std::string &assignment = "") {
            if (!resultVar.isInvalid()) {
                return "";
            }
            resultVar.setSpecialType(VariableData::TYPE_DEFAULT);
            resultVar.setIdentifier("result");
            if (gen.getConfig().gen.internalVkResult) {
                resultVar.setFullType("", "VkResult", "");
            }
            else {
                resultVar.setFullType("", "Result", "");
            }

            std::string out = resultVar.toString();
            if (!assignment.empty()) {
                out += " = " + assignment;
            }
            return out += ";\n";
        }

        virtual std::string generateMemberBody() {
            return "";
        };

        std::string castTo(const std::string &type,
                           const std::string &src) const {
            if (type != cmd->type) {
                return "static_cast<" + type + ">(" + src + ")";
            }
            return src;
        }

        bool useDispatchLoader() const {
            return ctx.ns == Namespace::VK && gen.useDispatchLoader();
        }     

        std::string getDispatchSource() const {
            std::string output = pfnSourceOverride;
            if (output.empty()) {
                if (ctx.ns == Namespace::RAII) {
                    if (cls->name == "Instance" && gen.getConfig().gen.staticInstancePFN) {
                        output += gen.format("{NAMESPACE_RAII}::Instance::dispatcher.");
                    }
                    else if (cls->name == "Device" && gen.getConfig().gen.staticDevicePFN) {
                        output += gen.format("{NAMESPACE_RAII}::Device::dispatcher.");
                    }
                    else {
                        if (!cls->ownerhandle.empty()) {
                            output += cls->ownerhandle + "->getDispatcher()->";
                        } else {
                            output += "m_dispatcher->";
                        }
                    }
                } else {
                    output += gen.getDispatchCall();
                }
            }
            return output;
        }

        std::string getDispatchPFN() const {
            std::string output = getDispatchSource();
            if (pfnNameOverride.empty()) {
                output += cmd->name.original;
            } else {
                output += pfnNameOverride;
            }
            return output;
        }

        std::string generatePFNcall(bool immediateReturn = false) {
            std::string call = getDispatchPFN();
            call += "(" + createPFNArguments() + ")";

            switch (cmd->pfnReturn) {
            case PFNReturnCategory::VK_RESULT:
                call = castTo(gen.getConfig().gen.internalVkResult
                                  ? "VkResult" : "Result", call);
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
                call = castTo(returnType, call);
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
            if (resultVar.isInvalid()) {
                return identifier;
            }
            if (ctx.ns == Namespace::RAII) {
                if (usesResultValue()) {
                    std::string result = resultVar.identifier();
                    if (gen.getConfig().gen.internalVkResult) {
                        result = "static_cast<Result>(" + result + ")";
                    }
                    return "std::make_pair( " + result + ", " + identifier + " )";
                }
                return identifier;
            }

            std::string out;
            if (usesResultValue()) {
                out += "ResultValue<" + returnType + ">";
            }
            else if (usesResultValueType()) {
                out += "createResultValueType";                
            }
            else {
                return identifier;
            }
            std::string args;
            if (resultVar.identifier() != identifier) {
                if (gen.getConfig().gen.internalVkResult) {
                    args += "static_cast<Result>(" + resultVar.identifier() + ")";
                }
                else {
                    args += resultVar.identifier();
                }

            }
            if (!identifier.empty()) {
                if (!args.empty()) {
                    args += ", ";
                }
                args += identifier;
            }

            out += "(" + args + ")";
            return out;
        }

        std::string createCheckMessageString() const {
            std::string message;

            const auto &macros = gen.getConfig().macro;
            const auto &ns = (ctx.ns == Namespace::RAII && !constructorInterop)
                                                       ? macros.mNamespaceRAII
                                                       : macros.mNamespace;
            if (ns->usesDefine) {
                message = ns->define + "_STRING \"";
            } else {
                message = "\"" + ns->value;
            }
            if (!clsname.empty()) {
                message += "::" + clsname;
            }
            message += "::" + name + "\"";
            return message;
        }

        std::string generateCheck() {
            if (cmd->pfnReturn != PFNReturnCategory::VK_RESULT ||
                resultVar.isInvalid()) {
                return "";
            }

            std::string message = createCheckMessageString();
            std::string codes;
            if (returnSuccessCodes() > 1) {
                codes = successCodesList("                ");
            }

            std::string output =
                gen.format(R"(
    resultCheck({0},
                {1}{2});
)",
                               resultVar.identifier(), message, codes);

            return output;
        }

        bool usesResultValue() const {
            if (returnSuccessCodes() <= 1) {
                return false;
            }
            //if (cmd->outParams.size() > 1) {
                for (const VariableData &d : cmd->outParams) {
                    if (d.isArray()) {
                        return false;
                    }
                }
            // }
            return !returnType.empty() && returnType != "Result" && cmd->pfnReturn == PFNReturnCategory::VK_RESULT;
        }

        bool usesResultValueType() const {
            const auto &cfg = gen.getConfig();
            if (!cfg.gen.resultValueType) {
                return false;
            }
            return !returnType.empty() && returnType != "Result" &&
                   cmd->pfnReturn == PFNReturnCategory::VK_RESULT;
        }

        std::string generateReturnType() const {
            if (ctx.ns == Namespace::VK) {
                if (usesResultValue()) {
                    return "ResultValue<" + returnType + ">";
                }
                if (usesResultValueType()) {
                    return "typename ResultValueType<" + returnType + ">::type";
                }
            }
            else {
                if (usesResultValue()) {
                    return gen.format("std::pair<{NAMESPACE}::Result, {0}>", returnType);
                }
            }
            return returnType;
        }

        std::string createReturnType() const {
            if (constructor) {
                return "";
            }

            std::string type;
            std::string str;
            for (const VariableData &p : cmd->outParams) {
                str += p.getReturnType() + ", ";
            }
            strStripSuffix(str, ", ");

            auto count = cmd->outParams.size();
            // cmd->pfnReturn == PFNReturnCategory::OTHER

            if (count == 0) {
                if (cmd->pfnReturn == PFNReturnCategory::OTHER) {
                    type = strStripVk(std::string(cmd->type));
                }
                else {
                    type = "void";
                }
            }
            else if (count == 1) {
                type = str;
            }
            else if (count >= 2) {
                type = "std::pair<" + str + ">";
            }

            if (returnSuccessCodes() > 1) {
                if (type.empty() || type == "void") {
                    type = "Result";
                }
            }

            return type;
        }

        std::string generateNodiscard() {
            const auto &cfg = gen.getConfig();
            if (!cfg.gen.nodiscard) {
                return "";
            }
            if (!returnType.empty() && returnType != "void") {
                return "VULKAN_HPP_NODISCARD ";
            }
            return "";
        }

        std::string getSpecifiers(bool decl) {
            std::string output;
            const auto &cfg = gen.getConfig();
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

        std::string getProto(const std::string &indent, bool declaration, bool &usesTemplate) {
            std::string tag = getDbgtag();
            std::string tag2;
            if (gen.getConfig().dbg.methodTags) {
                if (_indirect) {
                    tag2 += "  <INDIRECT>";
                }
                if (_indirect2) {
                    tag2 += "  <INDIRECT 2>";
                }
                if (cmd->createsHandle()) {
                    tag2 += "  <HANDLE>";
                }
                if (cmd->destroysObject()) {
                    tag2 += "  <DESTROY>";
                }
                if (!tag2.empty()) {
                    tag += "\n// " + tag2 + "\n";
                }
            }
            std::string output;
            if (!tag.empty()) {
                output += indent + tag;
            }

            std::string temp;
            std::string allocatorTemplate;
            for (VariableData &p : cmd->params) {
                const std::string &str = p.getTemplate();
                if (!str.empty()) {
                    temp += "typename " + str;
                    if (declaration) {
                        temp += p.getTemplateAssignment();
                    }
                    temp += ",\n";
                }
            }
            if (!allocatorTemplate.empty()) {
                temp += "typename B0 = " + allocatorTemplate + ",\n";
                strStripSuffix(allocatorTemplate, "Allocator");
                temp += "typename std::enable_if<std::is_same<typename B0::value_type, " + allocatorTemplate + ">::value, int>::type = 0";
            }

            strStripSuffix(temp, ",\n");
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
//                if (constructorInterop) {
//                    output += gen.getConfig().macro.mNamespace->get() + "::";
//                }
                output += clsname + "::";
            }

            output += name + "(" + createProtoArguments(declaration) + ")";
            if (!declaration && !initializer.empty()) {
                output += initializer;
            }
            if (specifierConst && !ctx.isStatic && !constructor) {
                output += " const";
            }
            if (ctx.ns == Namespace::RAII && !temp.empty()) {
                usesTemplate = true;
                return "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES\n" + output;
            }
            usesTemplate = false;

            return output;
        }

        std::string getDbgtag();

        std::string createProtoArguments(bool declaration = false) {
            return createProtoArguments(false, declaration);
        }

        void transformToArray(VariableData &var) {
            auto sizeVar = var.getLengthVar();
            if (!sizeVar) {
                return;
            }

            if (var.isLenAttribIndirect()) {
                sizeVar->setIgnoreFlag(true);
            }
            else {
                sizeVar->setIgnoreProto(true);
                sizeVar->setIgnorePass(true);
            }
            if (var.isArrayIn()) {
                sizeVar->setIgnoreFlag(true);
            }

            if (var.original.type() == "void" && !var.original.isConstSuffix()) {
                bool isPointer = sizeVar->original.isPointer();
                if (isPointer) {
                    var.setFullType("", "uint8_t", "");
                } else {
                    std::string templ = "DataType";
                    if (isInContainter(usedTemplates, templ)) {
                            std::cerr << "Warning: " << "same templates used in " << name << '\n';
                    }
                    usedTemplates.push_back(templ);
                    var.setFullType("", templ, "");
                    var.setTemplate(templ);
                    sizeVar->setIgnoreFlag(false);
                    sizeVar->setIgnoreProto(false);
                }
            }

            if (var.isArrayOut()) {
                var.convertToStdVector();
            }
            else {
                var.convertToArrayProxy();
            }
            if (var.isReference()) {
                std::cerr << "array is reference enabled" << '\n';
            }
        };

        void prepareParams() {

            if (constructor) {
                return;
            }

            for (VariableData &p : cmd->params) {
                const std::string &type = p.original.type();

                if (!p.isArray()) {
                    if (ctx.ns == Namespace::RAII && p.original.type() == cls->superclass.original) {
                        p.setIgnoreProto(true);
                        // p.appendDbg("/*PP()*/");
                    }
                    else if (p.original.type() == cls->name.original) {
                        p.setIgnoreProto(true);
                        // p.appendDbg("/*PP()*/");
                    }
                }
            }
        }

        bool checkMethod() const {

            bool blacklisted = true;
            gen.genOptional(*cmd,
                                [&](std::string &output) {
                                    blacklisted = false;
                                });
            if (blacklisted) {
//                std::cout << "checkMethod: " << name.original
//                              << " blacklisted\n";
                return false;
            }

            return true;
        }

      public:
        std::string dbgtag;
        std::string dbg2;

        bool hasDependencies = {};

        bool _indirect = {};
        bool _indirect2 = {};

        MemberResolver(Generator &gen, ClassCommandData &d, MemberContext &c, bool constructor = {})
            : gen(gen),
              cmd(d.src),
              name(d.name),
              cls(d.cls),
              ctx(c),
              resultVar(gen, VariableData::TYPE_INVALID),
              constructor(constructor)
        {
            _indirect = d.src->isIndirect();
            _indirect2 = d.raiiOnly;
            // std::cout << "MemberResolver" << '\n';
            if (!cmd) {
                std::cerr << "null cmd" << '\n';
                return;
            }
            if (!cls) {
                std::cerr << "null cls" << '\n';
                return;
            }

            if (!cmd->prepared) {
                dbg2 += "// params restore()\n";
            }
            // std::cerr << "MemberResolver() prepare  " << cmd->name.original << '\n';
            cmd->prepare();
            for (auto &p : cmd->outParams) {
                std::stringstream s;
                s << std::hex << &p.get();
                dbg2 += " // OUT: <" + s.str() + ">\n";
            }

            clsname = cls->name;

            returnType = strStripVk(std::string(cmd->type)); // TODO unnecessary?
            dbgtag = "default";
            specifierInline = true;
            specifierExplicit = false;
            specifierConst = true;
            specifierConstexpr = false;
            specifierConstexpr14 = false;

            hasDependencies = checkMethod();

            auto rbegin = cmd->params.rbegin();
            // std::cout << "rbegin" << '\n';
            if (rbegin != cmd->params.rend()) {
                // std::cout << "rbegin get" << '\n';
                last = &rbegin->get();
            }
            else {
                std::cerr << "No last variable" << '\n';
                v_placeholder = std::make_unique<VariableData>(gen, VariableData::TYPE_PLACEHOLDER);
                last = v_placeholder.get();
            }

            // std::cout << "prepare params" << '\n';
            prepareParams();
            // std::cout << "prepare params done" << '\n';

            const auto &cfg = gen.getConfig();
            bool dispatch = cfg.gen.dispatchParam && ctx.ns == Namespace::VK && !ctx.disableDispatch;

            for (VariableData &p : cmd->params) {
                if (p.original.type() == "VkAllocationCallbacks") {
                    allocatorVar = &p;
                    // std::cout << "[[ a: " << gen.getConfig().gen.allocatorParam << std::endl;
                    if (!cfg.gen.allocatorParam) {
                        allocatorVar->setIgnoreProto(true);
                    }
                }
            }

            if (ctx.insertClassVar) {
                VariableData &var = addVar(cmd->params.begin(), cls->name);
                var.toRAII();
                var.setIgnorePFN(true);
                var.setIgnoreFlag(true);
                var.setIgnoreProto(true);
            }

            if (ctx.insertSuperclassVar) {
                VariableData &var = addVar(cmd->params.begin(), cls->superclass);
                var.toRAII();
                var.setIgnorePFN(true);
                var.setDbgTag("(P)");
            }

            if (dispatch) {
                std::string type = gen.getDispatchType();
                // std::cout << "add dispatch var" << '\n';
                VariableData &var = addVar(cmd->params.end());
                var.setSpecialType(VariableData::TYPE_DISPATCH);
                var.setFullType("", type, " const &");
                var.setIdentifier("d");
                var.setIgnorePFN(true);                
                var.setOptional(true);
                if (cfg.gen.dispatchTemplate) {
                    var.setTemplate(type);
                    var.setTemplateAssignment(" = VULKAN_HPP_DEFAULT_DISPATCHER_TYPE");
                }
                std::string assignment = cfg.macro.mDispatch->get();
                if (!assignment.empty()) {
                    var.setAssignment(" " + assignment);
                }

            }

            cmd->prepared = false;
            // std::cout << "MemberResolver done" << '\n';
        }

        virtual ~MemberResolver() {

        }

        virtual void generate(UnorderedFunctionOutput &decl, UnorderedFunctionOutput &def);

        template<typename... Args>
        VariableData& addVar(decltype(cmd->params)::iterator pos,  Args&&... args) {
            auto &var = tempVars.emplace_back(std::make_unique<VariableData>(gen, args...));
            cmd->params.insert(pos, std::ref(*var.get()));
            cmd->prepared = false;
            return *var.get();
        }

        void disableFirstOptional() {
            for (Var &p : cmd->params) {
                if (p.isOptional()) {
                    p.setOptional(false);
                    break;
                }
            }
        }

        int returnSuccessCodes() const { // TODO rename
            int count = 0;
            for (const auto & c : cmd->successCodes) {
                if (c == "VK_INCOMPLETE") {
                    continue;
                }
                count++;
            }
            return count;
        }

        std::string createArgumentWithType(const std::string &type) const;

        std::string successCodesCondition(const std::string &id, const std::string &indent = "      ") const;

        std::string successCodesList(const std::string &indent) const;

        bool isIndirect() const { return cls->isSubclass; }

        Signature createSignature() const {
            Signature sig;
            sig.name = name;
            std::string sep;
            for (const VariableData &p : cmd->params) {
                bool sameType = p.original.type() == cls->name.original && !p.original.type().empty();

                if (!filterProto(p, sameType)) {
                    continue;
                }

                sig.args += p.prefix() + p.type() + p.suffix() + sep;
                sep = ", ";
            }
            return sig;
        }

        std::string createProtoArguments(bool useOriginal = false, bool declaration = false) const {
            std::string output;
            if (gen.getConfig().dbg.methodTags) {
                output = "/*{Pargs}*/";
            }
            output += createArguments(
                                       filterProto,
                                       [&](const VariableData &v) {
                                           if (useOriginal) {
                                               return v.originalToString();
                                           }
                                           if (declaration) {
                                               return v.toStringWithAssignment();
                                           }
                                           return v.toString();
                                       },
                                       true, false);
            return output;
        }

        std::string createPFNArguments(bool useOriginal = false) const {
            std::string output;
            if (gen.getConfig().dbg.methodTags) {
                output = "/*{PFNargs}*/";
            }
            output += createArguments(
                                         filterPFN,
                                         [&](const VariableData &v) { return v.toArgument(useOriginal); },
                                         false, true);
            return output;
        }

        std::string createPassArgumentsRAII() const {
            std::string output;
            if (gen.getConfig().dbg.methodTags) {
                output = "/*{RAIIargs}*/";
            }
            output += createArguments(
                                          filterRAII,
                                          [&](const VariableData &v) { return v.identifier(); },
                                          false, false);
            return output;
        }

        std::string createPassArguments(bool hasAllocVar) const {
            std::string output;
            if (gen.getConfig().dbg.methodTags) {
                output = "/*{PASSargs}*/";
            }
            output += createArguments(
                                          filterPass,
                                          [&](const VariableData &v) { return v.identifier(); },
                                          false, false);
            return output;
        }

        std::string createStaticPassArguments(bool hasAllocVar) const {
            std::string output;
            if (gen.getConfig().dbg.methodTags) {
                output = "/*{PASSargsStatic}*/";
            }
            output += createArguments(
                filterPass,
                [&](const VariableData &v) { return v.identifier(); },
                false, true);
            return output;
        }

//        std::string createPassArguments2() const {
//            return "/*{PASSargs2}*/" + createArguments(
//                                           filterPFN,
//                                           [&](const VariableData &v) { return v.identifier(); },
//                                           false, false);
//            return output;
//        }

        std::vector<std::reference_wrapper<VariableData>> getFilteredProtoVars() const {
            std::vector<std::reference_wrapper<VariableData>> out;
            for (Var &p : cmd->params) {
                if (filterProto(p, p.original.type() == cls->name.original)) {
                    out.push_back(std::ref(p));
                }
            }
            return out;
        }

        std::string createArgument(const VariableData &var, bool useOrignal) const {
            bool sameType = var.original.type() == cls->name.original && !var.original.type().empty();
            return createArgument(var, sameType, useOrignal);
        }


//        bool returnsVector() const {
//            return last->getSpecialType() == VariableData::TYPE_VECTOR;
//        }

        bool returnsTemplate() {
            return !last->getTemplate().empty();
        }

        void setGenInline(bool value) {
            ctx.generateInline = value;
        }

        void addStdAllocators() {

            auto rev = cmd->params.rbegin();
            if (rev != cmd->params.rend() && rev->get().getSpecialType() == VariableData::TYPE_DISPATCH) {
                rev++;
            }

            if (rev == cmd->params.rend()) {
                std::cerr << "can't add std allocators (no params) " << name.original << '\n';
                return;
            }

            decltype(cmd->params)::iterator it = rev.base();

            for (VariableData &v : cmd->outParams) {
                if (v.isArrayOut()) {
                    const std::string &type = strFirstUpper(v.type()) + "Allocator";
                    VariableData &var = addVar(it);
                    var.setSpecialType(VariableData::TYPE_STD_ALLOCATOR);
                    var.setFullType("", type, " &");
                    var.setIdentifier(strFirstLower(type));
                    var.setIgnorePFN(true);
                    var.setIgnorePass(true);
                    var.setTemplate(type);
                    var.setTemplateAssignment(" = std::allocator<" + last->type() + ">");

                    v.setStdAllocator(var.identifier());

                }
            }

        }

        std::string generateDeclaration();

        std::string generateDefinition(bool genInline, bool bypass = false);

        bool compareSignature(const MemberResolver &o) const {
            const auto removeWhitespace = [](std::string str) {
                return std::regex_replace(str, std::regex("\\s+"), "");
            };

            const auto getType = [&](const VariableData &var) {
                std::string type = removeWhitespace(var.type());
                std::string suf = removeWhitespace(var.suffix());
                return type + " " + suf;
            };

            auto lhs = getFilteredProtoVars();
            auto rhs = o.getFilteredProtoVars();
            if (lhs.size() == rhs.size()) {
                std::string str;
                for (size_t i = 0; i < lhs.size(); i++) {
                    std::string l = getType(lhs[i]);
                    std::string r = getType(rhs[i]);
                    if (l != r) {                        
                        return false;
                    }
                    str += l + ", ";
                }
                std::cout << "sig match: " << str << '\n';
                return true;
            }
            return false;
        }
    };

    class MemberResolverDefault : public MemberResolver {

      private:
        void transformMemberArguments() {
            auto &params = cmd->params;

            const auto convertName =
                [](VariableData &var) {
                    const std::string &id = var.identifier();
                    if (id.size() >= 2 && id[0] == 'p' && std::isupper(id[1])) {
                        var.setIdentifier(strFirstLower(id.substr(1)));
                    }
                };

            for (VariableData &p : params) {
                if (ctx.returnSingle && p.isArrayOut()) {
                    continue;
                }
                transformToArray(p);
            }

            for (VariableData &p : params) {
                const std::string &type = p.original.type();

                if (p.isArray()) {
                    convertName(p);
                    continue;
                }
                if (gen.isStructOrUnion(type)) {
                    convertName(p);
                    if (!p.isOutParam()) {
                        p.convertToReference();
                        //dbg2 += "// conv: " + p.identifier() + "\n";
                        p.setConst(true);
                    }
                }

                if (p.isOptional() && !p.isOutParam()) {
                    if (!p.isPointer() && p.original.isPointer()) {
                       p.convertToOptionalWrapper();
                    }
                }
            }

            if (ctx.ns == Namespace::RAII && cmd->createsHandle()) {
                const auto &var = cmd->getLastHandleVar();
                if (var) {
                    if (name.original != "vkGetSwapchainImagesKHR") {
                        var->toRAII();
                        var->setDbgTag(var->getDbgTag() + "(toR1)");
                    }

                    auto &h = gen.findHandle(var->original.type());
                    bool converted = false;
                    if (h.ownerRaii) {
                        for (VariableData &p : params) {
                            if (p.original.type() == h.ownerRaii->original.type()) {
                                p.toRAII();
                                p.setDbgTag(p.getDbgTag() + "(toR2)");
                                converted = true;
                                break;
                            }
                        }
                    }
                    if (!converted) {
                        if (h.parent) {
                            for (VariableData &p : params) {
                                if (p.original.type() == h.parent->name.original) {
                                    p.toRAII();
                                    p.setDbgTag(p.getDbgTag() + "(toR3)");
                                    break;
                                }
                            }
                        }
                    }
                }
                else {
                    std::cerr << "warning: " << name.original << " creates handle but var was not found" << '\n';
                }
            }

            bool toTemplate = false;
            for (VariableData &p : cmd->outParams) {
                p.removeLastAsterisk();
                // std::cerr << "transformMemberArguments set" << '\n';
                p.setIgnoreProto(true);
                p.setIgnorePass(true);
                p.setConst(false);
                auto &vars = p.getArrayVars();
                if (vars.empty()) {
                    if (p.fullType() == "void") {
                        if (toTemplate) {
                            std::cerr << "Warning: multile void returns" << '\n';
                        }

                        std::string templ = "DataType";
                        if (isInContainter(usedTemplates, templ)) {
                            std::cerr << "Warning: " << "same templates used in " << name << '\n';
                        }
                        usedTemplates.push_back(templ);
                        p.setType(templ);
                        p.setTemplate(templ);
                        toTemplate = true;
                    }
                }
            }

            returnType = createReturnType();
        }

        void setOptionalAssignments() {

            bool assignment = true;
            for (auto it = cmd->params.rbegin(); it != cmd->params.rend(); it++) {
                auto &v = it->get();
                if (v.getIgnoreProto()) {
                    continue;
                }                
                if (v.getSpecialType() == VariableData::TYPE_DISPATCH) {
                    continue;
                }

                if (v.isOptional()) {
                    if (v.isPointer()) {
                        v.setAssignment(" VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT");
                    }
                    else {
                        if (v.original.isPointer()) {
                            v.setAssignment(" VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT");
                        }
                        else {
                            v.setAssignment(" VULKAN_HPP_DEFAULT_ARGUMENT_ASSIGNMENT");
                            // if (v->isReference() && !v->isConst())
                            // integrity check?
                        }

                    }
                }
                if (!assignment) {
                    v.setAssignment("");
                }
                if (v.getAssignment().empty()) {
                    assignment = false;
                }
            }
        }

      protected:      

        std::string getSuperclassArgument(const String &superclass) const {
            std::string output;
            for (const VariableData &p : cmd->params) {
                if (!p.getIgnoreProto() && p.original.type() == superclass.original) {
                    output = p.identifier();
                }
            }
            if (output.empty()) {
                if (superclass == cls->superclass) {
                    output = "*m_" + strFirstLower(superclass);
                }
                else if (superclass == cls->name){
                    output = "*this";
                }
                else {
                    std::cerr << "warning: can't create superclass argument" << '\n';
                }
            }

            if (gen.getConfig().dbg.methodTags) {
                return "/*{SC " + superclass + ", " + cls->superclass + " }*/" + output;
            }
            return output;
        }

        virtual std::string generateMemberBody() override {

            std::string output;
            const auto &cfg = gen.getConfig();
            const bool dbg = cfg.dbg.methodTags;
            if (dbg) {
                output += "/* MemberResolverDefault */\n";
            }
            bool immediate = returnType != "void" && cmd->pfnReturn != PFNReturnCategory::VOID && cmd->outParams.empty() && !usesResultValueType();

            bool arrayVariation = false;
            bool returnsRAII = false;
            bool hasPoolArg = false;
            std::string returnId;
            VariableData *vectorSizeVar = {};            
            VariableData *inputSizeVar = {};
            if (!cmd->outParams.empty()) {

                std::set<VariableData*> countVars;
                for (const VariableData &v : cmd->outParams) {
                    // output += "/*Var: " + v.fullType() + "*/\n";
                    if (v.isArray()) {
                        const auto var = v.getLengthVar();
                        if (!v.isLenAttribIndirect()) {
                            arrayVariation = true;

                            if (!var->getIgnoreProto()) {
                                inputSizeVar = var;

                                auto t = v.getTemplate();
                                if (!t.empty()) {
                                    //std::cerr << "TT: " << t << '\n';

                                    output += gen.format("    VULKAN_HPP_ASSERT( {0} % sizeof( {1} ) == 0 );\n",
                                                             var->identifier(), t);
                                }
                            }

                            if (!countVars.contains(var)) {
                                countVars.insert(var);
                                var->removeLastAsterisk();
                                vectorSizeVar = var;
                            }
                        }
                        if (v.getNamespace() == Namespace::RAII) {
                            returnsRAII = true;
                            if (dbg) {
                                output += "/*RTR*/\n";
                            }
                        }
                    }
                }

                if (countVars.size() > 1) {
                    std::cerr << "generate member: multiple count vars in " << name.original << '\n';
                }

                for (const VariableData &v : cmd->params) {
                    if (v.isArrayIn()) {
                        const auto &var = v.getLengthVar();
                        if (countVars.contains(&(*var))) {
                            inputSizeVar = &(*var);                            
                            break;
                        }
                    }
                }

                if (cmd->outParams.size() > 1) {
                    output += "    " + returnType + " data";
                    if (returnsRAII) {
                        std::cerr << "warning: unhandled return RAII type " << name << '\n';
                    }
                    returnId = "data";
                    if (arrayVariation || inputSizeVar) {
                        if (cmd->outParams.size() > 1) {                            
                            std::vector<std::string> init;
                            bool hasInitializer = false;
                            for (const VariableData &v : cmd->outParams) {
                                std::string i = v.getLocalInit();
                                if (!i.empty()) {
                                    hasInitializer = true;
                                }
                                init.push_back(i);
                            }
                            if (hasInitializer) {
                                output += "( std::piecewise_construct";
                                for (const auto &i : init) {
                                    output += ", std::forward_as_tuple( ";
                                    output += i.empty()? "0" : i;
                                    output += " )";
                                }
                                output += " )";
                                if (dbg) {
                                    output += "/*L2*/";
                                }
                            }
                        }
                        else {
                            const auto &init = cmd->outParams[0].get().getLocalInit();
                            if (!init.empty()) {
                                output += "( " + init + " )";
                                if (dbg) {
                                    output += "/*L2*/";
                                }
                            }
                        }
                    }
                    if (dbg) {
                        output += "/*pair def*/";
                    }
                    output += ";\n";

                    std::string init = "data.first";
                    for (VariableData &v : cmd->outParams) {
                        v.createLocalReferenceVar("    ", init, output);
                        init = "data.second";
                    }
                }
                else {
                    VariableData &v = cmd->outParams[0];
                    returnId = v.identifier();
                    v.createLocalVar("    ", dbg? "/*var def*/" : "", output);
                }
                for (VariableData *v : countVars) {
                    if (v->getIgnoreProto() && !v->getIgnoreFlag()) {
                        v->createLocalVar("    ", dbg? "/*count def*/" : "", output);
                    }
                }

                if (cmd->pfnReturn == PFNReturnCategory::VK_RESULT) {
                    output += "    " + declareReturnVar();
                }
            }

            if (arrayVariation && !inputSizeVar) {

                std::string id = cmd->outParams[0].get().identifier();
                std::string size = vectorSizeVar->identifier();
                std::string call = generatePFNcall();

                std::string resizeCode;
                for (VariableData &v : cmd->outParams) {
                    if (v.isArray()) {
                        v.setAltPFN("nullptr");
                        resizeCode += "        " + v.identifier() + ".resize( " + size + " );\n";
                    }
                }

                std::string callNullptr = generatePFNcall();

                if (cmd->pfnReturn == PFNReturnCategory::VK_RESULT) {
                    std::string cond = successCodesCondition(resultVar.identifier());

                    std::string resultSuccess = cfg.gen.internalVkResult? "VK_SUCCESS" : "Result::eSuccess";
                    std::string resultIncomplete = cfg.gen.internalVkResult? "VK_INCOMPLETE" : "Result::eIncomplete";

                    output += gen.format(R"(
    do {
      {0}
      if (result == {1} && {2}) {
)",                 callNullptr, resultSuccess, size);

                    output += resizeCode;
                    output += gen.format(R"(
        {0}
      }
    } while (result == {1});
)",                 call, resultIncomplete);

                } else {
                    output += "    " + callNullptr + "\n";
                    output += resizeCode;
                    output += "    " + call + "\n";
                }

                output += generateCheck();

                output += gen.format("    if ({0} < {1}.size()) {\n", size, id);
                output += resizeCode;
                output += "    }\n";

            }
            else {
                output += "    " + generatePFNcall(immediate && !constructor) + "\n";
                if (cmd->pfnReturn != PFNReturnCategory::VOID) {
                    output += generateCheck();
                }
            }

            const auto createEmplaceRAII = [&](){
                std::string output;
                for (const VariableData &v : cmd->outParams) {
                    const std::string &id = v.identifier();
                    const auto &handle = gen.findHandle(v.original.type());

                    if (!constructor) {
                        output += "    " + v.getReturnType() + " _" + id + ";";
                        if (dbg) {
                            // output += "/*var ref*/";
                        }
                        output += '\n';
                    }
                    std::string iter = strFirstLower(v.type());
                    std::string dst = constructor ? "this->" : "_" + id + ".";
                    std::string arg = getSuperclassArgument(handle.superclass);
                    std::string poolArg;
                    if (handle.secondOwner) {
                        poolArg += ", " + createArgumentWithType(
                                              handle.secondOwner->type());
                        if (dbg) {
                            output += "/*pool: " + poolArg + " */\n";
                        }
                        hasPoolArg = true;
                    }

                    output += gen.format(R"(
    {0}reserve({1}.size());
    for (auto const &{2} : {3}) {
      {0}emplace_back({4}, {2}{5});
    }
)",
                                         dst, id, iter, id, arg, poolArg);
                }

                return output;
            };

            if (returnsRAII) {
                if (cmd->outParams.size() > 1) {
                    std::cerr << "Warning: unhandled returning multiple RAII types. " << name << '\n';
                }
                /*
                for (const VariableData &v : cmd->outParams) {
                    const std::string &id = v.identifier();
                    const auto &handle = gen.findHandle(v.original.type());

                    if (!constructor) {
                        output += "    " + v.getReturnType() + " _" + id + ";";
                        if (dbg) {
                            // output += "//var ref";
                        }
                        output += '\n';
                    }
                    std::string iter = strFirstLower(v.type());
                    std::string dst = constructor? "this->" : "_" + id + ".";
                    std::string arg = getSuperclassArgument(handle.superclass);
                    std::string poolArg;
                    if (handle.secondOwner) {
                        poolArg += ", " + createArgumentWithType(handle.secondOwner->type());
                        if (dbg) {
                            output += "// pool: " + poolArg + " \n";
                        }
                        hasPoolArg = true;                        
                    }

                    output += gen.format(R"(
    {0}reserve({1}.size());
    for (auto const &{2} : {3}) {
      {0}emplace_back({4}, {2}{5});
    }
)",                     dst, id, iter, id, arg, poolArg);

                }*/

                const VariableData &v = cmd->outParams[0];
                output += createEmplaceRAII();
                returnId = "_" + v.identifier();
            }

            if (!returnId.empty() && !immediate && !constructor) {
                output += "    return " + generateReturnValue(returnId) + ";";
                if (dbg) {
                    // output += "/*.R*/";
                }
                output += '\n';
            }

            const auto createInternalCall = [&](){
                auto &var = cmd->outParams[0].get();
                std::string type  = var.namespaceString(true) + var.type();
                std::string ctype = var.original.type();
                var.setIgnorePFN(true);
                var.getLengthVar()->setIgnorePFN(true);

                std::string func = "createArray";
                if (cmd->pfnReturn == PFNReturnCategory::VOID) {
                    func += "VoidPFN";
                }

                std::string pfn = getDispatchPFN();
                std::string msg = createCheckMessageString();
                std::string pfnType = "PFN_" + name.original;
                std::string sizeType = var.getLengthVar()->type();
                // output = "#if 0\n" + output + "#endif\n";
                std::string output;
                output += "internal::" + func;
                output += "<" + type + ", " + ctype + ", " + sizeType + ", " + pfnType + ">";
                output += "(" + pfn + ", " + msg;

                for (const VariableData &p : cmd->params) {
                    if (!p.getIgnorePFN()) {
                        output += ", " + createPFNArguments();
                        break;
                    }
                }
                output += ")";
                return output;
            };

            if (arrayVariation && gen.getConfig().gen.internalFunctions && cmd->outParams.size() == 1) {

                if (returnsRAII) {
                    if (hasPoolArg) {
////                        output += "// TODO createHandlesWithPoolRAII " + t + "\n";
                    }
                    else {
                        if (!inputSizeVar && !usesResultValue() && cmd->outParams.size() == 1) {
                            VariableData &var = cmd->outParams[0];
                            output = "";
                            // output += "// TODO createHandlesRAII \n";
                            var.createLocalVar("    ", dbg? "/*var def*/" : "", output, createInternalCall());
                            output += createEmplaceRAII();
                            if (!returnId.empty() && !immediate && !constructor) {
                                output += "    return " + generateReturnValue(returnId) + ";";
                                if (dbg) {
                                    // output += "/*.R*/";
                                }
                                output += '\n';
                            }
                        }
                    }
                }
                else {

                    if (!inputSizeVar && !usesResultValue()) {
                        /*
                        auto &var = cmd->outParams[0].get();
                        std::string type  = var.namespaceString() + var.type();
                        std::string ctype = var.original.type();
                        var.setIgnorePFN(true);
                        var.getLengthVar()->setIgnorePFN(true);

                        std::string func = "createArray";
                        if (cmd->pfnReturn == PFNReturnCategory::VOID) {
                            func += "VoidPFN";
                        }

                        std::string pfn = getDispatchPFN();
                        std::string msg = createCheckMessageString();
                        std::string pfnType = "PFN_" + name.original;
                        std::string sizeType = var.getLengthVar()->type();
                        // output = "#if 0\n" + output + "#endif\n";
                        output = "";
                        output += "    return " "internal::" + func;
                        output += "<" + type + ", " + ctype + ", " + sizeType + ", " + pfnType + ">";
                        output += "(" + pfn + ", " + msg;

                        for (const VariableData &p : cmd->params) {
                            if (!p.getIgnorePFN()) {
                                output += ", " + createPFNArguments();
                                break;
                            }
                        }
                        output += ");\n";
                         */
                        output = "";
                        output += "    return ";
                        output += createInternalCall();
                        output += ";\n";
                    }
                }
            }

            return output;
        }

      public:
        MemberResolverDefault(Generator &gen, ClassCommandData &d, MemberContext &ctx, bool constructor = {}) : MemberResolver(gen, d, ctx, constructor) {
            // std::cerr << "MemberResolverDefault()  " << d.src->name.original << '\n';
            transformMemberArguments();
        }

        virtual ~MemberResolverDefault() {}

        virtual void generate(UnorderedFunctionOutput &decl, UnorderedFunctionOutput &def) override {
            // std::cerr << "MemberResolverDefault::generate  " << cmd->name.original << '\n';
            setOptionalAssignments();
            MemberResolver::generate(decl, def);

            // std::cerr << "MemberResolverDefault::generate  " << cmd->name.original << "  done " << '\n';
        }
    };

    class MemberResolverDbg final : public MemberResolverDefault {
      public:
        MemberResolverDbg(Generator &gen, ClassCommandData &d, MemberContext &ctx) : MemberResolverDefault(gen, d, ctx) {
            dbgtag = "disabled";
        }

        virtual void generate(UnorderedFunctionOutput &decl, UnorderedFunctionOutput &def) override {
            decl += "/*\n";
            decl += generateDeclaration();
            decl += "*/\n";
        }
    };

    class MemberResolverStaticDispatch final : public MemberResolver {

      public:
        MemberResolverStaticDispatch(Generator &gen, ClassCommandData &d, MemberContext &ctx)
            : MemberResolver(gen, d, ctx)
        {
            returnType = cmd->type;
            name = name.original;
            dbgtag = "static dispatch";
        }

        std::string temporary() const {
            std::string protoArgs = createProtoArguments(true);
            std::string args = createStaticPassArguments(true);
            std::string proto = cmd->type + " " + name.original + "(" + protoArgs + ")";
            std::string call;
            if (cmd->pfnReturn != PFNReturnCategory::VOID) {
                call += "return ";
            }

            call += "::" + name + "(" + args + ");";

            return gen.format(R"(
    {0} const {NOEXCEPT} {
      {1}
    }
)",
                proto, call);
        }

        virtual std::string generateMemberBody() override {
            return "";
        }
    };

    class MemberResolverClearRAII final : public MemberResolver {

      public:
        MemberResolverClearRAII(Generator &gen, ClassCommandData &d, MemberContext &ctx)
            : MemberResolver(gen, d, ctx)
        {

            for (VariableData &p : cmd->params) {
                p.setIgnoreFlag(true);
                p.setIgnoreProto(true);
                if (p.type() == "uint32_t") {
                    p.setAltPFN("1");
                }
            }

            dbgtag = "raii clear";
        }

        std::string temporary(const std::string &handle) const {
            std::string call;
            call = "      if (" + handle + ") {\n";
            std::string src = getDispatchSource();
            call += "        " + src + cls->dtorCmd->name.original + "(" +
                    createPFNArguments() + ");\n";
            call += "      }\n";
            return call;
        }

        virtual std::string generateMemberBody() override {
            return "";
        }
    };

    class MemberResolverPass final : public MemberResolver {

      public:
        MemberResolverPass(Generator &gen, ClassCommandData &d, MemberContext &ctx)
            : MemberResolver(gen, d, ctx)
        {
            ctx.disableSubstitution = true;
            dbgtag = "pass";
        }

        virtual std::string generateMemberBody() override {
            return "    " + generatePFNcall(true) + "\n";
        }
    };

    class MemberResolverVectorRAII final : public MemberResolverDefault {
      protected:

      public:
        MemberResolverVectorRAII(Generator &gen, ClassCommandData &d, MemberContext &refCtx)
            : MemberResolverDefault(gen, d, refCtx) {

            // cmd->pfnReturn = PFNReturnCategory::VOID;
            ctx.useThis = true;

            dbgtag = "RAII vector";
        }

        virtual std::string generateMemberBody() override {
            std::string args = createPassArguments(true);
            return "    return " + last->namespaceString() + last->type() +
                   "s(" + args + ");\n";
        }
    };

    class MemberResolverCtor : public MemberResolverDefault {
      protected:
        String _name;
        std::string src;
        bool ownerInParent;

        struct SuperclassSource {
            std::string src;
            bool isPointer = {};
            bool isIndirect = {};

            std::string getSuperclassAssignment() const {
                std::string out;
                if (isIndirect || !isPointer) {
                    out += "&";
                }
                out += src;
                return out;
            }

            std::string getDispatcher() const {
                std::string out = src;
                if (!isIndirect && isPointer) {
                    out += "->";
                }
                else {
                    out += ".";
                }
                out += "getDispatcher()";
                return out;
            }

        } superclassSource;

      public:
        MemberResolverCtor(Generator &gen, ClassCommandData &d, MemberContext &refCtx)
            : MemberResolverDefault(gen, d, refCtx, true), _name("") {

            _name = gen.convertCommandName(name.original, cls->superclass);
            name = std::string(cls->name);

            if (cmd->params.empty()) {
                std::cerr << "error: MemberResolverCtor" << '\n';
                hasDependencies = false;
                return;
            }



            VariableData *parent = &cmd->params.begin()->get();

            ownerInParent = false;
            if (parent->original.type() != cls->superclass.original) {
                ownerInParent = true;
            }

            std::string id = parent->identifier();

            pfnSourceOverride = id;
            if (ownerInParent) {
                pfnSourceOverride += ".get" + cls->superclass + "()";
            }

            bool hasParentArgument = false;
            for (const VariableData &p : cmd->params) {
                if (p.original.type() == cls->superclass.original) {
                    // p->setIgnorePass(true);
                    src = p.identifier();
                } else if (cls->parent &&
                           p.original.type() == cls->parent->name.original) {
                    // this handle is not top level
                    if (src.empty()) {
                        src = p.identifier() + ".get" + cls->superclass + "()";
                        ownerInParent = false;
                    }
                }
            }

            if (src.empty()) { // VkInstance
                src = "defaultContext";
                ownerInParent = false;
            }

            pfnSourceOverride = "/*SRC*/" + src;
            if (ownerInParent) {
                pfnSourceOverride += "->getDispatcher()->";
            } else {
                pfnSourceOverride += ".getDispatcher()->";
            }

            superclassSource = getSuperclassSource();

            pfnSourceOverride = superclassSource.getDispatcher() + "->";

            specifierInline = true;
            specifierExplicit = true;
            specifierConst = false;

            if (!last || last->isArray()) {

                hasDependencies = false;
                return;
            }

            dbgtag = "raii constructor";
        }

        SuperclassSource getSuperclassSource() const {
            SuperclassSource s;
            const auto &type = cls->superclass;
            // pfnSourceOverride += ".get" + cls->superclass + "()";
            for (const VariableData &v : cmd->params) {
                if (!v.getIgnoreProto() && v.type() == type) {
                    s.src = v.identifier();
                    s.isPointer = v.isPointer();
                    return s;
                }
            }
            for (const VariableData &v : cmd->params) {
                if (!v.getIgnoreProto() && v.getNamespace() == Namespace::RAII && v.isHandle()) {
                    s.src = v.identifier();
                    s.src += v.isPointer()? "->" : ".";
                    s.src += "get" + type + "()";
                    s.isPointer = v.isPointer();
                    s.isIndirect = true;
                    return s;
                }
            }
            // not found, use default
            s.src = "/*NOT FOUND: " + type + "*/";
            std::cerr << "/*NOT FOUND: " << type << "*/" << '\n';
            for (const VariableData &v : cmd->params) {
                std::cerr << "  > " << v.type() << "  " << v.getIgnoreProto() << '\n';
            }
            return s;
        }

        virtual std::string generateMemberBody() override {

            std::string output;

            const std::string &owner = cls->ownerhandle;
            if (!owner.empty() && !constructorInterop) {
                output += "    " + owner + " = " + superclassSource.getSuperclassAssignment() + ";\n";
            }

            std::string call = generatePFNcall();

            output += "    " + call + "\n";
            output += generateCheck();

            if (!cls->isSubclass && !constructorInterop) {
                const auto &superclass = cls->superclass;
                if (cls->name == "Instance" && gen.getConfig().gen.staticInstancePFN) {
                    output += gen.format("    dispatcher = {2}Dispatcher( {1}vkGet{2}ProcAddr, {3} );\n",
                                         superclass, getDispatchSource(), cls->name, cls->vkhandle.toArgument());
                }
                else if (cls->name == "Device" && gen.getConfig().gen.staticDevicePFN) {
                    output += gen.format("    dispatcher = {2}Dispatcher( {1}vkGet{2}ProcAddr, {3} );\n",
                                         superclass, getDispatchSource(), cls->name, cls->vkhandle.toArgument());
                }
                else {
                    output += gen.format("    m_dispatcher.reset( new {2}Dispatcher( {1}vkGet{2}ProcAddr, {3} ) );\n",
                                         superclass, getDispatchSource(), cls->name, cls->vkhandle.toArgument());
                }
            }

            return output;
        }       

    };

    class MemberResolverVectorCtor final : public MemberResolverCtor {

      public:
        MemberResolverVectorCtor(Generator &gen, ClassCommandData &d, MemberContext &refCtx)
            : MemberResolverCtor(gen, d, refCtx) {

            if (cmd->params.empty()) {
                std::cerr << "error: MemberResolverVectorCtor" << '\n';
                hasDependencies = false;
                return;
            }
            hasDependencies = true;

            clsname += "s";
            name += "s";
            // last = cmd->getLastPointerVar();

            specifierInline = true;
            specifierExplicit = false;
            specifierConst = false;
            dbgtag = "vector constructor";
        }

        virtual std::string generateMemberBody() override {
            return MemberResolverDefault::generateMemberBody();
        }
    };

    class MemberResolverUniqueCtor final : public MemberResolverDefault {

    public:
        MemberResolverUniqueCtor(Generator &gen, ClassCommandData &d, MemberContext &refCtx)
            : MemberResolverDefault(gen, d, refCtx, true)
        {
            name = "Unique" + cls->name;


            InitializerBuilder init("        ");
            const auto &vars = getFilteredProtoVars();
            // base class init
            for (const VariableData &p : vars) {
                if (p.type() == cls->name) {
                    init.append(cls->name, p.identifier());
                    break;
                }
            }

            cls->foreachVars(VariableData::Flags::CLASS_VAR_UNIQUE, [&](const VariableData &v){
                for (const VariableData &p : vars) {
                    if (p.type() == v.type() || (p.getSpecialType() == VariableData::TYPE_DISPATCH && v.getSpecialType() == VariableData::TYPE_DISPATCH)) {
                        init.append(v.identifier(), p.toVariable(v));
                    }
                }
            });

            initializer = init.string();

            specifierInline = false;
            specifierExplicit = true;
            specifierConst = false;
            ctx.generateInline = true;
            dbgtag = "unique constructor";
        }

        virtual std::string generateMemberBody() override {
            return "";
        }

    };

    class MemberResolverCreateHandleRAII final : public MemberResolverDefault {

      public:
        MemberResolverCreateHandleRAII(Generator &gen, ClassCommandData &d, MemberContext &refCtx)
            : MemberResolverDefault(gen, d, refCtx) {

            ctx.useThis = true;

            dbgtag = "create handle raii";
        }

        virtual std::string generateMemberBody() override {
            if (last->isArray() && !ctx.returnSingle) {                
                return MemberResolverDefault::generateMemberBody();
            }
            else {                
                return gen.format(R"(    return {0}({1});
)",                             returnType, createPassArgumentsRAII());
            }
        }
    };

    class MemberResolverCreate : public MemberResolverDefault {

      public:
        MemberResolverCreate(Generator &gen, ClassCommandData &d, MemberContext &refCtx)
            : MemberResolverDefault(gen, d, refCtx)
        {

            dbgtag = cmd->nameCat == MemberNameCategory::ALLOCATE ? "allocate"
                                                                 : "create";
            ctx.useThis = true;

            if (ctx.returnSingle) {
                std::string tag = gen.strRemoveTag(name);
                auto pos = name.find_last_of('s');
                if (pos != std::string::npos) {
                    name.erase(pos);
                }
                else {
                    std::cout << "MemberResolverCreate single no erase!" << std::endl;
                }
                name += tag;

                auto var = last->getLengthVar();
                if (var) {
                    if (!last->isLenAttribIndirect()) {
                        var->setAltPFN("1");
                    }
                    for (VariableData* v : var->getArrayVars()) {
                        if (v->isArrayIn()) {
                            v->convertToConstReference();
                            auto id = v->identifier();
                            auto pos = id.find_last_of('s');
                            if (pos != std::string::npos) {
                                id.erase(pos);
                                v->setIdentifier(id);
                            }
                        }
                    }
                }

            }
        }

        virtual std::string generateMemberBody() override {
            std::string output;

            if (last->isArray() && !ctx.returnSingle) {
                return MemberResolverDefault::generateMemberBody();
            } else {
                if (ctx.returnSingle && last->isLenAttribIndirect()) {
                    auto rhs = last->getLenAttribRhs();
                    if (!rhs.empty()) {
                        auto var = last->getLengthVar();
                        if (var) {
                            output += "    VULKAN_HPP_ASSERT( " + var->identifier() + "." + rhs + " == 1 );\n";
                        }
                    }
                }
                // output += "/* MemberResolverCreate */\n";
                if (ctx.ns == Namespace::RAII) {

                    last->setIgnorePFN(true);
                    if (!cmd->params.empty()) {
                        auto &first = cmd->params.begin()->get();
                        if (first.original.type() == cls->name.original) {
                            first.setIgnorePFN(true);
                        }
                    }
                    std::string args = //"*this, " +
                                       createPassArguments(true);
                    output +=
                        "    return " + last->fullType() + "(" + args + ");\n";
                } else {

                    const std::string &call = generatePFNcall();
                    std::string id = last->identifier();
                    output += "    " + last->fullType() + " " + id + ";\n";
                    output += "    " + call + "\n";
                    output += generateCheck();
                    returnValue = generateReturnValue(id);
                }
            }
            return output;
        }
    };

    class MemberResolverCreateUnique final : public MemberResolverCreate {
        bool isSubclass = {};

      public:
        MemberResolverCreateUnique(Generator &gen, ClassCommandData &d, MemberContext &refCtx)
            : MemberResolverCreate(gen, d, refCtx) {

            returnType = "Unique" + last->type();

            if (last->isHandle()) {
                const auto &handle = gen.findHandle(last->original.type());
                isSubclass = handle.isSubclass;
            }
            name += "Unique";
            dbgtag = "create unique";
        }

        virtual std::string generateMemberBody() override {            
            std::string args = last->identifier();
            if (isSubclass) {
                args += ", *this";
            }
            if (gen.getConfig().gen.allocatorParam) {
                args += ", allocator";
            }
            if (gen.getConfig().gen.dispatchParam) {
                args += ", d";
            }

            std::string output = MemberResolverCreate::generateMemberBody();            
            returnValue = generateReturnValue(returnType + "(" + args + ")");            
            return output;
        }
    };

    class MemberGenerator {
        Generator &gen;
        ClassCommandData &m;
        MemberContext &ctx;
        GenOutputClass &out;
        UnorderedFunctionOutput &funcs;
        UnorderedFunctionOutput &funcsEnhanced;
        std::vector<Signature> generated;

        enum class MemberGuard {
            NONE,
            UNIQUE
        };

        void generate(MemberResolver &resolver, MemberGuard g = MemberGuard::NONE, bool dbg = false) {
            const auto &sig = resolver.createSignature();
            for (const auto &g : generated) {
                if (g == sig) {
                    if (dbg) {
                        std::cerr << "generate(): signature conflict: " << sig.name << "(" << sig.args << ")" << '\n';
                    }
                    return;
                }
            }

            if (g != MemberGuard::NONE) {
                static const std::array<std::string, 2> guards{
                    "", "VULKAN_HPP_NO_SMART_HANDLE"};

                resolver.guard = guards[static_cast<int>(g)];
            }
            else {
                if (resolver.ctx.ns == Namespace::RAII) {
                    if (resolver.cmd->createsHandle()) {
                        resolver.guard = "VULKAN_HPP_EXPERIMENTAL_NO_RAII_CREATE_CMDS";
                    }
                    else if (resolver._indirect || resolver._indirect2) {
                        if (resolver.cls->isSubclass) {
                            resolver.guard = "VULKAN_HPP_EXPERIMENTAL_NO_RAII_INDIRECT_SUB";
                        }
                        else {
                            resolver.guard = "VULKAN_HPP_EXPERIMENTAL_NO_RAII_INDIRECT";
                        }
                    }
                }
            }
            if (generated.empty()) {
                resolver.generate(out.sFuncs, funcs);
            }
            else {
                resolver.generate(out.sFuncsEnhanced, funcsEnhanced);
            }

            generated.push_back(sig);
            // std::cout << "generated: " << resolver.dbgtag << '\n';
        }

        template<typename T>
        void generate(MemberGuard g = MemberGuard::NONE) {
            T resolver{gen, m, ctx};
            generate(resolver, g);

//            if (resolver->returnsVector()) {
//                // std::cout << "> returns vector: " << ctx.name << '\n';
//                resolver->addStdAllocators();
//                gen(*resolver);
//            }
        }

        void generateDestroyOverload(ClassCommandData &m, MemberContext &ctx, const std::string &name) {

            generate<MemberResolverDefault>(MemberGuard::NONE); // TODO ignore sFuncs

            const auto &type = m.cls->name;

            auto &generated = gen.generatedDestroyOverloads[type];

            m.src->prepare();
#ifndef NDEBUG
            bool ok;
            m.src->check(ok);
#endif
            auto last = m.src->getLastHandleVar();
            if (!last) {
                std::cerr << "null access" << '\n';
                return;
            }
            const auto &lastType = last->type();
            auto s = generated.size();
            if (generated.contains(lastType)) {
                // std::cerr << "can't generate destroy: already exists  " << m.cls->name << ": " << last->type() << '\n';
                return;
            }

            if (last->original.type() == m.src->name.original) {
                return;
            }


            auto second = MemberResolverDefault{gen, m, ctx};
            // TODO move to ctx
            const auto getFirst = [&]() -> VariableData* {
                const auto &vars = second.getFilteredProtoVars();
                if (vars.empty()) {
                    return nullptr;
                }
                return &vars.begin()->get();
            };
            auto first = getFirst();
            if (!first || !gen.isHandle(first->original.type())) {
                // std::cout << ">>> drop: " << c.name.original << '\n';
//                std::cerr << "can't generate destroy: drop  " << m.cls->name << ": " << last->type() << '\n';
//                for (const VariableData &v : second.getFilteredProtoVars()) {
//                    std::cerr << "    v: " << v.fullType() << '\n';
//                }
                return;
            }
            first->setOptional(false);

            second.name = name;
            second.dbgtag = "d. overload";
            generate(second);

            generated.insert(last->type());
            // std::cerr << "generated destroy: " << m.cls->name << ": " << last->type() << '\n';
        }

    public:
        MemberGenerator(Generator &gen,
                        ClassCommandData &m,
                        MemberContext &ctx,
                        GenOutputClass &out,
                        UnorderedFunctionOutput &funcs,
                        UnorderedFunctionOutput &funcsEnhanced)
          : gen(gen), m(m), ctx(ctx), out(out), funcs(funcs), funcsEnhanced(funcsEnhanced)
        {}

        void generate() {
            if (!m.src->canGenerate()) {
                return;
            }
            if (m.raiiOnly && ctx.ns != Namespace::RAII) {
                return;
            }

            // out.sFuncs += "    // C: " + m.src->name + " (" + m.src->name.original + ")\n";

            if (m.src->canTransform()) {
                generate<MemberResolverPass>();
            }

            // std::cerr << "generate() prepare  " << m.src->name.original << '\n';
            m.src->prepare(); // TODO unnecessary?
            auto last = m.src->getLastVar(); // last argument
            if (!last) {
                std::cout << "null access getPrimaryResolver" << '\n';
                generate<MemberResolverDefault>();
                return;
            }
            //        std::cout << "last: " << last << '\n';
            //        std::cout << "  " << last->type() << '\n';
            bool uniqueVariant = false;
            if (last->isHandle()) {
                const auto &handle = gen.findHandle(last->original.type());
                uniqueVariant = handle.uniqueVariant();

                if (last->isArrayOut() && handle.vectorVariant &&
                    ctx.ns == Namespace::RAII) {

                    const auto &parent = m.src->params.begin()->get();
                    if (parent.isHandle()) {
                        const auto &handle = gen.findHandle(parent.original.type());
                        if (handle.isSubclass) {
                            const auto &superclass = handle.superclass;
                            if (superclass.original !=
                                m.cls->superclass.original)
                            {

                                std::cerr << "add var: " <<  superclass << ", p: " << parent.type() << ", " << m.name << '\n';
//                                auto &var = resolver->addVar(resolver->cmd->params.begin(), superclass);
//                                var.setConst(true);

                                //ctx.insertSuperclassVar = true;
                            }
                        }
                    }

                    generate<MemberResolverVectorRAII>();
                    return;
                }

                if (ctx.ns == Namespace::RAII && m.src->createsHandle()) {
                    generate<MemberResolverCreateHandleRAII>();
                    return;
                }

            }

            if (m.src->pfnReturn == PFNReturnCategory::OTHER) {
                generate<MemberResolverDefault>();
                return;
            }

            const auto &cfg = gen.getConfig();
            switch (m.src->nameCat) {
            case MemberNameCategory::ALLOCATE:
            case MemberNameCategory::CREATE:

                generate<MemberResolverCreate>();

                if (ctx.ns == Namespace::VK) {
                    if (uniqueVariant && !last->isArray() && cfg.gen.smartHandles) {
                        generate<MemberResolverCreateUnique>(MemberGuard::UNIQUE);
                    }
                }

                if (m.src->returnsVector()) {

                    out.sFuncs += " // RETURN SINGLE\n";
                    ctx.returnSingle = true;
                    generate<MemberResolverCreate>();
                }
                return;
            case MemberNameCategory::DESTROY:
                generateDestroyOverload(m, ctx, "destroy");
                return;
            case MemberNameCategory::FREE:
                generateDestroyOverload(m, ctx, "free");
                return;
            default:
                generate<MemberResolverDefault>();
                return;
            }

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

    void generateClassMember(ClassCommandData &m, MemberContext &ctx, GenOutputClass &out,
                             UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, bool inlineFuncs = false);

    void generateClassMembers(const HandleData &data, GenOutputClass &out,
                              UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, Namespace ns, bool inlineFuncs = false);

    void generateClassConstructors(const HandleData &data, GenOutputClass &out);

    void generateClassConstructorsRAII(const HandleData &data,
                                       GenOutputClass &out, UnorderedFunctionOutput &funcs);

    std::string generateUniqueClassStr(const HandleData &data, UnorderedFunctionOutput &funcs, bool inlineFuncs);

    std::string generateUniqueClass(const HandleData &data, UnorderedFunctionOutput &funcs);

    std::string generateClass(const HandleData &data, UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced);

    std::string generateClassStr(const HandleData &data,
                                 UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, bool inlineFuncs);

    std::string generateClassRAII(const HandleData &data);

    std::string generatePFNs(const HandleData &data, GenOutputClass &out);

    std::string generateLoader();

    std::string genMacro(const Macro &m);    

    std::string beginNamespace(bool noExport = false) const;

    std::string beginNamespaceRAII(bool noExport = false) const;

    std::string beginNamespace(const Registry::Macro &ns, bool noExport = false) const;

    std::string endNamespace() const;

    std::string endNamespaceRAII() const;

    std::string endNamespace(const Registry::Macro &ns) const;

    std::string expIfndef(const std::string &name) const {
        if (!cfg.gen.expApi) {
            return "";
        }
        return "#ifndef " + name + "\n";
    }

    std::string expEndif(const std::string &name) const {
        if (!cfg.gen.expApi) {
            return "";
        }
        return "#endif // " + name + "\n";
    }

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
                std::cout << "disabled elem: " << t.name << " " << t.typeString() << std::endl;
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

        virtual void prepare() = 0;
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

        virtual void prepare() noexcept override;
    };

    void bindVars(Variables &vars) const;

  public:
    Generator();

    void resetConfig();

    void loadConfigPreset();

    void setOutputFilePath(const std::string &path);

    bool isOuputFilepathValid() const {
        return std::filesystem::is_directory(outputFilePath);
    }

    bool separatePlatforms() const {
        return cfg.gen.separatePlatforms;
    }

    std::string getOutputFilePath() const { return outputFilePath; }

    void load(const std::string &xmlPath);

    void generate();

    Platforms &getPlatforms() { return platforms; };

    Extensions &getExtensions() { return extensions; };

    auto& getHandles() { return handles; }

    auto& getCommands() { return commands; }

    auto& getStructs() { return structs; }

    auto &getEnums() { return enums; }

    Config &getConfig() { return cfg; }

    const Config &getConfig() const { return cfg; }

    bool isInNamespace(const std::string &str) const;

    const Registry::Macro& findNamespace(Namespace ns) const;

    std::string getNamespace(Namespace ns, bool colons = true) const;

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
