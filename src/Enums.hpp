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

#ifndef ENUMS_H
#define ENUMS_H

#include <string>

namespace vkgen
{

    enum class Namespace
    {
        NONE,
        VK,
        RAII,
        STD
    };

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
            Command
        };
        using enum Value;

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
            switch (value) {
                case Enum: return "enum";
                case Struct: return "struct";
                case Union: return "union";
                case Handle: return "class";
                default: return "unknown";
            }
        }

        std::string metaTypeString() const noexcept {
            switch (value) {
                case Enum: return "enum";
                case Struct: return "struct";
                case Union: return "union";
                case Handle: return "handle";
                case Command: return "command";
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
            return value == Enum;
        }

        bool isStruct() const noexcept {
            return value == Struct;
        }

        bool isUnion() const noexcept {
            return value == Union;
        }

        bool isHandle() const noexcept {
            return value == Handle;
        }

        bool isCommand() const noexcept {
            return value == Command;
        }

        bool isStructOrUnion() const noexcept {
            return isStruct() || isUnion();
        }

      private:
        Value value = {};
    };

}  // namespace vkgen

#endif  // ENUMS_H
