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
#include "Format.hpp"
#include "Members.hpp"
#include "Output.hpp"
#include "Registry.hpp"

#include <filesystem>
#include <forward_list>
#include <iostream>
#include <optional>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace vkgen
{

    using namespace Utils;

    class Generator : public Registry
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
        bool           dispatchLoaderBaseGenerated;

        std::string                            outputFilePath;
        UnorderedFunctionOutput                outputFuncs;
        UnorderedFunctionOutput                outputFuncsEnhanced;
        UnorderedFunctionOutput                outputFuncsRAII;
        UnorderedFunctionOutput                outputFuncsEnhancedRAII;
        UnorderedFunctionOutput                outputFuncsInterop;
        UnorderedOutput                        platformOutput;
        std::unordered_map<Namespace, Macro *> namespaces;

        std::string genWithProtect(const std::string &code, const std::string &protect) const;

        std::string genWithProtectNegate(const std::string &code, const std::string &protect) const;

        std::pair<std::string, std::string>
          genCodeAndProtect(const BaseType &type, const std::function<void(std::string &)> &function, bool bypass = false) const;

        std::string genOptional(const BaseType &type, std::function<void(std::string &)> function, bool bypass = false) const;

        std::string gen(const Define &define, std::function<void(std::string &)> function) const;

        std::string gen(const NDefine &define, std::function<void(std::string &)> function) const;

        std::string genNamespaceMacro(const Macro &m);

        std::string generateDefines();

        std::string generateHeader();

        void generateFlags(OutputBuffer &) const;

        void generateMainFile(OutputBuffer &);

        std::pair<std::string, std::string> generateModuleFunctions(GenOutput &);

        void generateModuleEnums(OutputBuffer &);

        void generateModuleStructs(OutputBuffer &);

        void generateModuleHandles(OutputBuffer &);

        void generateModules(GenOutput &, std::filesystem::path);

        void wrapNamespace(OutputBuffer &output, std::function<void(OutputBuffer &)> func);

        void generateFiles(std::filesystem::path path);

        void generateForwardHandles(OutputBuffer &output);

        void generateEnumStr(const Enum &data, std::string &output, std::string &to_string_output);

        void generateEnum(const Enum &data, OutputBuffer &output, OutputBuffer &output_forward, OutputBuffer &to_string_output);

        std::string generateToStringInclude() const;

        void generateEnums(OutputBuffer &output, OutputBuffer &output_forward, OutputBuffer &to_string_output);

        std::string genFlagTraits(const Enum &data, std::string name, std::string &to_string_output);

        std::string generateDispatch();

        std::string generateErrorClasses();

        std::string generateApiConstants();

        std::string generateDispatchLoaderBase();

        std::string generateDispatchLoaderStatic();

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

        std::string generateStructDecl(const Struct &d) const;

        std::string generateClassDecl(const Handle &data) const;

        std::string generateClassDecl(const Handle &data, const std::string &name) const;

        std::string generateClassString(const std::string &className, const GenOutputClass &from, Namespace ns) const;

        std::string generateForwardInclude(GenOutput &out) const;

        void generateHandles(OutputBuffer &output, OutputBuffer &output_smart, GenOutput &out, std::string &funcs);

        void generateUniqueHandles(OutputBuffer &output);

        std::string generateStructsInclude() const;

        std::string generateStructs(bool exp = false);

        void generateStructChains(OutputBuffer &output);

        bool generateStructConstructor(const Struct &data, bool transform, std::string &output);

        std::string generateStruct(const Struct &data, bool exp);

        std::string generateIncludeRAII(GenOutput &out) const;

        std::string generateClassWithPFN(Handle &h, UnorderedFunctionOutput &funcs, UnorderedFunctionOutputX &funcs2);

        void generateContext(OutputBuffer &output, UnorderedFunctionOutput &inter);

        void generateRAII(OutputBuffer &output, OutputBuffer &output_forward, GenOutput &out);

        void generateExperimentalRAII(OutputBuffer &output, GenOutput &out);

        void generateFuncsRAII(OutputBuffer &output);

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
                                 GenOutputClass          &out,
                                 UnorderedFunctionOutput &funcs,
                                 UnorderedFunctionOutput &funcsEnhanced,
                                 bool                     inlineFuncs = false);

        void generateClassMembers(const Handle            &data,
                                  GenOutputClass          &out,
                                  UnorderedFunctionOutput &funcs,
                                  UnorderedFunctionOutput &funcsEnhanced,
                                  Namespace                ns,
                                  bool                     inlineFuncs = false);

        void generateClassConstructors(const Handle &data, GenOutputClass &out, UnorderedFunctionOutput &funcs);

        void generateClassConstructorsRAII(const Handle &data, GenOutputClass &out, UnorderedFunctionOutput &funcs);

        std::string generateUniqueClassStr(const Handle &data, UnorderedFunctionOutput &funcs, bool inlineFuncs);

        std::string generateUniqueClass(const Handle &data, UnorderedFunctionOutput &funcs);

        std::string
          generateClass(const Handle &data, UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, UnorderedFunctionOutput &expfuncs);

        std::string generateClassStr(const Handle            &data,
                                     UnorderedFunctionOutput &funcs,
                                     UnorderedFunctionOutput &funcsEnhanced,
                                     UnorderedFunctionOutput &expfuncs,
                                     bool                     inlineFuncs,
                                     bool                     noFuncs = false);

        std::string generateClassStrRAII(const Handle &data, bool exp = false);

        std::string generateClassRAII(const Handle &data, bool exp = false);

        std::string generatePFNs(const Handle &data, GenOutputClass &out) const;

        std::string generateLoader(bool exp = false);

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

        void load(const std::string &xmlPath);

        void generate();

        Platforms &getPlatforms() {
            return platforms;
        };

        Extensions &getExtensions() {
            return extensions;
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
