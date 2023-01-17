/*
MIT License

Copyright (c) 2021 guritchi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef XMLUTILS_HPP
#define XMLUTILS_HPP

#include "tinyxml2.h"
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <optional>

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

#endif // XMLUTILS_HPP
