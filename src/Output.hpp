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

#ifndef GENERATOR_OUTPUT_HPP
#define GENERATOR_OUTPUT_HPP

#include <filesystem>
#include <functional>
#include <list>
#include <map>
#include <span>
#include <variant>

namespace vkgen
{

    class OutputBuffer;
    struct BaseType;
    class Generator;

    class UnorderedFunctionOutput
    {
        const Generator &g;

        struct Segment
        {
            std::map<std::string, std::string> guards;

            void add(const std::string &guard, const std::string &code) {
                guards[guard] += code;
            }

            std::string get(const Generator &g) const;
        };

      public:
        std::map<std::string, Segment> segments;

        UnorderedFunctionOutput(const Generator &gen) : g(gen) {}

        void add(const BaseType &type, std::function<void(std::string &)> function, const std::string &guard);

        void clear() {
            segments.clear();
        }

        UnorderedFunctionOutput &operator+=(const std::string &str) {
            segments[""].add("", str);
            return *this;
        }

        std::string get(bool onlyNoProtect = false) const;
    };

    using Protect = std::pair<std::string, bool>;

    class UnorderedFunctionOutputX
    {
        std::unique_ptr<OutputBuffer>                   output;
        std::map<std::string, UnorderedFunctionOutputX> segments;
        bool                                            ifdef = {};

        OutputBuffer &get() {
            return *output;
        }

        UnorderedFunctionOutputX &get(const std::string &protect, bool ifdef) {
            auto &s = segments[protect];
            s.ifdef = ifdef;
            return s;
        }

      public:
        UnorderedFunctionOutputX();

        OutputBuffer &operator*() {
            return *output;
        }

        template <typename T>
        UnorderedFunctionOutputX &operator+=(T &&v) {
            *output += std::forward<T>(v);
            return *this;
        }

        size_t size() const;

        void clear();

        void add(const BaseType &type, std::function<void(std::string &)> function, const std::string &guard = "");

        OutputBuffer &get(std::span<Protect> protects);

        void write(std::ostream &os) const;

        std::string toString() const;
    };

    struct UnorderedFunctionOutputGroup
    {
        UnorderedFunctionOutputX def;
        UnorderedFunctionOutputX templ;
        UnorderedFunctionOutputX platform;

        void clear() {
            def.clear();
            templ.clear();
            platform.clear();
        }

    };

    inline std::ostream &operator<<(std::ostream &os, const vkgen::UnorderedFunctionOutputX &s) {
        s.write(os);
        return os;
    }

    // class OutputSegment {
    // public:
    // };
    //
    // inline std::ostream& operator<<(std::ostream& os, const OutputSegment& s) {
    //
    //     return os;
    // }

    class UnmutableString : public std::string
    {
      public:
        UnmutableString(std::string &&str) : std::string(std::move(str)) {}
    };

    // inline std::ostream& operator<<(std::ostream& os, const vkgen::UnmutableString& s) {
    //     os << static_cast<std::string>(s);
    //     return os;
    // }

    /*
    class UnorderedOutput
    {
        const Generator &g;

      public:
        std::map<std::string, std::string> segments;

        UnorderedOutput(const Generator &gen) : g(gen){};

        std::string get(bool onlyNoProtect = false) const;

        void clear() {
            segments.clear();
        }

        void add(const BaseType &type, std::function<void(std::string &)> function, bool bypass = false);

        UnorderedOutput &operator+=(const std::string &str) {
            segments[""] += str;
            return *this;
        }
    };
    */

    class OutputBuffer
    {
        using Segment = std::variant<std::string, std::string_view, UnmutableString, OutputBuffer, UnorderedFunctionOutputX>;
        std::list<Segment> list;
        size_t             m_size = {};

      public:
        OutputBuffer();

        OutputBuffer(std::string &&str);

        size_t size() const;

        std::string &emplace();  // deprecate

        void write(std::ostream &stream) const;

        OutputBuffer &operator+=(std::string_view str);

        OutputBuffer &operator+=(std::string &&str);

        OutputBuffer &operator+=(const std::string &str);

        OutputBuffer &operator+=(const char *str);

        OutputBuffer &operator+=(UnorderedFunctionOutputX &&);

        void print() const;
    };

    inline std::ostream &operator<<(std::ostream &os, const vkgen::OutputBuffer &s) {
        s.write(os);
        return os;
    }

    struct OutputFile
    {
        OutputBuffer content;
        std::string  filename;

        OutputFile(const std::string_view filename) : filename(filename) {}

        //        OutputFile& operator+=(const std::string& str){
        //            content += str;
        //            return *this;
        //        }
    };

    struct GenOutputClass
    {
        // GenOutputClass(const Generator &gen)
        //: sFuncs(gen), sFuncsEnhanced(gen), sPublic(gen), sPrivate(gen), sProtected(gen)
        // {}

        UnorderedFunctionOutputX sFuncs;
        UnorderedFunctionOutputX sPublic;
        UnorderedFunctionOutputX sPrivate;
        UnorderedFunctionOutputX sProtected;
        std::string              inherits;
    };

    struct GenOutput
    {
        const std::string                 prefix;
        const std::string                 extension;
        const std::filesystem::path       path;
        std::map<std::string, OutputFile> files;
        std::string                       protectSuffix;

        GenOutput(const std::string &prefix, const std::string &extension, std::filesystem::path &path) : prefix(prefix), extension(extension), path(path) {}

        auto &addFile(const std::string &suffix, const std::string &extension) {
            auto it = files.try_emplace(suffix, createFilename(suffix, extension));
            return it.first->second.content;
        }

        auto &addFile(const std::string &suffix) {
            return addFile(suffix, extension);
        }

        std::string createFilename(const std::string &suffix, const std::string &extension) const {
            return prefix + suffix + extension;
        }

        std::string createFilename(const std::string &suffix) const {
            return createFilename(suffix, extension);
        }

        const std::string &getFilename(const std::string &suffix) const {
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
            for (const auto &c : name) {
                if (c == '.') {
                    out += '_';
                } else {
                    out += std::toupper(c);
                }
            }
            return out;
        };

        void writeFile(Generator &gen, OutputFile &file);

        void writeFile(Generator &gen, const std::string &filename, const OutputBuffer &content, bool addProtect = true);
    };

}  // namespace vkgen

#endif  // GENERATOR_OUTPUT_HPP
