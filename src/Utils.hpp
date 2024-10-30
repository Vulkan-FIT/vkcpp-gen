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

#ifndef UTILS_H
#define UTILS_H

#include "tinyxml2.h"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
namespace rg = std::ranges;

namespace vkgen
{

    namespace xml
    {

        class AttributeNotFoundException : public std::runtime_error
        {
          public:
            AttributeNotFoundException() : runtime_error("missing XML attribute") {}

            explicit AttributeNotFoundException(const std::string &msg) : runtime_error(msg.c_str()) {}
        };

        std::string_view requiredAttrib(const tinyxml2::XMLElement *e, const std::string_view &attribute);

        std::string_view value(const tinyxml2::XMLNode &node);

        std::optional<std::string_view> attrib(const tinyxml2::XMLElement *e, const std::string_view &attribute);

        // Wrapper
        class Element
        {
            tinyxml2::XMLElement *data = {};

          public:
            Element() = default;

            explicit Element(tinyxml2::XMLElement *e) : data(e) {}

            Element &operator=(tinyxml2::XMLElement *e);

            bool operator==(const Element &o) const;

            tinyxml2::XMLElement *operator->() const;

            tinyxml2::XMLElement *toElement() const;

            Element firstChild() const;

            std::string_view operator[](const std::string_view attrib) const;

            std::string_view required(const std::string_view attrib) const;

            std::optional<std::string_view> optional(const std::string_view attrib) const;

            std::string_view value() const;

            std::string_view getNested(const std::string_view attrib) const;
        };

        class Iterator
        {
            Element p = {};

          public:
            using iterator_category = std::input_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = Element;
            using pointer           = value_type *;
            using reference         = value_type &;
            using const_reference   = const value_type &;

            Iterator() = default;

            explicit Iterator(const Element &e) {
                p = e;
            }

            bool operator==(const Iterator &o) const {
                return p == o.p;
            }

            Iterator operator++(int) {
                Iterator temp = *this;
                p             = p->NextSiblingElement();
                return temp;
            }

            Iterator &operator++() {
                p = p->NextSiblingElement();
                return *this;
            }

            const_reference operator*() const {
                return p;
            }

            // reference operator*() { return p; }
            pointer operator->() {
                return &p;
            }
        };

        static_assert(std::input_iterator<Iterator>, "failed input iterator");
        static_assert(std::weakly_incrementable<Iterator>, "Failed the weakly incrementable test");
        static_assert(std::movable<Iterator>, "Failed the moveable test");
        static_assert(std::default_initializable<Iterator>, "Failed the default initializable test");

        class View : public std::ranges::view_interface<View>
        {
            Element data = {};

          public:
            using iterator       = Iterator;
            using const_iterator = const Iterator;

            View() = default;

            explicit View(tinyxml2::XMLElement *e) : data(e) {}

            explicit View(const Element &e) : data(e) {}

            const_iterator begin() const {
                return Iterator{ data };
            }

            const_iterator end() const {
                return Iterator{};
            }
        };

        static_assert(std::movable<View>, "Failed the moveable test");
        static_assert(std::default_initializable<View>, "Failed the default initializable test");

        inline auto filter_elements(const std::string_view name) {
            return std::views::filter([=](const Element &e) {
                const auto *v = e->Value();
                if (v) {
                    return std::string_view(v) == name;
                }
                return false;
            });
        }

        bool isVulkan(const Element e);

        bool isVulkanExtension(const Element e);

        inline auto filter_vulkan() {
            return std::views::filter([&](const Element &e) { return isVulkan(e); });
        }

        inline auto elements(const Element e, const std::string_view value) {
            return View(e) | filter_elements(value);
        }

        inline auto vulkanElements(const Element e, const std::string_view value) {
            return View(e) | filter_elements(value) | filter_vulkan();
        }

    }  // namespace xml

    namespace Utils
    {

        namespace Enums
        {

            template <typename BitType>
            class Flags
            {
              public:
                using MaskType = typename std::underlying_type<BitType>::type;

                // constructors
                constexpr Flags() noexcept : m_mask(0) {}

                constexpr Flags(BitType bit) noexcept : m_mask(static_cast<MaskType>(bit)) {}

                constexpr Flags(Flags<BitType> const &rhs) noexcept = default;

                constexpr explicit Flags(MaskType flags) noexcept : m_mask(flags) {}

                // relational operators
#if defined(VULKAN_HPP_HAS_SPACESHIP_OPERATOR)
                auto operator<=>(Flags<BitType> const &) const = default;
#else
                constexpr bool operator<(Flags<BitType> const &rhs) const noexcept {
                    return m_mask < rhs.m_mask;
                }

                constexpr bool operator<=(Flags<BitType> const &rhs) const noexcept {
                    return m_mask <= rhs.m_mask;
                }

                constexpr bool operator>(Flags<BitType> const &rhs) const noexcept {
                    return m_mask > rhs.m_mask;
                }

                constexpr bool operator>=(Flags<BitType> const &rhs) const noexcept {
                    return m_mask >= rhs.m_mask;
                }

                constexpr bool operator==(Flags<BitType> const &rhs) const noexcept {
                    return m_mask == rhs.m_mask;
                }

                constexpr bool operator!=(Flags<BitType> const &rhs) const noexcept {
                    return m_mask != rhs.m_mask;
                }
#endif

                // logical operator
                constexpr bool operator!() const noexcept {
                    return !m_mask;
                }

                // bitwise operators
                constexpr Flags<BitType> operator&(Flags<BitType> const &rhs) const noexcept {
                    return Flags<BitType>(m_mask & rhs.m_mask);
                }

                constexpr Flags<BitType> operator|(Flags<BitType> const &rhs) const noexcept {
                    return Flags<BitType>(m_mask | rhs.m_mask);
                }

                constexpr Flags<BitType> operator^(Flags<BitType> const &rhs) const noexcept {
                    return Flags<BitType>(m_mask ^ rhs.m_mask);
                }

                //    constexpr Flags<BitType> operator~() const noexcept
                //    {
                //        return Flags<BitType>( m_mask ^ FlagTraits<BitType>::allFlags.m_mask );
                //    }

                // assignment operators
                constexpr Flags<BitType> &operator=(Flags<BitType> const &rhs) noexcept = default;

                constexpr Flags<BitType> &operator|=(Flags<BitType> const &rhs) noexcept {
                    m_mask |= rhs.m_mask;
                    return *this;
                }

                constexpr Flags<BitType> &operator&=(Flags<BitType> const &rhs) noexcept {
                    m_mask &= rhs.m_mask;
                    return *this;
                }

                constexpr Flags<BitType> &operator^=(Flags<BitType> const &rhs) noexcept {
                    m_mask ^= rhs.m_mask;
                    return *this;
                }

                // cast operators
                explicit constexpr operator bool() const noexcept {
                    return !!m_mask;
                }

                explicit constexpr operator MaskType() const noexcept {
                    return m_mask;
                }

#if defined(VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC)
              public:
#else
              private:
#endif
                MaskType m_mask;
            };

#if !defined(VULKAN_HPP_HAS_SPACESHIP_OPERATOR)
            // relational operators only needed for pre C++20
            template <typename BitType>
            constexpr bool operator<(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator>(bit);
            }

            template <typename BitType>
            constexpr bool operator<=(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator>=(bit);
            }

            template <typename BitType>
            constexpr bool operator>(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator<(bit);
            }

            template <typename BitType>
            constexpr bool operator>=(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator<=(bit);
            }

            template <typename BitType>
            constexpr bool operator==(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator==(bit);
            }

            template <typename BitType>
            constexpr bool operator!=(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator!=(bit);
            }
#endif

            // bitwise operators
            template <typename BitType>
            constexpr Flags<BitType> operator&(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator&(bit);
            }

            template <typename BitType>
            constexpr Flags<BitType> operator|(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator|(bit);
            }

            template <typename BitType>
            constexpr Flags<BitType> operator^(BitType bit, Flags<BitType> const &flags) noexcept {
                return flags.operator^(bit);
            }

            // bitwise operators on BitType
            template <typename BitType>  //, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
            inline constexpr Flags<BitType> operator&(BitType lhs, BitType rhs) noexcept {
                return Flags<BitType>(lhs) & rhs;
            }

            template <typename BitType>  //, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
            inline constexpr Flags<BitType> operator|(BitType lhs, BitType rhs) noexcept {
                return Flags<BitType>(lhs) | rhs;
            }

            template <typename BitType>  //, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
            inline constexpr Flags<BitType> operator^(BitType lhs, BitType rhs) noexcept {
                return Flags<BitType>(lhs) ^ rhs;
            }

            template <typename BitType>  //, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
            inline constexpr Flags<BitType> operator~(BitType bit) noexcept {
                return ~(Flags<BitType>(bit));
            }

        }  // namespace Enums
    }      // namespace Utils

    static int toInt(const std::string &str) {
        int number;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), number);
        if (ec != std::errc{}) {
            throw std::runtime_error("can't convert string to int");
        }
        return number;
    }

    static bool caseInsensitivePredicate(char a, char b) {
        return std::toupper(a) == std::toupper(b);
    }

    // case insensitve search for substring
    static bool strContains(const std::string &str, const std::string &substr) {
        auto it = std::search(str.begin(), str.end(), substr.begin(), substr.end(), caseInsensitivePredicate);
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
        bool        flag = false;
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

    static std::vector<std::string_view> split2(const std::string_view str, const std::string_view delim) {
        std::vector<std::string_view> out;
        size_t                        start = 0;
        auto                          end   = str.find(delim);
        while (end != std::string::npos) {
            std::string_view token = str.substr(start, end - start);
            out.push_back(token);
            start = end + delim.size();
            end   = str.find(delim, start);
        }
        std::string_view token = str.substr(start, end - start);
        out.push_back(token);
        return out;
    }

    static std::vector<std::string> split(const std::string &str, const std::string &delim) {
        std::vector<std::string> out;
        size_t                   start = 0;
        auto                     end   = str.find(delim);
        while (end != std::string::npos) {
            std::string token = str.substr(start, end - start);
            out.push_back(token);
            start = end + delim.size();
            end   = str.find(delim, start);
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
        return firstCapital ? strFirstUpper(out) : strFirstLower(out);
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

    class String : public std::string
    {
      public:
        std::string original;

//        String &operator=(const std::string &rhs) {
//            std::string::assign(rhs);
//            return *this;
//        }

        String() = default;

        explicit String(const std::string &src) {
            reset(src);
        }

        explicit String(const String &src) {
            std::string::assign(src);
            original = src.original;
        }

        explicit String(String &&src) = default;

        String &operator=(String &&) = default;

        String &operator=(const String &) = default;

        String &operator=(const std::string &str) {
            std::string::assign(str);
            return *this;
        }

        String(const std::string &src, bool firstCapital) {
            convert(src, firstCapital);
        }

        void reset(const std::string &src = "") {
            std::string::assign(src);
            original = src;
        }

        void convert(const std::string &src, bool firstCapital = false) {
            original = src;
            std::string::assign(toCppStyle(src, firstCapital));
        }
    };

    class Argument
    {
      public:
        std::string type;
        std::string id;
        std::string assignment;

        Argument(const std::string &type, const std::string &id, const std::string &assignment = "") : type(type), id(id), assignment(assignment) {}

        operator std::string() const noexcept {
            return type + id + assignment;
        }

        bool empty() const {
            return type.empty() && id.empty();
        }
    };

    class InitializerBuilder
    {
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

    class ArgumentBuilder
    {
        std::string str;
        std::string init;
        bool        declaration;

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
                init += initId + "(" + (ref ? "&" : "") + id + ")";
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

    // Class for XMLNode and XMLElement iteration
    template <class Base>
    class NodeContainer
    {
      protected:
        Base *first;
        Base *last;

      public:
        using value_type = Base;

        class iterator
        {
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

            Base &operator*() const {
                return *node;
            }

            Base *operator&() const {
                return node;
            }

            Base *operator->() {
                return node;
            }
        };

        iterator begin() const {
            return iterator{ first };
        }

        iterator end() const {
            return iterator{ last };
        }

        static_assert(std::is_same<Base, tinyxml2::XMLNode>::value || std::is_same<Base, tinyxml2::XMLElement>::value,
                      "NodeContainter does not support this type.");
    };

    class Nodes : public NodeContainer<tinyxml2::XMLNode>
    {
      public:
        Nodes(tinyxml2::XMLNode *parent) {
            first = parent->FirstChild();
            last  = nullptr;  // parent->LastChild()->NextSibling();
        }
    };

    class Elements : public NodeContainer<tinyxml2::XMLElement>
    {
      public:
        Elements(tinyxml2::XMLNode *parent) {
            first = parent->FirstChildElement();
            last  = nullptr;  // parent->LastChildElement()->NextSiblingElement();
        }
    };

    class ValueFilter
    {
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
        std::vector<value_type *>      output;
        for (const auto &element : container) {
            if (filter(element.Value())) {
                output.push_back(const_cast<value_type *>(&element));
            }
        }
        return output;
    }

}  // namespace vkgen

#endif  // UTILS_H
