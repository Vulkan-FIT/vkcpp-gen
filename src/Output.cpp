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

#include "Output.hpp"

#include "Generator.hpp"

#include <fstream>
#include <ostream>

namespace vkgen
{

    void GuardedOutput::add(const GenericType &type, std::function<void(OutputBuffer&)> function, const std::string &guard) {
        if (!type.canGenerate()) {
            return;
        }
        const auto &p = type.getProtect();
        if (guard.empty()) {
            std::array<Protect, 1> pro = {Protect(p, true)};
            function(get(pro));
        }
        else {
            std::array<Protect, 2> pro = {Protect(guard, true), Protect(p, true)};
            function(get(pro));
        }
    }

//    void GuardedOutput::add(const GenericType &type, std::function<void(std::string &)> function, const std::string &guard) {
//        if (!type.canGenerate()) {
//            return;
//        }
//
//        std::string out;
//        function(out);
//
//        const auto &p = type.getProtect();
//        if (guard.empty()) {
//            std::array<Protect, 1> pro = {Protect(p, true)};
//            get(pro) += out;
//        }
//        else {
//            std::array<Protect, 2> pro = {Protect(guard, true), Protect(p, true)};
//            get(pro) += out;
//        }
//    }

    void GenOutput::writeFile(Generator &gen, OutputFile &file) {
        writeFile(gen, file.filename, file.content, true);
    }

    std::string GenOutput::getFileNameProtect(const std::string_view name, bool cguard) {
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

    void GenOutput::writeFile(Generator &gen, const std::string_view filename, const OutputBuffer &content, bool addProtect) {
        std::string protect;
        if (addProtect) {
            protect = getFileNameProtect(filename, cguard);
        }
        std::ofstream output;

        auto p = std::filesystem::path(this->path).replace_filename(filename);
        output.open(p, std::ios::out | std::ios::trunc);
        if (!output.is_open()) {
            throw std::runtime_error("Can't open file: " + p.string());
        }
        std::filesystem::resize_file(p, content.size());

        if (!protect.empty()) {
            output << "#ifndef " << protect << "\n";
            output << "#define " << protect;
            if (cguard) {
                output << " 1";
            }
            output << "\n\n";
        }

        output <<
          R"(/*
** Copyright 2015-2024 The Khronos Group Inc.
**
** SPDX-License-Identifier: Apache-2.0
*/

/*
** This header is generated from the Khronos Vulkan XML API Registry.
**
*/
)";

        if (cguard) {
            output << R"(
#ifdef __cplusplus
extern "C" {
#endif
)";
        }

        output << content;

        if (cguard) {
            output << R"(
#ifdef __cplusplus
}
#endif
)";
        }

        if (!protect.empty()) {
            output << "#endif // " + protect + "\n";
        }

        // std::cout << "Generated: " << p << ", reserved: " << content.size() << "B\n";
    }

    GuardedOutput::GuardedOutput() {
        output = std::make_unique<OutputBuffer>();
    }

    void GuardedOutput::clear() {
        if (output) {
            output->clear();
        }
        else {
            output = std::make_unique<OutputBuffer>();
        }
        segments.clear();
    }

    size_t GuardedOutput::size() const {
        size_t s = 0;
        if (output) {
            s += output->size();
        }
        for (const auto &k : segments) {
            s += k.second.size();
        }
        return s;
    }

    std::string GuardedOutput::toString() const {
        std::stringstream s;
        write(s);
        return s.str();
    }

    OutputBuffer &GuardedOutput::get(const std::span<Protect> protects) {
        GuardedOutput *output = this;
        for (const auto &p : protects) {
            if (!p.first.empty()) {
                output = &output->get(p.first, p.second);
            }
        }
        return output->get();
    }

    void GuardedOutput::write(std::ostream &os) const {
        os << *output;
        for (const auto &s : segments) {
            if (s.second.ifdef) {
                os << "#ifdef ";
            } else {
                os << "#ifndef ";
            }
            os << s.first << '\n';
            s.second.write(os);
            os << "#endif // " << s.first << '\n';
        }
    }

    std::ostream &operator<<(std::ostream &os, const vkgen::OutputClass &c) {
        os << "  class " << c.name;
        if (!c.inherits.empty()) {
            os << " : " << c.inherits;
        }
        os << " {\n";

        const auto addSection = [&](const std::string &visibility, const GuardedOutput &segment) {
            if (segment.size() > 0) {
                if (!visibility.empty()) {
                    os << "  " + visibility + ":\n";
                }
                os << segment;
            }
        };

        addSection("public", c.sPublic);
        addSection("", c.sFuncs);
        addSection("private", c.sPrivate);
        addSection("protected", c.sProtected);
        os << "  };\n";

        return os;
    }

    OutputBuffer::OutputBuffer() {
        this->operator+=(std::string_view{}); // sentinel
    }

    OutputBuffer::OutputBuffer(std::string &&str) {
        list.emplace_back(UnmutableString{ std::move(str) });
    }

    size_t OutputBuffer::size() const {
        return m_size;
    }

    void OutputBuffer::clear() {
        list.clear();
    }

//    std::string &OutputBuffer::emplace() {
//        auto &s = list.emplace_back(std::string{});
//        return std::get<std::string>(s);
//    }

    void OutputBuffer::print() const {
        std::cout << "-- Out buffer {" << std::endl;
        for (const auto &l : list) {
            std::cout << "<" << l.index() << ">";
            std::visit([&](auto &&arg) { std::cout << ", " << arg.size() << "B"; }, l);
            std::cout << std::endl;
        }
        std::cout << "}" << std::endl;

        //    std::string output;
        //    for (const auto &l : list) {
        //        output += l;
        //    }
        //    std::cout << output << std::endl;
    }

    OutputBuffer &OutputBuffer::operator+=(const std::string_view str) {
        list.emplace_back(std::string_view{ str });
        m_size += str.size();
        return *this;
    }

    OutputBuffer &OutputBuffer::operator+=(std::string &&str) {
        m_size += str.size();
        list.emplace_back(vkgen::UnmutableString{ std::move(str) });
        return *this;
    }

    OutputBuffer &OutputBuffer::operator+=(const std::string &str) {
        if (list.rbegin()->index() == 0) {
            std::get<std::string>(*list.rbegin()) += str;
            m_size += str.size();
        } else {
            list.emplace_back(std::string{ str });
            m_size += str.size();
        }
        return *this;
    }

    OutputBuffer &OutputBuffer::operator+=(const char * const str) {
        const auto &s = std::string::traits_type::length(str);
        list.emplace_back(std::string_view{ str, s });
        m_size += s;
        return *this;
    }

    OutputBuffer &OutputBuffer::operator+=(OutputBuffer &&out) {
        m_size += out.size();
        list.emplace_back(std::move(out));
        return *this;
    }

    OutputBuffer &OutputBuffer::operator+=(GuardedOutput &&out) {
        m_size += out.size();
        list.emplace_back(std::move(out));
        return *this;
    }

    OutputBuffer &OutputBuffer::operator+=(OutputClass &&out) {
        m_size += out.size();
        list.emplace_back(std::move(out));
        return *this;
    }

    std::ostream &operator<<(std::ostream &os, const vkgen::OutputBuffer &s) {
        for (const auto &l : s.list) {
            std::visit([&](auto &&arg) { os << arg; }, l);
        }
        return os;
    }

}  // namespace vkgen
