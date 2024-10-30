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

#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include "Config.hpp"
#include "Members.hpp"
#include "Output.hpp"
#include "Registry.hpp"

#include <string>
#include <optional>
#include <variant>
#include <vector>

namespace vkgen
{

    using namespace Utils;

    /*
    struct FunctionGenerator {
        using StringSource = std::variant<const char*, const std::string_view, std::string>;

        StringSource type;
    };
    */

    class FunctionGenerator
    {
      protected:
        using ArgVar = std::variant<Argument, const VariableData*>;
        struct ArgInit {
            std::string dst;
            std::string src;
        };

        const Generator &gen;
        std::vector<ArgVar> arguments;
        std::vector<ArgInit> inits;

        std::string getTemplate() const;

        void generatePrefix(std::string &output, bool declaration, bool isInline = false);

        void generateSuffix(std::string &output, bool declaration);

        void generateArguments(std::string &output, bool declaration);

        void generateArgument(std::string &output, const Argument &arg, bool declaration);

        void generateArgument(std::string &output, const VariableData *var, bool declaration);

        void generatePrototype(std::string &output, bool declaration, bool isInline = false);

        // TODO
        std::string generate(bool declaration, bool isInline = false);

      public:
        explicit FunctionGenerator(const Generator &gen) noexcept : gen(gen) {}

        FunctionGenerator(const Generator &gen, const std::string &type, const std::string &name) noexcept
          : gen(gen), type(type), name(name)
        {}

        std::string generate();

        std::string generate(GuardedOutputFuncs &impl);

        Argument& add(const std::string &type, const std::string &id, const std::string &assignment = "") {
            return get<Argument>(arguments.emplace_back(Argument{type, id, assignment}));
        }

        void addInit(const std::string &dst, const std::string &src) {
            inits.emplace_back(dst, src);
        }

        std::string      type;
        std::string      name;
        std::string      code;
        std::string      className;
        std::string      additonalTemplate;
        std::string      indent = "    ";
        Protect          optionalProtect;
        const GenericType *base = nullptr;
        bool          allowInline          = true;
        bool          specifierInline      = {};
        bool          specifierExplicit    = {};
        bool          specifierNoexcept    = {};
        bool          specifierConst       = {};
        bool          specifierConstexpr   = {};
        bool          specifierConstexpr14 = {};
    };

    class Generator : public VulkanRegistry
    {
      public:
        struct ClassVariables
        {
            VariableData raiiAllocator;
            VariableData raiiInstanceDispatch;
            VariableData raiiDeviceDispatch;
            VariableData uniqueAllocator;
            VariableData uniqueDispatch;
        };

        Config         cfg;
        ClassVariables cvars;

        std::string                            m_ns;
        std::string                            m_ns_raii;
        std::string                            m_cast;
        std::string                            m_constexpr;
        std::string                            m_constexpr14;
        std::string                            m_inline;
        std::string                            m_noexcept;
        std::string                            m_nodiscard;


        std::string outputFilePath;

        GuardedOutputFuncs outputFuncs;
        GuardedOutputFuncs outputFuncsRAII;

        // std::string genWithProtect(const std::string &code, const std::string &protect) const;

        // std::string genWithProtectNegate(const std::string &code, const std::string &protect) const;

//        std::pair<std::string, std::string>
//          genCodeAndProtect(const GenericType &type, const std::function<void(std::string &)> &function, bool bypass = false) const;

        void genOptional(OutputBuffer &output, const GenericType &type, const std::function<void(OutputBuffer&)> &function) const;

        // std::string genOptional(const GenericType &type, std::function<void(std::string &)> function, bool bypass = false) const;

        void genPlatform(OutputBuffer &output, const GenericType &type, const std::function<void(OutputBuffer&)> &function);
        // std::string genPlatform(const GenericType &type, std::function<void(std::string &)> function, bool bypass = false);

        // std::string gen(const Define &define,  std::function<void(std::string &)> function) const;

        // std::string gen(const NDefine &define, std::function<void(std::string &)> function) const;

        void gen(OutputBuffer &output, const Define &define, const std::function<void(OutputBuffer&)> &function) const;

        std::string genNamespaceMacro(const Macro &m);

        std::string generateDefines();

        std::string generateHeader();

        void generateFlags(OutputBuffer &) const;

        void generateMainFile(OutputBuffer &);

        void generateModuleEnums(OutputBuffer &);

        void generateModuleStructs(OutputBuffer &);

        void generateModuleHandles(OutputBuffer &);

        void generateModules(GenOutput &, std::filesystem::path);

        void wrapNamespace(OutputBuffer &output, std::function<void(OutputBuffer &)> func);

        void generateApiVideo(std::filesystem::path path);

        void generateApiC(std::filesystem::path path);

        void generateApiCpp(std::filesystem::path path);

        void generateForwardHandles(OutputBuffer &output);

        void generateEnumStr(const Enum &data, OutputBuffer &output, OutputBuffer &to_string_output);

        void generateEnum(const Enum &data, OutputBuffer &output, OutputBuffer &output_forward, OutputBuffer &to_string_output);

        std::string generateToStringInclude() const;

        void generateCore(OutputBuffer &output);

        void generateEnums(OutputBuffer &output, OutputBuffer &output_forward, OutputBuffer &to_string_output);

        void genFlagTraits(const Enum &data, std::string name, OutputBuffer &output, std::string &to_string_code);

        void generateDispatch(OutputBuffer &output);

        void generateErrorClasses(OutputBuffer &output);

        void generateResultValue(OutputBuffer &output);

        void generateApiConstants(OutputBuffer &output);

        std::string generateDispatchLoaderBase();

        void generateDispatchLoaderStatic(OutputBuffer &output);

        bool useDispatchLoader() const {
            const auto &cfg = getConfig();
            return cfg.gen.dispatchLoaderStatic && !cfg.gen.useStaticCommands;
        }

        std::string getDispatchArgument(bool assignment) const {
            if (!cfg.gen.dispatchParam) {
                return "";
            }
            std::string out = "::VULKAN_HPP_NAMESPACE::DispatchLoaderStatic const &d";
            if (assignment) {
                out += " " + cfg.macro.mDispatch.get();
            }
            return out;
        }

        std::string getDispatchType() const {
            if (cfg.gen.dispatchTemplate) {
                return "Dispatch";
            }
            if (cfg.macro.mDispatchType.usesDefine) {
                return cfg.macro.mDispatchType.define;
            }
            return cfg.macro.mDispatchType.value;
        }

        Argument getDispatchArgument() const {
            if (!cfg.gen.dispatchParam) {
                return Argument("", "");
            }
            std::string assignment = cfg.macro.mDispatch.get();
            if (!assignment.empty()) {
                assignment = " " + assignment;
            }
            auto type = "::VULKAN_HPP_NAMESPACE::DispatchLoaderStatic const &";
            return Argument(type, "d", assignment);
        }

        std::string getDispatchCall(const std::string &var = "d.") const {
            if (cfg.gen.dispatchParam) {
                return var;
            }
            return "::";
        }

        void generateStructDecl(OutputBuffer &output, const Struct &d) const;

        void generateClassDecl(OutputBuffer &output, const Handle &data) const;

        void generateClassDecl(OutputBuffer &output, const Handle &data, const std::string &name) const;

        // std::string generateClassString(const std::string &className, const OutputClass &from, Namespace ns) const;

        std::string generateForwardInclude(GenOutput &out) const;

        void generateHandles(OutputBuffer &output, OutputBuffer &output_smart, GenOutput &out);

        void generateUniqueHandles(OutputBuffer &output);

        std::string generateStructsInclude() const;

        void generateStructs(OutputBuffer &output, bool exp = false);

        void generateStructChains(OutputBuffer &output, bool ctype = false);

        bool generateStructConstructor(OutputBuffer &output, const Struct &data, bool transform);

        void generateStruct(OutputBuffer &output, const Struct &data, bool exp);

        std::string generateIncludeRAII(GenOutput &out) const;

        void generateClassWithPFN(OutputBuffer &output, Handle &h);

        void generateContext(OutputBuffer &output);

        void generateRAII(OutputBuffer &output, OutputBuffer &output_forward, GenOutput &out);

        void generateExperimentalRAII(OutputBuffer &output, GenOutput &out);

        void generateFuncsRAII(OutputBuffer &output);

        void generateContextMembers(const Handle &h, bool vma, OutputBuffer &output, std::string &init);

        void generateDispatchRAII(OutputBuffer &output);

        void evalCommand(Command &ctx) const;

        static Command::NameCategory evalNameCategory(const std::string &name);

        static bool isTypePointer(const VariableData &m) {
            return strContains(m.suffix(), "*");
        }

        ArraySizeArgument evalArraySizeArgument(const VariableData &m) {
            if (m.identifier().ends_with("Count")) {
                return isTypePointer(m) ? ArraySizeArgument::COUNT : ArraySizeArgument::CONST_COUNT;
            }
            if (m.identifier().ends_with("Size")) {
                return ArraySizeArgument::SIZE;
            }
            return ArraySizeArgument::INVALID;
        }

        bool isPointerToCArray(const std::string &id) {
            if (id.size() >= 2) {
                if (id[0] == 'p' && std::isupper(id[1])) {
                    return true;
                }
            }
            return false;
        }

        std::string stripStartingP(const std::string &str) {
            std::string out = str;
            if (isPointerToCArray(str)) {
                out = std::tolower(str[1]);
                out += str.substr(2);
            }
            return out;
        }

        void generateClassMember(ClassCommand            &m,
                                 MemberContext           &ctx,
                                 OutputClass          &out,
                                 GuardedOutputFuncs &outFuncs,
                                 bool                     inlineFuncs = false);

        void generateClassMembers(const Handle            &data,
                                  OutputClass          &out,
                                  GuardedOutputFuncs &outFuncs,
                                  Namespace                ns,
                                  bool                     inlineFuncs = false);

        void generateClassConstructors(const Handle &data, OutputClass &out);

        void generateClassConstructorsRAII(const Handle &data, OutputClass &out);

        void generateUniqueClassStr(OutputBuffer &output, const Handle &data, bool inlineFuncs);

        // std::string generateUniqueClass(const Handle &data);

        void generateUniqueClass(OutputBuffer &output, const Handle &data);

        void generateClassTypeInfo(const Handle &h, OutputBuffer &output, OutputClass &out);

        void generateClass(OutputBuffer &output,
                           const Handle            &data,
                                     bool                     inlineFuncs,
                                     bool                     noFuncs = false);

        void generateClassRAII(OutputBuffer &output, const Handle &data, bool asUnique = false);

        void generateClassesRAII(OutputBuffer &output, bool exp = false);

        // std::string generatePFNs(const Handle &data, OutputClass &out) const;

        void generateLoader(OutputBuffer &output, bool exp = false);

        std::string genMacro(const Macro &m);

        std::string beginNamespace(bool noExport = false) const;

        std::string beginNamespaceRAII(bool noExport = false) const;

        std::string beginNamespace(const Macro &ns, bool noExport = false) const;

        std::string endNamespace() const;

        std::string endNamespaceRAII() const;

        std::string endNamespace(const Macro &ns) const;

        std::string expIfndef(const std::string &name) const {
            if (!cfg.gen.expApi) {
                return "";
            }
            return "#ifndef " + name + "\n";
        }

        std::string expEndif(const std::string &name) const {
            if (!cfg.gen.expApi) {
                return "";
            }
            return "#endif // " + name + "\n";
        }

      public:
        Generator();

        void resetConfig();

        void loadConfigPreset();

        void setOutputFilePath(const std::string &path);

        bool isOuputFilepathValid() const {
            return std::filesystem::is_directory(outputFilePath);
        }

        std::string getOutputFilePath() const {
            return outputFilePath;
        }

        bool load(const std::string &xmlPath);

        void generate();

        std::string_view getNamespace(Namespace ns) const;

        Platforms &getPlatforms() {
            return platforms;
        };

        Extensions &getExtensions() {
            return extensions;
        };

        Features &getFeatures() {
            return features;
        };

        auto &getHandles() {
            return handles;
        }

        auto &getCommands() {
            return commands;
        }

        auto &getStructs() {
            return structs;
        }

        auto &getEnums() {
            return enums;
        }

        Config &getConfig() {
            return cfg;
        }

        const Config &getConfig() const {
            return cfg;
        }

        void saveConfigFile(const std::string &filename);

        void loadConfigFile(const std::string &filename);
    };

}  // namespace vkgen
#endif  // GENERATOR_HPP
