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
    struct GenericType;
    class OutputBuffer;
    class Generator;

    using Protect = std::pair<std::string, bool>;

    class GuardedOutput
    {
        std::unique_ptr<OutputBuffer>                   output;
        std::map<std::string, GuardedOutput> segments;
        bool                                            ifdef = {};

        GuardedOutput &get(const std::string &protect, bool ifdef) {
            auto &s = segments[protect];
            s.ifdef = ifdef;
            return s;
        }

      public:
        GuardedOutput();

        OutputBuffer &operator*() {
            return *output;
        }

        template <typename T>
        GuardedOutput &operator+=(T &&v) {
            *output += std::forward<T>(v);
            return *this;
        }

        size_t size() const;

        void clear();

        // void add(const GenericType &type, std::function<void(std::string &)> function, const std::string &guard = "");

        void add(const GenericType &type, std::function<void(OutputBuffer &)> function, const std::string &guard = "");

        OutputBuffer &get() {
            return *output;
        }

        OutputBuffer &get(std::span<Protect> protects);

        void write(std::ostream &os) const;

        std::string toString() const;
    };

    struct GuardedOutputFuncs
    {
        GuardedOutput def;
        GuardedOutput templ;
        GuardedOutput platform;

        void clear() {
            def.clear();
            templ.clear();
            platform.clear();
        }

    };

    inline std::ostream &operator<<(std::ostream &os, const vkgen::GuardedOutput &s) {
        s.write(os);
        return os;
    }

    class UnmutableString : public std::string
    {
      public:
        UnmutableString(std::string &&str) : std::string(std::move(str)) {}
    };

    // inline std::ostream& operator<<(std::ostream& os, const vkgen::UnmutableString& s) {
    //     os << static_cast<std::string>(s);
    //     return os;
    // }


    struct OutputClass
    {
        // GenOutputClass(const Generator &gen)
        //: sFuncs(gen), sFuncsEnhanced(gen), sPublic(gen), sPrivate(gen), sProtected(gen)
        // {}

        GuardedOutput sFuncs;
        GuardedOutput sPublic;
        GuardedOutput sPrivate;
        GuardedOutput sProtected;
        std::string   name;
        std::string   inherits;

        size_t size() const {
            return name.size() + sFuncs.size() + sPublic.size() + sPrivate.size() + sProtected.size();
        }

        friend std::ostream & operator<<(std::ostream&, const OutputClass&);
    };

    std::ostream &operator<<(std::ostream &os, const vkgen::OutputClass &s);

    class OutputBuffer
    {
        using Segment = std::variant<std::string, std::string_view, OutputBuffer, GuardedOutput, OutputClass>;
        std::list<Segment> list;
        size_t             m_size = {};

      public:
        OutputBuffer();

        OutputBuffer(std::string &&str);

        size_t size() const;

        void clear();

        // std::string &emplace();  // deprecate

        OutputBuffer &operator+=(std::string_view str);

        OutputBuffer &operator+=(std::string &&str);

        OutputBuffer &operator+=(const std::string &str);

        OutputBuffer &operator+=(const char *str);

        OutputBuffer &operator+=(OutputBuffer &&);

        OutputBuffer &operator+=(GuardedOutput &&);

        OutputBuffer &operator+=(OutputClass &&);

        void print() const;

        friend std::ostream & operator<<(std::ostream&, const OutputBuffer&);
    };

    std::ostream &operator<<(std::ostream &os, const OutputBuffer &s);

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

    struct GenOutput
    {
        const std::string                 prefix;
        const std::string                 extension;
        const std::filesystem::path       path;
        std::map<std::string, OutputFile> files;
        std::string                       protectSuffix;
        bool                              cguard = false;

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
            if (cguard) {
                out += '_';
            }
            return out;
        };

        void writeFile(Generator &gen, OutputFile &file);

        void writeFile(Generator &gen, const std::string &filename, const OutputBuffer &content, bool addProtect = true);
    };

}  // namespace vkgen

#endif  // GENERATOR_OUTPUT_HPP
