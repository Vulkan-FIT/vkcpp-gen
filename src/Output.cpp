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

    std::string UnorderedFunctionOutput::Segment::get(const Generator &g) const {
        std::string out;
        for (const auto &k : guards) {
            out += g.genWithProtectNegate(k.second, k.first);
        }
        return out;
    }

    /*
    std::string UnorderedOutput::get(bool onlyNoProtect) const {
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

    void UnorderedOutput::add(const BaseType &type, std::function<void(std::string &)> function, bool bypass) {
        auto [code, protect] = g.genCodeAndProtect(type, function, bypass);
        segments[protect] += code;
    }
    */

    void UnorderedFunctionOutput::add(const BaseType &type, std::function<void(std::string &)> function, const std::string &guard) {
        if (!type.canGenerate()) {
            return;
        }

        auto [code, protect] = g.genCodeAndProtect(type, function, false);

        segments[protect].add(guard, code);
    }

    void UnorderedFunctionOutputX::add(const BaseType &type, std::function<void(std::string &)> function, const std::string &guard) {
        if (!type.canGenerate()) {
            return;
        }

        std::string out;
        function(out);

        const auto &p = type.getProtect();
        if (guard.empty()) {
            std::array<Protect, 1> pro = {Protect(p, true)};
            get(pro) += out;
        }
        else {
            std::array<Protect, 2> pro = {Protect(guard, true), Protect(p, true)};
            get(pro) += out;
        }
    }

    std::string UnorderedFunctionOutput::get(bool onlyNoProtect) const {
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

    void GenOutput::writeFile(Generator &gen, OutputFile &file) {
        writeFile(gen, file.filename, file.content, true);
    }

    void GenOutput::writeFile(Generator &gen, const std::string &filename, const OutputBuffer &content, bool addProtect) {
        std::string protect;
        if (addProtect) {
            protect = getFileNameProtect(filename) + protectSuffix;
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
            output << "#define " << protect << "\n";
        }

        content.write(output);

        if (!protect.empty()) {
            output << "#endif // " + protect + "\n";
        }

        // std::cout << "Generated: " << p << ", reserved: " << content.size() << "B\n";
    }


    UnorderedFunctionOutputX::UnorderedFunctionOutputX() {
        output = std::make_unique<OutputBuffer>();
    }

    void UnorderedFunctionOutputX::clear() {
        output = std::make_unique<OutputBuffer>(); // TODO
        segments.clear();
    }

    size_t UnorderedFunctionOutputX::size() const {
        size_t s = output->size();
        for (const auto &k : segments) {
            s += k.second.size();
        }
        return s;
    }

    std::string UnorderedFunctionOutputX::toString() const {
        std::stringstream s;
        write(s);
        return s.str();
    }

    OutputBuffer &UnorderedFunctionOutputX::get(const std::span<Protect> protects) {
        UnorderedFunctionOutputX *output = this;
        for (const auto &p : protects) {
            if (!p.first.empty()) {
                output = &output->get(p.first, p.second);
            }
        }
        return output->get();
    }

    void UnorderedFunctionOutputX::write(std::ostream &os) const {
        output->write(os);
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

    OutputBuffer::OutputBuffer() {
        emplace();  // sentinel
    }

    OutputBuffer::OutputBuffer(std::string &&str) {
        list.emplace_back(UnmutableString{ std::move(str) });
    }

    size_t OutputBuffer::size() const {
        return m_size;
    }

    std::string &OutputBuffer::emplace() {
        auto &s = list.emplace_back(std::string{});
        return std::get<std::string>(s);
    }

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

    void OutputBuffer::write(std::ostream &stream) const {
        for (const auto &l : list) {
            std::visit([&](auto &&arg) { stream << arg; }, l);
        }
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

    OutputBuffer &OutputBuffer::operator+=(UnorderedFunctionOutputX &&out) {
        m_size += out.size();
        list.emplace_back(std::move(out));
        return *this;
    }

}  // namespace vkgen
