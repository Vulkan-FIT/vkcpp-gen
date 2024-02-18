#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
#include <optional>

#include "tinyxml2.h"

namespace Utils {

namespace Enums {

template <typename BitType>
    class Flags {
    public:
        using MaskType = typename std::underlying_type<BitType>::type;

     // constructors
     constexpr Flags() noexcept : m_mask( 0 ) {}

     constexpr Flags( BitType bit ) noexcept : m_mask( static_cast<MaskType>( bit ) ) {}

     constexpr Flags( Flags<BitType> const & rhs ) noexcept = default;

     constexpr explicit Flags( MaskType flags ) noexcept : m_mask( flags ) {}

     // relational operators
    #if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
     auto operator<=>( Flags<BitType> const & ) const = default;
    #else
     constexpr bool operator<( Flags<BitType> const & rhs ) const noexcept
     {
         return m_mask < rhs.m_mask;
     }

     constexpr bool operator<=( Flags<BitType> const & rhs ) const noexcept
     {
         return m_mask <= rhs.m_mask;
     }

     constexpr bool operator>( Flags<BitType> const & rhs ) const noexcept
     {
         return m_mask > rhs.m_mask;
     }

     constexpr bool operator>=( Flags<BitType> const & rhs ) const noexcept
     {
         return m_mask >= rhs.m_mask;
     }

     constexpr bool operator==( Flags<BitType> const & rhs ) const noexcept
     {
         return m_mask == rhs.m_mask;
     }

     constexpr bool operator!=( Flags<BitType> const & rhs ) const noexcept
     {
         return m_mask != rhs.m_mask;
     }
    #endif

     // logical operator
     constexpr bool operator!() const noexcept
     {
         return !m_mask;
     }

     // bitwise operators
     constexpr Flags<BitType> operator&( Flags<BitType> const & rhs ) const noexcept
     {
         return Flags<BitType>( m_mask & rhs.m_mask );
     }

     constexpr Flags<BitType> operator|( Flags<BitType> const & rhs ) const noexcept
     {
         return Flags<BitType>( m_mask | rhs.m_mask );
     }

     constexpr Flags<BitType> operator^( Flags<BitType> const & rhs ) const noexcept
     {
         return Flags<BitType>( m_mask ^ rhs.m_mask );
     }

    //    constexpr Flags<BitType> operator~() const noexcept
    //    {
    //        return Flags<BitType>( m_mask ^ FlagTraits<BitType>::allFlags.m_mask );
    //    }

     // assignment operators
     constexpr Flags<BitType> & operator=( Flags<BitType> const & rhs ) noexcept = default;

     constexpr Flags<BitType> & operator|=( Flags<BitType> const & rhs ) noexcept
     {
         m_mask |= rhs.m_mask;
         return *this;
     }

     constexpr Flags<BitType> & operator&=( Flags<BitType> const & rhs ) noexcept
     {
         m_mask &= rhs.m_mask;
         return *this;
     }

     constexpr Flags<BitType> & operator^=( Flags<BitType> const & rhs ) noexcept
     {
         m_mask ^= rhs.m_mask;
         return *this;
     }

     // cast operators
     explicit constexpr operator bool() const noexcept
     {
         return !!m_mask;
     }

     explicit constexpr operator MaskType() const noexcept
     {
         return m_mask;
     }

    #if defined( VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC )
    public:
    #else
    private:
    #endif
     MaskType m_mask;
    };

    #if !defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
    // relational operators only needed for pre C++20
    template <typename BitType>
    constexpr bool operator<( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator>( bit );
    }

    template <typename BitType>
    constexpr bool operator<=( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator>=( bit );
    }

    template <typename BitType>
    constexpr bool operator>( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator<( bit );
    }

    template <typename BitType>
    constexpr bool operator>=( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator<=( bit );
    }

    template <typename BitType>
    constexpr bool operator==( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator==( bit );
    }

    template <typename BitType>
    constexpr bool operator!=( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator!=( bit );
    }
    #endif

    // bitwise operators
    template <typename BitType>
    constexpr Flags<BitType> operator&( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator&( bit );
    }

    template <typename BitType>
    constexpr Flags<BitType> operator|( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator|( bit );
    }

    template <typename BitType>
    constexpr Flags<BitType> operator^( BitType bit, Flags<BitType> const & flags ) noexcept
    {
     return flags.operator^( bit );
    }

    // bitwise operators on BitType
    template <typename BitType>//, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
    inline constexpr Flags<BitType> operator&( BitType lhs, BitType rhs ) noexcept
    {
     return Flags<BitType>( lhs ) & rhs;
    }

    template <typename BitType>//, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
    inline constexpr Flags<BitType> operator|( BitType lhs, BitType rhs ) noexcept
    {
     return Flags<BitType>( lhs ) | rhs;
    }

    template <typename BitType>//, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
    inline constexpr Flags<BitType> operator^( BitType lhs, BitType rhs ) noexcept
    {
     return Flags<BitType>( lhs ) ^ rhs;
    }

    template <typename BitType>//, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
    inline constexpr Flags<BitType> operator~( BitType bit ) noexcept
    {
     return ~( Flags<BitType>( bit ) );
    }

    }
}

static bool caseInsensitivePredicate(char a, char b) {
    return std::toupper(a) == std::toupper(b);
}

// case insensitve search for substring
static bool strContains(const std::string &str, const std::string &substr) {
    auto it = std::search(str.begin(), str.end(), substr.begin(), substr.end(),
                          caseInsensitivePredicate);
    return it != str.end();
}

static void strStripPrefix(std::string &str, const std::string_view &prefix) {
    if (prefix.size() > str.size()) {
        return;
    }
    if (str.substr(0, prefix.size()) == prefix) {
        str.erase(0, prefix.size());
    }
}

static void strAddPrefix(std::string &str, const std::string_view &prefix) {
    if (!str.starts_with(prefix)) {
        str = prefix.data() + str;
    }
}

static void strStripSuffix(std::string &str, const std::string_view &suffix) {
    if (suffix.size() > str.size()) {
        return;
    }
    if (str.substr(str.size() - suffix.size(), std::string::npos) == suffix) {
        str.erase(str.size() - suffix.size(), std::string::npos);
    }
}

static void strStripVk(std::string &str) {
    strStripPrefix(str, "Vk");
    strStripPrefix(str, "vk");
}

static std::string strStripVk(const std::string &str) {
    std::string out = str;
    strStripVk(out);
    return out;
}

static std::string addVkPrefix(const std::string &str) {
    std::string out = str;
    strAddPrefix(out, "Vk");
    return out;
}

static std::string stripVkPrefix(const std::string &str) {
    std::string out = str;
    strStripVk(out);
    return out;
}

static std::string camelToSnake(const std::string &str) {
    std::string out;
    for (char c : str) {
        if (std::isupper(c) && !out.empty()) {
            out += '_';
        }
        out += std::toupper(c);
    }
    return out;
}

static std::string convertSnakeToCamel(const std::string &str) {
    std::string out;
    bool flag = false;
    for (char c : str) {
        if (c == '_') {
            flag = true;
            continue;
        }
        out += flag ? std::toupper(c) : std::tolower(c);
        flag = false;
    }
    return out;
}

static std::vector<std::string_view> split2(const std::string_view &str, const std::string_view &delim) {
    std::vector<std::string_view> out;
    size_t start = 0;
    auto end = str.find(delim);
    while (end != std::string::npos) {
        std::string_view token = str.substr(start, end - start);
        out.push_back(token);
        start = end + delim.size();
        end = str.find(delim, start);
    }
    std::string_view token = str.substr(start, end - start);
    out.push_back(token);
    return out;
}

static std::vector<std::string> split(const std::string &str, const std::string &delim) {
    std::vector<std::string> out;
    size_t start = 0;
    auto end = str.find(delim);
    while (end != std::string::npos) {
        std::string token = str.substr(start, end - start);
        out.push_back(token);
        start = end + delim.size();
        end = str.find(delim, start);
    }
    std::string token = str.substr(start, end - start);
    out.push_back(token);
    return out;
}

static std::string strFirstLower(const std::string_view &str) {
    std::string out = str.data();
    if (!out.empty()) {
        out[0] = std::tolower(out[0]);
    }
    return out;
}

static std::string strFirstUpper(const std::string_view &str) {
    std::string out = str.data();
    if (!out.empty()) {
        out[0] = std::toupper(out[0]);
    }
    return out;
}

static std::string toCppStyle(const std::string &str, bool firstCapital = false) {
    std::string out = stripVkPrefix(str);
    return firstCapital? strFirstUpper(out) : strFirstLower(out);
}

static int countPointers(const std::string &str) {
    return static_cast<int>(std::count(str.begin(), str.end(), '*'));
}

static std::pair<int, int> countPointers(const std::string &from, const std::string &to) {
    return std::make_pair(countPointers(from), countPointers(to));
}

static std::string matchTypePointers(const std::string &from, const std::string &to) {
    int cfrom;
    int cto;
    std::tie(cfrom, cto) = countPointers(from, to);
    if (cfrom > cto) {
        return "*";
    }
    if (cfrom < cto) {
        return "&";
    }
    return "";
}

class String : public std::string {

  public:
    std::string original;
    String& operator=(const std::string &rhs) {
        std::string::assign(rhs);
        return *this;
    }

    String(const std::string &src) {
        reset(src);
    }

    String(const std::string &src, bool firstCapital) {
        convert(src, firstCapital);
    }

    void reset(const std::string &src = "") {
        original = src;
        std::string::assign(src);
    }

    void convert(const std::string &src, bool firstCapital = false) {
        original = src;
        std::string::assign(toCppStyle(src, firstCapital));
    }
};

class Argument {
  public:
    std::string type;
    std::string id;
    std::string assignment;

    Argument(const std::string &type, const std::string &id, const std::string &assignment = "") :
                                                                                                   type(type), id(id), assignment(assignment)
    {

    }

    operator std::string() const noexcept {
        return type + id + assignment;
    }

    bool empty() const {
        return type.empty() && id.empty();
    }

};

class InitializerBuilder {
    std::string init;
    std::string indent;

  public:
    InitializerBuilder(const std::string &indent) : indent(indent) {}

    void append(const std::string &id, const std::string &assignment) {
        if (!init.empty()) {
            init += indent + ", ";
        }
        init += id + "(" + assignment + ")\n";
    }

    void appendRaw(const std::string &str) {
        init += str;
    }

    std::string string() const noexcept {
        if (init.empty()) {
            return "";
        }
        return "\n" + indent + ": " + init;
    }

};

class ArgumentBuilder {

    std::string str;
    std::string init;
    bool declaration;

  public:
    std::vector<Argument> args;

    ArgumentBuilder(bool declaration) noexcept : declaration(declaration) {}

    void append(const std::string &type, const std::string &id, const std::string &assignment = "", const std::string &initId = "", bool ref = false) {
        if (!str.empty()) {
            str += ", ";
        }
        str += type + id;
        if (declaration) {
            str += assignment;
        }
        if (!initId.empty()) {
            if (!init.empty()) {
                init += ", ";
            }
            init += initId + "(" + (ref? "&" : "") + id + ")";
        }
        args.push_back(Argument(type, id, assignment));
    }

    void append(const Argument &arg, const std::string &initId = "", bool ref = false) {
        append(arg.type, arg.id, arg.assignment, initId, ref);
    }

    operator std::string() const noexcept {
        return str;
    }

    std::string string() const noexcept {
        return str;
    }

    std::string initializer() const noexcept {
        if (init.empty()) {
            return "";
        }
        return " : " + init;
    }

};


class AttributeNotFoundException: public std::runtime_error {
  public:
    AttributeNotFoundException() : runtime_error("missing XML attribute") {}
    AttributeNotFoundException(const std::string &msg) : runtime_error(msg.c_str()) {}
};

inline std::string_view getRequiredAttrib(const tinyxml2::XMLElement *e, const std::string_view &attribute) {
    const char *attrib = e->Attribute(attribute.data());
    if (!attrib) {
        throw new AttributeNotFoundException{"missing XML attribute: " + std::string{attribute}};
    }
    return attrib;
}

inline std::optional<std::string_view> getAttrib(const tinyxml2::XMLElement *e, const std::string_view &attribute) {
    const char *attrib = e->Attribute(attribute.data());
    if (!attrib) {
        return std::optional<std::string_view>();
    }
    return attrib;
}

// Class for XMLNode and XMLElement iteration
template <class Base> class NodeContainer {
  protected:
    Base *first;
    Base *last;

  public:
    using value_type = Base;

    class iterator {
        Base *node;

      public:
        iterator() = default;
        iterator(Base *node) : node(node) {}

        iterator operator++() {
            if constexpr (std::is_same<Base, tinyxml2::XMLNode>::value) {
                node = node->NextSibling();
            } else {
                node = node->NextSiblingElement();
            }
            return *this;
        }

        bool operator!=(const iterator &other) const {
            return node != other.node;
        }

        bool operator==(const iterator &other) const {
            return node == other.node;
        }

        Base &operator*() const { return *node; }
        Base *operator&() const { return node; }
        Base *operator->() { return node; }
    };

    iterator begin() const { return iterator{first}; }

    iterator end() const { return iterator{last}; }

    static_assert(std::is_same<Base, tinyxml2::XMLNode>::value ||
                      std::is_same<Base, tinyxml2::XMLElement>::value,
                  "NodeContainter does not support this type.");
};

class Nodes : public NodeContainer<tinyxml2::XMLNode> {
  public:
    Nodes(tinyxml2::XMLNode *parent) {
        first = parent->FirstChild();
        last = nullptr; // parent->LastChild()->NextSibling();
    }
};

class Elements : public NodeContainer<tinyxml2::XMLElement> {

  public:
    Elements(tinyxml2::XMLNode *parent) {
        first = parent->FirstChildElement();
        last = nullptr; // parent->LastChildElement()->NextSiblingElement();
    }
};

class ValueFilter {
    std::string text;

  public:
    ValueFilter(std::string text) : text(text) {}

    bool operator()(const char *value) const {
        return std::string_view(value) == text;
    }
};

template <typename T>
auto operator|(const T &container, const ValueFilter &filter) {
    typedef typename T::value_type value_type;
    std::vector<value_type *> output;
    for (const auto &element : container) {
        if (filter(element.Value())) {
            output.push_back(const_cast<value_type *>(&element));
        }
    }
    return output;
}

#endif // UTILS_H
