#include <iostream>
#include <algorithm>
#include <string>
#include <string_view>
#include <fstream>
#include <filesystem>
#include <cctype>

#include "../Generator.hpp"

#include "imgui.h"

static std::string input;
static std::string outputC;
static std::string outputCpp;

static std::vector<char> readFile(const std::string_view filename) {
    std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + std::string(filename));
    }
    auto              fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

namespace vkgen::tools
{

    std::vector<std::string_view> getStatements(const std::string_view src) {
        std::vector<std::string_view> stmt;
        size_t pos = 0;
        auto it = src.begin();
        while (true) {
            pos = src.find(';', pos);
            if (pos == std::string::npos) {
                size_t s = src.end() - it;
                if (s > 0) {
                    stmt.emplace_back(it, src.begin() + s);
                }
                break;
            }
            pos++;
            auto end = src.begin() + pos;
            stmt.emplace_back(it, end);
            it = end;
            if (pos == src.size()) {
                break;
            }
        }
        return stmt;
    }

    std::string trim(const std::string_view str) {
        if (str.empty()) {
            return "";
        }
        auto begin = str.begin();
        auto end = str.end() - 1;
        while (begin != str.end() && std::isspace(*begin)) {
            ++begin;
        }
        while (end != str.begin() && std::isspace(*end)) {
            --end;
        }
        end++;
        if (begin == end) {
            return "";
        }
        // std::cout << "trim: [" << str << "] -> [" << std::string {begin, end} << "]\n";
        return std::string {begin, end};
    }

    std::string_view getIdentifier(const std::string_view str) {
        auto begin = str.begin();
        while (begin != str.end() && std::isspace(*begin)) {
            ++begin;
        }
        auto end = begin;
        while (end != str.end() && (std::isalnum(*end) || *end == '_')) {
            ++end;
        }
        return std::string_view {begin, end};
    }


    template<typename T>
    using Mapping = std::unordered_map<std::string, std::pair<std::string, const T*>>;

    template <typename T>
    const T* applyMapping(std::string_view &match, Mapping<T> &map) {
        auto it = map.find(std::string{match});
        if (it != map.end()) {
            match = it->second.first;
            return it->second.second;
        }
        return nullptr;
    }

    static Mapping<vkgen::vkr::Struct> m_structs;
    static Mapping<vkgen::GenericType> m_types;
    static Mapping<vkgen::EnumValue> m_enumvalues;

    //        const auto findStruct = [&](const std::string_view str) -> std::string_view {
    //            for (const auto &s : gen.structs) {
    //                if (s.name.original == str) {
    //                    return s.name;
    //                }
    //            }
    //            return "";
    //        };

    struct Context {
        const vkgen::Struct* stateStructDecl = {};

        Mapping<vkgen::vkr::Struct> &m_structs;
        Mapping<vkgen::GenericType> &m_types;
        Mapping<vkgen::EnumValue> &m_enumvalues;

        Context(Mapping<vkgen::Struct> &m_structs,
                Mapping<vkgen::GenericType> &m_types,
                Mapping<vkgen::EnumValue> &m_enumvalues)
          : m_structs(m_structs),  m_types(m_types),  m_enumvalues(m_enumvalues)
        {

        }

        int transform(std::string_view &match) {
            if (const auto *it = applyMapping(match, m_structs)) {
                // std::cout << "-> " << cpp << "\n";
                stateStructDecl = it;
                return 2;
            }
            if (const auto *it = applyMapping(match, m_types)) {
                return 4;
            }
            if (match.starts_with("VK_STRUCTURE_TYPE")) {
                match = "";
                return 3;
            }
            if (const auto *it = applyMapping(match, m_enumvalues)) {
                return 5;
            }
            return 1;
        }
    };

    struct CodeParse {
        std::vector<std::pair<std::string_view, int>> seg;

        CodeParse() = default;

        CodeParse(std::string_view src, Context &ctx) {
            std::string_view sub = "VK";
            auto pos  = src.begin();
            auto prev = src.begin();
            while (true) {
                pos = std::search(
                  pos, src.end(), sub.begin(), sub.end(), [](unsigned char ch1, unsigned char ch2) { return std::toupper(ch1) == std::toupper(ch2); });

                if (pos == src.end()) {
                    break;
                }
                auto end = pos;
                while (end != src.end() && (std::isalnum(*end) || *end == '_')) {
                    ++end;
                }

                size_t n = pos - prev;
                if (n > 0) {
                    seg.emplace_back(std::string_view{ &*prev, n }, 0);
                    // std::cout << std::string_view{ &*prev, n } << "\n";
                }

                auto match = std::string_view{ pos, end };
                // std::cout << "$" << match << "$\n";

                int t = ctx.transform(match);
                seg.emplace_back(match, t);

                prev = end;
                if (end == src.end()) {
                    break;
                }
                ++pos;
            }
            if (prev != src.end()) {
                seg.emplace_back(std::string_view {prev, src.end()}, 10);
            }

        }

        virtual std::string get(bool dbg = false) {
            std::string out;
            for (const auto &s : seg) {
                if (dbg) {
                    out += "<";
                    out += s.second;
                    out += ">";
                }
                out += s.first;
            }
            return out;
        }

    };

    struct StructParse : public CodeParse {

        const vkgen::Struct* src = {};
        std::unordered_map<std::string, std::string> assigns;
        Context ctx;

        StructParse(const vkgen::Struct* src, std::string_view type, std::string_view id, Context ctx) : src(src), ctx(ctx) {
            seg.emplace_back(type, 2);
            seg.emplace_back(id, 1);
        }

        void add(const std::string &id, const std::string &value) {
            assigns[trim(id)] = trim(value);
        }

        void print(bool dbg = false) {
            // CodeParse::print();
            std::cout << "{\n";
            for (const auto &m : src->members) {
                auto it = assigns.find(m->identifier());
                if (it == assigns.end()) {
                    continue;
                }
                auto tmp = ctx;
                CodeParse parse(it->second, tmp);

                std::cout << "  ." << m->identifier() << " = ";
                // std::cout << "[";
                std::cout << parse.get();
                // std::cout << "]\n";
                std::cout << ",\n";
            }
            std::cout << "};\n";
        }
    };

/*
    R"(
VkImageViewCreateInfo depthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
depthStencilView.viewType         = VK_IMAGE_VIEW_TYPE_2D;
depthStencilView.format           = m_offscreenDepthFormat;
depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
depthStencilView.image            = image.image;

vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);
VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
info.renderPass      = m_offscreenRenderPass;
info.attachmentCount = 2;
info.pAttachments    = attachments.data();
info.width           = m_size.width;
info.height          = m_size.height;
info.layers          = 1;
vkCreateFramebuffer(m_device, &info, nullptr, &m_offscreenFramebuffer);
)"
*/

    void parse(const std::string_view input, std::string &outputC, std::string &outputCpp) {

        std::regex  currentStructRgx = std::regex("");
        StructParse* currentStruct = {};

        auto stmt = getStatements(input);

        // std::cout << input << "\n";
        // std::cout << "parse: " << stmt.size() << "\n";

        std::vector<std::unique_ptr<CodeParse>> parse;

        for (auto &src : stmt) {


            Context ctx{ m_structs, m_types, m_enumvalues };

            if (currentStruct) {
                // std::cout << "M:" << std::regex_match(std::string{src}, currentStructRgx) << "\n";
                std::smatch matches;
                std::string s{src};
                if(std::regex_search(s, matches, currentStructRgx)) {
                    // std::cout << "Match found\n";
                    // std::cout << src << "\n";
                    if (matches.size() >= 6) {
                        const auto& id = matches[3];
                        const auto& value = matches[5];
                        currentStruct->add(id, value);
                        continue;
                    }
//                    for (size_t i = 0; i < matches.size(); ++i) {
//                        std::cout << i << ": '" << matches[i].str() << "'\n";
//                    }
                }
            }

            auto p = std::make_unique<CodeParse>(src, ctx);

            // std::cout << "src: " << src << "\n";

            if (ctx.stateStructDecl && true) {
                int i = 0;
                std::string_view type;
                for (const auto &s : p->seg) {
                    if (s.second == 2) {
                        type = s.first;
                        i++;
                        break;
                    }
                    i++;
                }
                if (i < p->seg.size()) {
                    auto id = getIdentifier(p->seg[i].first);
                    if (!id.empty()) {
                        // std::cout << "> " << id << "\n";
                        auto s = std::make_unique<StructParse>(ctx.stateStructDecl, type, id, Context{ m_structs, m_types, m_enumvalues });
                        currentStruct = s.get();
                        currentStructRgx = std::regex("(.|\n)*(" + std::string(id) + ")\\.((.|\n)*)=((.|\n)*);");
                        parse.emplace_back(std::move(s));
                    }
                }
            }
            else {
                parse.emplace_back(std::move(p));
            }


        }

        outputC.resize(0);
        outputCpp.resize(0);

        for (auto &p : parse) {
            // outputC += p->get(false);
            outputCpp += p->get(false);
        }
    }

    void init(vkgen::Generator &gen) {
        m_structs.clear();
        m_types.clear();
        m_enumvalues.clear();
        m_structs.reserve(gen.structs.size());
        for (const auto &s : gen.structs) {
            m_structs.emplace(s.name.original, std::make_pair("vk::" + s.name, &s));
            for (const auto &a : s.aliases) {
                m_structs.emplace(a.name.original, std::make_pair("vk::" + a.name, &s));
            }
        }
        for (const auto &s : gen.enums) {
            for (const auto &m : s.members) {
                // std::cout << m.name.original <<  " -> " << m.name << "\n";
                const auto &name = s.isBitmask()? s.bitmask : s.name;
                m_enumvalues.emplace(m.name.original, std::make_pair("vk::" + name + "::" + m.name, &m));
            }
        }

        for (const auto &t : gen.types) {
            if (t.second->isStruct()) {
                continue;
            }
            const auto &s = *t.second;
            std::string prefix = t.second->isCommand() ? "" : "vk::";
            m_types.emplace(s.name.original, std::make_pair(prefix + s.name, t.second));
            // std::cout << s.name.original <<  " -> " << prefix << s.name << "\n";
            for (const auto &a : s.aliases) {
                m_types.emplace(a.name.original, std::make_pair(prefix + a.name, t.second));
            }
        }

        size_t size = 4096;
        input.resize(size);
        outputC.resize(size);
        outputCpp.resize(size);
    }

    static std::string_view::size_type parseCallSubstring(const std::string_view code, std::string_view::size_type pos) {
        std::string_view::size_type end = code.size();
        const char* str = code.data() + pos;
        while (pos < end && std::isspace(*str)) {
            str++;
            pos++;
        }
        while (pos < end && std::isalnum(*str)) {
            str++;
            pos++;
        }
        if (pos == end) {
            return std::string_view::npos;
        }
        return pos;
    }

    void analyzeCode(vkgen::Generator &gen, std::string_view dir) {
        auto extfilter = std::array{
            ".cpp",
            ".hpp"
        };
        using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

        std::unordered_map <std::string, std::pair<uint64_t, const vkgen::Command*>> functions;
        functions.reserve(gen.commands.size());
        for (const auto &s : gen.commands) {
            // std::cout << s.name << "\n";
            functions.emplace(s.name, std::make_pair(0, &s));
            for (const auto &a : s.aliases) {
                // std::cout << a.name << "\n";
                functions.emplace(a.name, std::make_pair(0, &s));
            }
        }

        auto path = std::filesystem::absolute(dir);
        for (const auto& entry : recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                auto ext = std::filesystem::path(entry).extension();
                if (std::find(extfilter.begin(), extfilter.end(), ext) != extfilter.end()) {
                    std::cout << "Scanning " << entry << "\n";

                    std::ifstream t(entry.path().string());
                    std::stringstream buffer;
                    buffer << t.rdbuf();

                    std::string_view code = buffer.view();
                    std::string_view::size_type pos = 0;
                    while (true) {
                        pos = code.find("vk::", pos);
                        if (pos == std::string_view::npos) {
                            break;
                        }
                        auto start = pos + 4;
                        pos = parseCallSubstring(code.data(), start);
                        if (pos == std::string_view::npos) {
                            break;
                        }
                        // if (code[pos] == '(') {
                            auto             len = pos - start;
                            std::string_view str = code.substr(start, len);
                            // std::cout << "[" << str << "]\n";
                            auto f = functions.find(std::string(str));
                            if (f != functions.end()) {
                                f->second.first++;
                            }

                        // }

                    }

                }
            }
        }


        std::unordered_set<std::string> extensions;

        uint32_t count = 0;
        for (const auto &f : functions) {
            if (f.second.first > 0) {
                count++;

                const auto &command = *f.second.second;

                auto* ext = command.getExtension();
                if (ext) {
                    extensions.insert(ext->name.original);
                }
                // auto* feature = command.getFeature();
            }
        }


        std::cout << "Found " << count << " calls \n";
        for (const auto &f : functions) {
            auto count = f.second.first;
            if (count > 0) {
                std::cout << f.second.second->name.original << "\n"; // << " {" << count << "}\n";
            }
        }

        if (!extensions.empty()) {
            std::cout << "\nFound extensions:\n";
            for (const auto &e : extensions) {
                std::cout << e << "\n";
            }
        }

    }

}

namespace vkgen
{

    void toolScreen() {

        ImGui::PushID(1);
        bool c = ImGui::InputTextMultiline("", input.data(), input.size());
        if (c) {
            vkgen::tools::parse(input, outputC, outputCpp);
        }
        ImGui::PopID();

        ImGui::PushID(2);
        // ImGui::InputTextMultiline("", outputC.data(), outputC.size());
        ImGui::PopID();

        ImGui::PushID(3);
        ImGui::InputTextMultiline("", outputCpp.data(), outputCpp.size());
        ImGui::PopID();

    }

}


