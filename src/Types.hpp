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

#pragma once
#ifndef VKCPP_GEN_TYPES_HPP
#define VKCPP_GEN_TYPES_HPP

#ifndef USE_PCH
#include <set>
#endif

#include "Utils.hpp"

namespace vkgen
{

    class MetaType
    {
      public:
        enum class Value
        {
            Unknown,
            Enum,
            Struct,
            Union,
            Handle,
            Command,
            EnumValue,
            BaseType,
            FuncPointer,
            Feature,
            Extension,
            Platform
        };
        // using enum Value;

        MetaType() = default;

        explicit MetaType(Value type) : value(type) {}

        explicit operator Value() const {
            return value;
        }

        MetaType & operator=(Value type) {
            value = type;
            return *this;
        }

        std::string metaTypeDeclaration() const noexcept {
            using enum Value;
            switch (value) {
                case Enum: return "enum";
                case Struct: return "struct";
                case Union: return "union";
                case Handle: return "class";
                default: return "unknown";
            }
        }

        std::string metaTypeString() const noexcept {
            using enum Value;
            switch (value) {
                case Enum: return "enum";
                case Struct: return "struct";
                case Union: return "union";
                case Handle: return "handle";
                case Command: return "command";
                case EnumValue: return "enumvalue";
                case BaseType: return "basetype";
                case FuncPointer: return "funcpointer";
                case Feature: return "feature";
                case Extension: return "extension";
                case Platform: return "platform";
                default: return "unknown";
            }
        }

        void setMetaType(Value type) noexcept {
            value = type;
        }

        Value metaType() const {
            return value;
        }

        bool isEnum() const noexcept {
            return value == Value::Enum;
        }

        bool isStruct() const noexcept {
            return value == Value::Struct;
        }

        bool isUnion() const noexcept {
            return value == Value::Union;
        }

        bool isHandle() const noexcept {
            return value == Value::Handle;
        }

        bool isCommand() const noexcept {
            return value == Value::Command;
        }

        bool isStructOrUnion() const noexcept {
            return isStruct() || isUnion();
        }

      private:
        Value value = {};
    };

#ifdef GENERATOR_GUI
    struct SelectableGUI
    {
        bool selected = {};
        bool hovered  = {};
        bool filtered = true;

        virtual void setEnabledChildren(bool value, bool ifSelected = false) {}

        void setSelected(bool value) {
            selected = value;
        }

        bool isSelected() const {
            return selected;
        }
    };
#endif

    struct Feature;
    struct Extension;
    struct Platform;

    struct GenericType
      : public MetaType
#ifdef GENERATOR_GUI
      , public SelectableGUI
#endif
    {
      private:
        Extension *ext = {};
        Feature   *feature = {};

      public:
        String name;
        int id = {};

        std::set<GenericType *>   dependencies;
        std::set<std::string>     directDependencies;
        std::set<GenericType *>   subscribers;
        // std::vector<GenericType>  aliases;
        std::string_view     protect;
        // std::string          version      = {};
        // std::string          tempversion;
        bool                 forceRequired = {};
        GenericType         *aliasParent = {};
        xml::Element         xmlElement;

        GenericType() = default;

        explicit GenericType(MetaType::Value type) noexcept : MetaType(type) {}

        GenericType(MetaType::Value type, std::string_view name, bool firstCapital = false) : name(std::string{ name }, firstCapital), MetaType(type) {}

        GenericType(const GenericType &parent, std::string_view name, bool firstCapital = false);

        bool operator()(const GenericType &other) const noexcept {
            return name.original < other.name.original;
        }

        bool operator<(const GenericType &other) const noexcept {
            return name < other.name;
        }

        std::string_view getProtect() const;

        Platform* getPlatfrom() const;

        std::string getVersionDebug() const;

        Feature *getFeature() const {
            return feature;
        }

        Extension *getExtension() const {
            return ext;
        }

        void setProtect(const std::string_view);

        bool hasProtect() const {
            return !protect.empty();
        }

        bool updateBinding(Feature *feature);

        bool updateBinding(Extension *extension);

        // void bind(Feature *feature, Extension *ext, const std::string_view protect = "");

        bool isEnabled() const {
            return enabled // && supported
            ;
        }

        bool isRequired() const {
            return !subscribers.empty() || forceRequired;
        }

        bool canGenerate() const {
            return // supported &&
            (enabled || isRequired());
        }

        // void addAlias(const std::string_view alias, bool firstCapital) {
        //     aliases.emplace_back(*this, std::string{ alias }, firstCapital);
        // }

        void setEnabled(bool value) {
            if (enabled == value // || !supported
            ) {
                return;
            }
            enabled = value;

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
        void subscribe(GenericType *s) {
            if (!subscribers.contains(s)) {
                bool empty = subscribers.empty();
                subscribers.emplace(s);
                // std::cout << "  subscribed to: " << name << '\n';
                if (empty) {
                    setEnabled(true);
                }
            }
        }

        void unsubscribe(GenericType *s) {
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
    };

}

#endif  // VKCPP_GEN_TYPES_HPP
