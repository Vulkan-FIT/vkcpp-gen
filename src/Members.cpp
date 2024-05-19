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

#include "Members.hpp"
#include "Format.hpp"
#include "Output.hpp"

#include "Generator.hpp"

namespace vkgen
{

    void MemberResolver::reset() {
        resultVar.setSpecialType(VariableData::TYPE_INVALID);
        usedTemplates.clear();
    }

    std::string MemberResolver::getDbgtag(const std::string &prefix, bool bypass) {
        if (!gen.getConfig().dbg.methodTags && !bypass) {
            return "";
        }
        std::string out = "// ";
        out += prefix;
        out += " ";
        out += cmd->name.original;

        out += " <" + dbgtag + ">";
        //    if (ctx.raiiOnly) {
        //        out += " <RAII indirect>";
        //    } else
        if (isIndirect()) {
            out += " [indirect]";
        }
        if (cmd->createsHandle()) {
            out += " [handle]";
        }
        out += " [" + std::to_string((int)cmd->nameCat) + ":" + Registry::to_string(cmd->nameCat) + "]";
        out += " [" + std::to_string((int)cmd->pfnReturn) + ":" + Registry::to_string(cmd->pfnReturn) + "]";

        if (_indirect) {
            out += "  <INDIRECT>";
        }
        if (_indirect2) {
            out += "  <INDIRECT 2>";
        }
        if (cmd->createsHandle()) {
            out += "  <HANDLE>";
        }
        if (cmd->destroysObject()) {
            out += "  <DESTROY>";
        }

        out += "\n";
        out += dbgfield;
        return out;
    }

    std::string MemberResolver::generateDeclaration() {
        std::string output;
        if (ctx.commentOut) {
            output += "/*\n";
        }
        // std::string const indent = ctx.isStatic ? "  " : "    ";
        std::string const indent       = "    ";
        bool              usesTemplate = false;  // TODO
        output += getProto(indent, "(declaration)", true, usesTemplate) + ";\n";
        if (usesTemplate) {
            output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES\n";
        }
        output += "\n";
        if (ctx.commentOut) {
            output += "*/\n";
        }
        return output;
    }

    std::string MemberResolver::generateDefinition(bool genInline, bool bypass) {
        std::string output;
        if (ctx.commentOut) {
            output += "/*\n";
        }
        // std::string const indent = genInline ? "    " : "  ";
        std::string const indent = "    ";

        bool usesTemplate = false;
        output += getProto(indent, "(definition)", genInline, usesTemplate) + "\n    {\n";
        if (ctx.ns == Namespace::RAII && isIndirect() && !constructor) {
            if (cls->ownerhandle.empty()) {
                std::cerr << "Error: can't generate function: class has "
                             "no owner ("
                          << cls->name << ", " << name << ")" << '\n';
            } else {
                pfnSourceOverride = cls->ownerhandle + "->getDispatcher()->";
            }
        }
        for (const VariableData &p : cmd->params) {
            if (p.getIgnoreFlag()) {
                continue;
            }
            if (p.getSpecialType() == VariableData::TYPE_ARRAY_PROXY) {
                if (p.isLenAttribIndirect()) {
                    const auto       &var  = p.getLengthVar();
                    std::string const size = var->identifier() + "." + p.getLenAttribRhs();
                    output += "    // VULKAN_HPP_ASSERT (" + p.identifier() + ".size()" + " == " + size + ")\n";
                }
            }
        }

#ifdef INST
        std::cout << "Inst::bodyStart" << '\n';
        output += Inst::bodyStart(ctx.ns, name.original);
#endif

        output += generateMemberBody();

        if (generateReturnType() != "void" && !returnValue.empty()) {
            output += "      return " + returnValue + ";\n";
        }
        output += "    }\n";
        if (usesTemplate) {
            output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES\n";
        }
        output += "\n";
        if (ctx.commentOut) {
            output += "*/\n";
        }
        return output;
    }

    std::string MemberResolver::createArgumentWithType(const std::string &type) const {
        for (const VariableData &p : cmd->params) {
            if (p.type() == type) {
                return p.identifier();
            }
        }
        // type is not in command parameters, look inside structs
        for (const VariableData &p : cmd->params) {
            auto it = gen.structs.find(p.original.type());
            if (it != gen.structs.end()) {
                for (const auto &m : it->members) {
                    if (m->type() == type) {
                        return p.identifier() + (p.isPointer() ? "->" : ".") + m->identifier();
                    }
                }
            }
        }
        // not found
        return "";
    }

    std::string MemberResolver::successCodesCondition(const std::string &id, const std::string &indent) const {
        std::string output;
        for (const auto &c : cmd->successCodes) {
            if (c == "VK_INCOMPLETE") {
                continue;
            }
            output += "( " + id + " == Result::" + gen.enumConvertCamel("Result", c) + " ) ||\n" + indent;
        }
        strStripSuffix(output, " ||\n" + indent);
        return output;
    }

    std::string MemberResolver::successCodesList(const std::string &indent) const {
        std::string output;
        if (cmd->successCodes.empty()) {
            return output;
        }
        output += ",\n" + indent + "{ ";
        std::string const suffix = ",\n" + indent + "  ";
        for (const auto &c : cmd->successCodes) {
            if (c == "VK_INCOMPLETE") {
                continue;
            }
            if (gen.getConfig().gen.internalVkResult) {
                output += c + suffix;
            } else {
                output += "Result::" + gen.enumConvertCamel("Result", c) + suffix;
            }
        }
        strStripSuffix(output, suffix);
        output += "}";
        return output;
    }

    std::string MemberResolver::createArgument(std::function<bool(const VariableData &, bool)>  filter,
                                               std::function<std::string(const VariableData &)> function,
                                               bool                                             proto,
                                               bool                                             pfn,
                                               const VariableData                              &var) const {
        bool const sameType = var.original.type() == cls->name.original && !var.original.type().empty();

        if (!filter(var, sameType)) {
            return "";
        }

        if (!proto) {
            if (pfn) {
                std::string alt = var.getAltPNF();
                if (!alt.empty()) {
                    return alt;
                }
            }

            // TODO deprecated
            auto v = varSubstitution.find(var.original.identifier());
            if (v != varSubstitution.end()) {
                return "/* $V */" + v->second;
            }

            if (var.getIgnoreProto() && !var.isLocalVar()) {
                if (var.original.type() == "VkAllocationCallbacks" && !gen.cfg.gen.allocatorParam) {
                    // return pfn ? "nullptr/*ALLOC*/" : "/*- ALLOC*/";
                    if (ctx.ns == Namespace::VK && !ctx.disableAllocatorRemoval) {
                        return "";
                    }
                    return pfn ? "nullptr" : "";
                }

                VariableData::Flags flag = VariableData::Flags::CLASS_VAR_VK;
                if (ctx.ns == Namespace::RAII) {
                    flag = VariableData::Flags::CLASS_VAR_RAII;
                } else if (ctx.inUnique) {
                    flag = VariableData::Flags::CLASS_VAR_UNIQUE;
                }
                for (const VariableData &v : cls->vars) {
                    if (!hasFlag(v.getFlags(), flag)) {
                        continue;
                    }
                    static const auto cmp = [&](const std::string_view lhs, const std::string_view rhs) { return !lhs.empty() && lhs == rhs; };
                    if (cmp(var.type(), v.type()) || cmp(var.original.type(), v.original.type())) {
                        if (sameType && ctx.useThis) {
                            break;
                        }
                        return v.toVariable(gen, var, pfn);
                    }
                }
                if (sameType) {
                    // std::cerr << "> to this: " << var.type() << '\n';
                    std::string s;
                    if (ctx.ns == Namespace::RAII && (var.getNamespace() != Namespace::RAII || pfn)) {
                        s = "*";
                    }
                    s += (var.isPointer() ? "this" : "*this");
                    if (pfn) {
                        s = "static_cast<" + cls->name.original + ">(" + s + ")";
                    }
                    return s;  //"/*" + var.original.type() + "*/";
                }
            }
        }

        return function(var);
    }

    std::string MemberResolver::createArgument(const VariableData &var, bool sameType, bool pfn) const {
        auto v = varSubstitution.find(var.original.identifier());
        if (v != varSubstitution.end()) {
            return "/* $V */" + v->second;
        }

        if (var.getIgnoreProto()) {
            VariableData::Flags flag = VariableData::Flags::CLASS_VAR_VK;
            if (ctx.ns == Namespace::RAII) {
                flag = VariableData::Flags::CLASS_VAR_RAII;
            } else if (ctx.inUnique) {
                flag = VariableData::Flags::CLASS_VAR_UNIQUE;
            }
            for (const VariableData &v : cls->vars) {
                if (!hasFlag(v.getFlags(), flag)) {
                    continue;
                }
                static const auto cmp = [&](const std::string_view lhs, const std::string_view rhs) { return !lhs.empty() && lhs == rhs; };
                if (cmp(var.type(), v.type()) || cmp(var.original.type(), v.original.type())) {
                    if (sameType && ctx.useThis) {
                        break;
                    }
                    return v.toVariable(gen, var, pfn);
                }
            }
            if (sameType) {
                // std::cerr << "> to this: " << var.type() << '\n';
                std::string s;
                if (ctx.ns == Namespace::RAII && (var.getNamespace() != Namespace::RAII || pfn)) {
                    s = "*";
                }
                s += (var.isPointer() ? "this" : "*this");
                if (pfn) {
                    s = "static_cast<" + cls->name.original + ">(" + s + ")";
                }
                return s;  //"/*" + var.original.type() + "*/";
            }
        }
        return "";
    }

    std::string MemberResolver::createArguments(std::function<bool(const VariableData &, bool)>  filter,
                                                std::function<std::string(const VariableData &)> function,
                                                bool                                             proto,
                                                bool                                             pfn) const {
        static constexpr auto sep = "\n        ";
        std::string           out;
        const bool            dbg = gen.getConfig().dbg.methodTags;
        // bool last = true;
        for (const VariableData &p : cmd->params) {
            const std::string &arg = createArgument(filter, function, proto, pfn, p);
            if (!arg.empty() || dbg) {
                out += sep;
            }
            if (!arg.empty()) {
                out += arg + ", ";
            }
            if (dbg) {
                out += p.argdbg();
            }
        }

        auto it = out.find_last_of(',');
        if (it != std::string::npos) {
            if (dbg) {
                out[it] = ' ';
            } else {
                out.erase(it, 1);
            }
        }
        if (!out.empty()) {
            out += "\n      ";
        }
        return out;
    }

    std::string MemberResolver::declareReturnVar(const std::string &assignment) {
        if (!resultVar.isInvalid()) {
            return "";
        }
        resultVar.setSpecialType(VariableData::TYPE_DEFAULT);
        resultVar.setIdentifier("result");
        if (gen.getConfig().gen.internalVkResult) {
            resultVar.setFullType("", "VkResult", "");
        } else {
            resultVar.setFullType("", "Result", "");
        }

        std::string out = resultVar.toString(gen);
        if (!assignment.empty()) {
            out += " = " + assignment;
        }
        return out += ";\n";
    }

    std::string MemberResolver::generateMemberBody() {
        return "";
    };

    std::string MemberResolver::castTo(const std::string &type, const std::string &src) const {
        if (type != cmd->type) {
            return "static_cast<" + type + ">(" + src + ")";
        }
        return src;
    }

    bool MemberResolver::useDispatchLoader() const {
        return ctx.ns == Namespace::VK && gen.useDispatchLoader();
    }

    bool MemberResolver::isTemplated() const {
        if (!ctx.disableDispatch) {
            return true;
        }
        for (const VariableData &p : cmd->params) {
            if (!p.getTemplate().empty()) {
                return true;
            }
        }
        return false;
    }

    std::string MemberResolver::getDispatchSource() const {
        std::string output = pfnSourceOverride;
        if (output.empty()) {
            if (ctx.ns == Namespace::RAII) {
                if (cls->name == "Instance" && gen.getConfig().gen.raii.staticInstancePFN) {
                    output += gen.m_ns_raii + "::Instance::m_dispatcher.";
                } else if (cls->name == "Device" && gen.getConfig().gen.raii.staticDevicePFN) {
                    output += gen.m_ns_raii + "::Device::m_dispatcher.";
                } else {
                    if (!cls->ownerhandle.empty()) {
                        output += cls->ownerhandle + "->getDispatcher()->";
                    } else {
                        output += "m_dispatcher->";
                    }
                }
            } else if (ctx.exp) {
                if (cls && !cls->name.empty() && !cls->isSubclass) {
                    output += "m_dispatcher.";
                } else {
                    if (!cmd->top) {
                        std::cerr << "can't get dispatch source" << std::endl;
                    } else {
                        output += strFirstLower(cmd->top->name) + ".getDispatcher()->";
                    }
                }
            } else {
                output += gen.getDispatchCall();
            }
        }
        return output;
    }

    std::string MemberResolver::getDispatchPFN() const {
        std::string output = getDispatchSource();
        if (pfnNameOverride.empty()) {
            output += cmd->name.original;
        } else {
            output += pfnNameOverride;
        }
        return output;
    }

    std::string MemberResolver::generatePFNcall(bool immediateReturn) {
        std::string call = getDispatchPFN();
        call += "(" + createPFNArguments() + ")";

        using enum Command::PFNReturnCategory;
        switch (cmd->pfnReturn) {
            case VK_RESULT:
                call = castTo(gen.getConfig().gen.internalVkResult ? "VkResult" : "Result", call);
                if (!immediateReturn) {
                    return assignToResult(call);
                }
                break;
            case OTHER: call = castTo(returnType, call); break;
            case VOID: return call + ";";
            default: break;
        }
        if (immediateReturn) {
            call = castTo(returnType, call);
            call = "return " + call;
        }
        return call + ";";
    }

    std::string MemberResolver::assignToResult(const std::string &assignment) {
        if (resultVar.isInvalid()) {
            return declareReturnVar(assignment);
        } else {
            return resultVar.identifier() + " = " + assignment + ";";
        }
    }

    std::string MemberResolver::generateReturnValue(const std::string &identifier) {
        if (resultVar.isInvalid()) {
            return identifier;
        }
        if (ctx.ns == Namespace::RAII) {
            if (usesResultValue()) {
                std::string result = resultVar.identifier();
                if (gen.getConfig().gen.internalVkResult) {
                    result = "static_cast<Result>(" + result + ")";
                }
                return "std::make_pair( " + result + ", " + identifier + " )";
            }
            return identifier;
        }

        std::string out;
        if (usesResultValue()) {
            out += "ResultValue<" + returnType + ">";
        } else if (usesResultValueType()) {
            out += "createResultValueType";
        } else {
            return identifier;
        }
        std::string args;
        if (resultVar.identifier() != identifier) {
            if (gen.getConfig().gen.internalVkResult) {
                args += "static_cast<Result>(" + resultVar.identifier() + ")";
            } else {
                args += resultVar.identifier();
            }
        }
        if (!identifier.empty()) {
            if (!args.empty()) {
                args += ", ";
            }
            args += identifier;
        }

        out += "(" + args + ")";
        return out;
    }

    std::string MemberResolver::createCheckMessageString() const {
        std::string message;

        const auto &macros = gen.getConfig().macro;
        const auto &ns     = (ctx.ns == Namespace::RAII && !constructorInterop) ? macros.mNamespaceRAII.data : macros.mNamespace.data;
        if (ns.usesDefine) {
            message = ns.define + "_STRING \"";
        } else {
            message = "\"" + ns.value;
        }
        if (!clsname.empty()) {
            message += "::" + clsname;
        }
        message += "::" + name + "\"";
        return message;
    }

    std::string MemberResolver::generateCheck() {
        if (cmd->pfnReturn != Command::PFNReturnCategory::VK_RESULT || resultVar.isInvalid()) {
            return "";
        }

        std::string message = createCheckMessageString();
        std::string codes;
        if (returnSuccessCodes() > 1) {
            codes = successCodesList("                ");
        }

        std::string output = vkgen::format(R"(
      resultCheck({0},
                {1}{2});
)",
                                           resultVar.identifier(),
                                           message,
                                           codes);

        return output;
    }

    bool MemberResolver::usesResultValue() const {
        if (returnSuccessCodes() <= 1) {
            return false;
        }
        // if (cmd->outParams.size() > 1) {
        for (const VariableData &d : cmd->outParams) {
            if (d.isArray()) {
                return false;
            }
        }
        // }
        return !returnType.empty() && returnType != "Result" && cmd->pfnReturn == Command::PFNReturnCategory::VK_RESULT;
    }

    bool MemberResolver::usesResultValueType() const {
        const auto &cfg = gen.getConfig();
        if (!cfg.gen.resultValueType) {
            return false;
        }
        if (ctx.exp) {
            return false;
        }
        return !returnType.empty() && returnType != "Result" && cmd->pfnReturn == Command::PFNReturnCategory::VK_RESULT;
    }

    std::string MemberResolver::generateReturnType() const {
        if (ctx.ns == Namespace::VK) {
            if (usesResultValue()) {
                return "ResultValue<" + returnType + ">";
            }
            if (usesResultValueType()) {
                return "typename ResultValueType<" + returnType + ">::type";
            }
        } else {
            if (usesResultValue()) {
                return vkgen::format("std::pair<{0}::Result, {1}>", gen.m_ns, returnType);
            }
        }
        return returnType;
    }

    std::string MemberResolver::createReturnType() const {
        if (constructor) {
            return "";
        }

        std::string type;
        std::string str;
        for (const VariableData &p : cmd->outParams) {
            str += p.getReturnType(gen) + ", ";
        }
        strStripSuffix(str, ", ");

        auto count = cmd->outParams.size();
        // cmd->pfnReturn == PFNReturnCategory::OTHER

        if (count == 0) {
            if (cmd->pfnReturn == Command::PFNReturnCategory::OTHER) {
                type = strStripVk(std::string(cmd->type));
            } else {
                type = "void";
            }
        } else if (count == 1) {
            type = str;
        } else if (count >= 2) {
            type = "std::pair<" + str + ">";
        }

        if (returnSuccessCodes() > 1) {
            if (type.empty() || type == "void") {
                type = "Result";
            }
        }

        return type;
    }

    std::string MemberResolver::generateNodiscard() {
        const auto &cfg = gen.getConfig();
        if (!returnType.empty() && returnType != "void") {
            return "VULKAN_HPP_NODISCARD ";
        }
        return "";
    }

    std::string MemberResolver::getSpecifiers(bool decl) {
        std::string output;
        const auto &cfg = gen.getConfig();
        if (specifierInline && !decl) {
            output += cfg.macro.mInline.get() + " ";
        }
        if (specifierExplicit && decl) {
            output += cfg.macro.mExplicit.get() + " ";
        }
        if (specifierConstexpr) {
            output += cfg.macro.mConstexpr.get() + " ";
        } else if (specifierConstexpr14) {
            output += cfg.macro.mConstexpr14.get() + " ";
        }
        return output;
    }

    std::string MemberResolver::getProto(const std::string &indent, const std::string &prefix, bool declaration, bool &usesTemplate) {
        std::string dbg = getDbgtag(prefix);
        std::string output;
        if (!dbg.empty()) {
            output += indent + dbg;
        }

        std::string temp;
        std::string allocatorTemplate;
        for (VariableData &p : cmd->params) {
            const std::string &str = p.getTemplate();
            if (!str.empty()) {
                temp += str;
                if (declaration) {
                    temp += p.getTemplateAssignment();
                }
                temp += ", ";
            }
        }
        if (!allocatorTemplate.empty()) {
            temp += "typename B0 = " + allocatorTemplate + ",\n";
            strStripSuffix(allocatorTemplate, "Allocator");
            temp += "typename std::enable_if<std::is_same<typename B0::value_type, " + allocatorTemplate + ">::value, int>::type = 0";
        }

        strStripSuffix(temp, ", ");
        if (!temp.empty()) {
            output += indent + "template <" + temp + ">\n";
        }

        std::string spec = getSpecifiers(declaration);
        std::string ret  = generateReturnType();
        output += indent;
        if (!declaration) {
            output += generateNodiscard();
        }
        if (!spec.empty()) {
            output += spec;
        }
        if (!ret.empty()) {
            output += ret + " ";
        }
        if (!declaration && !ctx.isStatic) {
            //                if (constructorInterop) {
            //                    output += gen.getConfig().macro.mNamespace->get() + "::";
            //                }
            output += clsname + "::";
        }

        output += name + "(" + createProtoArguments(declaration) + ")";
        if (!declaration && !initializer.empty()) {
            output += initializer;
        }
        if (specifierConst && !ctx.isStatic && !constructor) {
            output += " const";
        }
        if (ctx.ns == Namespace::RAII && !temp.empty()) {
            usesTemplate = true;
            return "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES\n" + output;
        }
        usesTemplate = false;

        return output;
    }

    std::string MemberResolver::createProtoArguments(bool declaration) {
        return createProtoArguments(false, declaration);
    }

    void MemberResolver::transformToArray(VariableData &var) {
        auto sizeVar = var.getLengthVar();
        if (!sizeVar) {
            return;
        }

        if (var.isLenAttribIndirect()) {
            sizeVar->setIgnoreFlag(true);
        } else {
            sizeVar->setIgnoreProto(true);
            sizeVar->setIgnorePass(true);
        }
        if (var.isArrayIn()) {
            sizeVar->setIgnoreFlag(true);
        }

        if (var.original.type() == "void" && !var.original.isConstSuffix()) {
            bool isPointer = sizeVar->original.isPointer();
            if (isPointer) {
                var.setFullType("", "uint8_t", "");
            } else {
                std::string templ = "DataType";
                if (isInContainter(usedTemplates, templ)) {
                    std::cerr << "Warning: "
                              << "same templates used in " << name << '\n';
                }
                usedTemplates.push_back(templ);
                var.setFullType("", templ, "");
                var.setTemplate("typename " + templ);
                var.setTemplateDataType(templ);
                sizeVar->setIgnoreFlag(false);
                sizeVar->setIgnoreProto(false);
            }
        }

        if (var.isArrayOut()) {
            var.convertToStdVector(gen);
        } else {
            var.convertToArrayProxy();
        }
        if (var.isReference()) {
            std::cerr << "array is reference enabled" << '\n';
        }
    };

    void MemberResolver::prepareParams() {
        if (constructor) {
            return;
        }

        for (VariableData &p : cmd->params) {
            const std::string &type = p.original.type();

            if (!p.isArray()) {
                if (ctx.ns == Namespace::RAII && p.original.type() == cls->superclass.original) {
                    p.setIgnoreProto(true);
                    // p.appendDbg("/*PP()*/");
                } else if (p.original.type() == cls->name.original) {
                    p.setIgnoreProto(true);
                    // p.appendDbg("/*PP()*/");
                }
            }
        }
    }

    bool MemberResolver::checkMethod() const {
        bool blacklisted = true;
        gen.genOptional(*cmd, [&](std::string &output) { blacklisted = false; });
        if (blacklisted) {
            //                std::cout << "checkMethod: " << name.original
            //                              << " blacklisted\n";
            return false;
        }

        return true;
    }

    MemberResolver::MemberResolver(const Generator &gen, const ClassCommand &d, MemberContext &c, bool constructor)
      : gen(gen), cmd(d.src), name(d.name), cls(d.cls), ctx(c), resultVar(VariableData::TYPE_INVALID), constructor(constructor) {
        _indirect  = d.src->isIndirect();
        _indirect2 = d.raiiOnly;
        // std::cout << "MemberResolver" << '\n';
        if (!cmd) {
            std::cerr << "null cmd" << '\n';
            return;
        }
        if (!cls) {
            std::cerr << "null cls" << '\n';
            return;
        }

        if (!cmd->prepared) {
            dbgfield += "    // params restore()\n";
        }
        // std::cerr << "MemberResolver() prepare  " << cmd->name.original << '\n';
        cmd->prepare();
        //            size_t i = 0;
        //            for (auto &p : cmd->params) {
        //                std::stringstream s;
        //                s << std::hex << &p.get();
        //                dbgfield += "    // p[" + std::to_string(i) + "]: <" + s.str() + ">\n";
        //                i++;
        //            }
        //            i = 0;
        //            for (auto &p : cmd->outParams) {
        //                std::stringstream s;
        //                s << std::hex << &p.get();
        //                dbgfield += "    // o[" + std::to_string(i) + "]: <" + s.str() + ">\n";
        //                i++;
        //            }

        clsname = cls->name;

        returnType           = strStripVk(std::string(cmd->type));  // TODO unnecessary?
        dbgtag               = "default";
        specifierInline      = true;
        specifierExplicit    = false;
        specifierConst       = true;
        specifierConstexpr   = false;
        specifierConstexpr14 = false;

        hasDependencies = checkMethod();

        auto rbegin = cmd->params.rbegin();
        // std::cout << "rbegin" << '\n';
        if (rbegin != cmd->params.rend()) {
            // std::cout << "rbegin get" << '\n';
            last = &rbegin->get();
        } else {
            std::cerr << "MemberResolver(): No last variable" << '\n';
            //                v_placeholder = std::make_unique<VariableData>(VariableData::TYPE_PLACEHOLDER);
            //                last = v_placeholder.get();
        }

        // std::cout << "prepare params" << '\n';
        prepareParams();
        // std::cout << "prepare params done" << '\n';

        const auto &cfg      = gen.getConfig();
        bool        dispatch = cfg.gen.dispatchParam && ctx.ns == Namespace::VK && !ctx.disableDispatch;

        for (VariableData &p : cmd->params) {
            if (p.original.type() == "VkAllocationCallbacks") {
                allocatorVar = &p;
                // std::cout << "[[ a: " << gen.getConfig().gen.allocatorParam << std::endl;
                if (!cfg.gen.allocatorParam) {
                    allocatorVar->setIgnoreProto(true);
                }
            }
        }

        if (ctx.insertClassVar) {
            VariableData &var = addVar(cmd->params.begin(), cls->name);
            var.toRAII();
            var.setIgnorePFN(true);
            var.setIgnoreFlag(true);
            var.setIgnoreProto(true);
            var.setDbgTag("(C)");
        }

        if (ctx.insertSuperclassVar) {
            if (ctx.exp) {
                VariableData &first = *cmd->params.begin();
                if (cmd->top) {
                    if (first.type() != cmd->top->name) {
                        VariableData &var = addVar(cmd->params.begin(), cmd->top->name);
                        var.setDbgTag("(S)");
                        var.setIgnorePFN(true);
                    } else {
                        first.appendDbg("<NS>");
                    }
                }
                {
                    VariableData &first = *cmd->params.begin();
                    first.setNamespace(Namespace::VK);
                    first.convertToReference();
                    first.setConst(true);
                }
            } else {
                VariableData &var = addVar(cmd->params.begin(), cls->superclass);
                var.toRAII();
                var.setIgnorePFN(true);
                var.setDbgTag("(S)");
            }
        }

        if (dispatch) {
            std::string type = gen.getDispatchType();
            // std::cout << "add dispatch var" << '\n';
            VariableData &var = addVar(cmd->params.end());
            var.setSpecialType(VariableData::TYPE_DISPATCH);
            var.setFullType("", type, " const &");
            var.setIdentifier("d");
            var.setIgnorePFN(true);
            var.setOptional(true);
            var.setDbgTag("(D)");
            if (cfg.gen.dispatchTemplate) {
                var.setTemplate("typename " + type);
                var.setTemplateAssignment(" = VULKAN_HPP_DEFAULT_DISPATCHER_TYPE");
            }
            std::string assignment = cfg.macro.mDispatch.get();
            if (!assignment.empty()) {
                var.setAssignment(" " + assignment);
            }
        }

        cmd->prepared = false;
        // std::cout << "MemberResolver done" << '\n';
    }

    MemberResolver::~MemberResolver() {}

//    void MemberResolver::generateX(std::string &def) {
//        setOptionalAssignments();
//        def += generateDefinition(true);
//    }
//
//    void MemberResolver::generateX(std::string &decl, std::string &def) {
//        setOptionalAssignments();
//        decl += generateDeclaration();
//        def += generateDefinition(false);
//    }

    void MemberResolver::generate(UnorderedFunctionOutputX &decl, UnorderedFunctionOutputGroup &def, const std::span<Protect> opt) {
        setOptionalAssignments();

        if (gen.getConfig().dbg.methodTags) {
            for (auto &p : cmd->_params) {
                dbgfield += p->dbgstr();
            }
        }

        std::vector<Protect> protects;
        if (enhanced) {
            protects.emplace_back("VULKAN_HPP_DISABLE_ENHANCED_MODE", false);
        }
        if (!structChainType.empty()) {
            protects.emplace_back("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN", false);
        }
        if (!ctx.exp && ctx.isStatic) {
            protects.emplace_back("VK_NO_PROTOTYPES", false);
        }
        for (const auto &p : opt) {
            protects.emplace_back(p);
        }

        const auto &p = cmd->getProtect();
        if (!guard.empty()) {
            protects.emplace_back(guard, true);
        }
        if (!p.empty()) {
            protects.emplace_back(p, true);
        }
        if (ctx.generateInline) {
            decl.get(protects) += generateDefinition(true); // TODO move
        }
        else {
            decl.get(protects) += generateDeclaration(); // TODO move
            std::string str;
//            str = " // ";
//            for (const auto &p : protects) {
//                str += p.first;
//                str += " ->";
//            }
//            str += '\n';
            str += generateDefinition(false);
            if (!p.empty()) {
                def.platform.get(protects) += str;
            }
            else if (isTemplated()) {
                def.templ.get(protects) += str;
            }
            else {
                def.def.get(protects) += str;
            }
        }

        reset();
    }

    void MemberResolver::disableFirstOptional() {
        for (Var &p : cmd->params) {
            if (p.isOptional()) {
                p.setOptional(false);
                break;
            }
        }
    }

    int MemberResolver::returnSuccessCodes() const {  // TODO rename
        int count = 0;
        for (const auto &c : cmd->successCodes) {
            if (c == "VK_INCOMPLETE") {
                continue;
            }
            count++;
        }
        return count;
    }

    bool MemberResolver::isIndirect() const {
        return cls->isSubclass;
    }

    Signature MemberResolver::createSignature() const {
        Signature sig;
        sig.name = name;
        std::string sep;
        for (const VariableData &p : cmd->params) {
            bool sameType = p.original.type() == cls->name.original && !p.original.type().empty();

            if (!filterProto(p, sameType)) {
                continue;
            }

            sig.args += p.prefix() + p.type() + p.suffix() + sep;
            sep = ", ";
        }
        return sig;
    }

    std::string MemberResolver::createProtoArguments(bool useOriginal, bool declaration) const {
        std::string output;
        if (gen.getConfig().dbg.methodTags) {
            output = "// {Pargs}";
        }
        output += createArguments(
          filterProto,
          [&](const VariableData &v) {
              if (useOriginal) {
                  return v.originalToString();
              }
              if (declaration) {
                  return v.toStringWithAssignment(gen);
              }
              return v.toString(gen);
          },
          true,
          false);
        return output;
    }

    std::string MemberResolver::createPFNArguments(bool useOriginal) const {
        std::string output;
        if (gen.getConfig().dbg.methodTags) {
            output = "// {PFNargs}";
        }
        output += createArguments(
          filterPFN, [&](const VariableData &v) { return v.toArgument(gen, useOriginal); }, false, true);
        return output;
    }

    std::string MemberResolver::createPassArgumentsRAII() const {
        std::string output;
        if (gen.getConfig().dbg.methodTags) {
            output = "// {RAIIargs}";
        }

        //            if (cls->superclass.empty()) {
        //                output += "*this, ";
        //            }
        output += createArguments(
          filterRAII, [&](const VariableData &v) { return v.identifier(); }, false, false);
        return output;
    }

    std::string MemberResolver::createPassArguments(bool hasAllocVar) const {
        std::string output;
        if (gen.getConfig().dbg.methodTags) {
            output = "// {PASSargs}";
        }
        output += createArguments(
          filterPass, [&](const VariableData &v) { return v.identifier(); }, false, false);
        return output;
    }

    std::string MemberResolver::createStaticPassArguments(bool hasAllocVar) const {
        std::string output;
        if (gen.getConfig().dbg.methodTags) {
            output = "// {PASSargsStatic}";
        }
        output += createArguments(
          filterPass, [&](const VariableData &v) { return v.identifier(); }, false, true);
        return output;
    }

    std::vector<std::reference_wrapper<VariableData>> MemberResolver::getFilteredProtoVars() const {
        std::vector<std::reference_wrapper<VariableData>> out;
        for (Var &p : cmd->params) {
            if (filterProto(p, p.original.type() == cls->name.original)) {
                out.push_back(std::ref(p));
            }
        }
        return out;
    }

    std::string MemberResolver::createArgument(const VariableData &var, bool useOrignal) const {
        bool sameType = var.original.type() == cls->name.original && !var.original.type().empty();
        return createArgument(var, sameType, useOrignal);
    }

    bool MemberResolver::returnsTemplate() {
        return !last->getTemplate().empty();
    }

    void MemberResolver::setGenInline(bool value) {
        ctx.generateInline = value;
    }

    void MemberResolver::addStdAllocators() {
        auto rev = cmd->params.rbegin();
        if (rev != cmd->params.rend() && rev->get().getSpecialType() == VariableData::TYPE_DISPATCH) {
            rev++;
        }

        if (rev == cmd->params.rend()) {
            std::cerr << "can't add std allocators (no params) " << name.original << '\n';
            return;
        }

        auto it = rev.base();

        for (VariableData &v : cmd->outParams) {
            if (v.isArrayOut()) {
                const std::string &type = strFirstUpper(v.type()) + "Allocator";
                VariableData      &var  = addVar(it);
                var.setSpecialType(VariableData::TYPE_STD_ALLOCATOR);
                var.setFullType("", type, " &");
                var.setIdentifier(strFirstLower(type));
                var.setIgnorePFN(true);
                var.setIgnorePass(true);
                var.setTemplate("typename " + type);
                var.setTemplateAssignment(" = std::allocator<" + last->type() + ">");
                var.setDbgTag("(A)");

                v.setStdAllocator(var.identifier());
            }
        }
    }

    void MemberResolver::setOptionalAssignments() {
        bool assignment = true;
        for (auto it = cmd->params.rbegin(); it != cmd->params.rend(); it++) {
            auto &v = it->get();
            if (v.getIgnoreProto()) {
                continue;
            }
            if (!assignment) {
                v.setAssignment("");
            }
            else {
                if (v.getSpecialType() == VariableData::TYPE_DISPATCH) {
                    continue;
                }
                if (v.type() == "AllocationCallbacks") {
                    if (!v.isPointer()) {
                        v.setAssignment(" VULKAN_HPP_DEFAULT_ALLOCATOR_ASSIGNMENT");
                    }
                    else {
                        v.setAssignment("");
                    }
                }
                else if (v.isOptional()) {
                    if (v.isPointer()) {
                        v.setAssignment(" VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT");
                    } else {
                        if (v.original.isPointer()) {
                            v.setAssignment(" VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT");
                        } else {
                            v.setAssignment(" VULKAN_HPP_DEFAULT_ARGUMENT_ASSIGNMENT");
                            // if (v->isReference() && !v->isConst())
                            // integrity check?
                        }
                    }
                }
                if (v.getAssignment().empty()) {
                    assignment = false;
                }
            }
        }
    }

    bool MemberResolver::compareSignature(const MemberResolver &o) const {
        const auto removeWhitespace = [](std::string str) { return std::regex_replace(str, std::regex("\\s+"), ""); };

        const auto getType = [&](const VariableData &var) {
            std::string type = removeWhitespace(var.type());
            std::string suf  = removeWhitespace(var.suffix());
            return type + " " + suf;
        };

        auto lhs = getFilteredProtoVars();
        auto rhs = o.getFilteredProtoVars();
        if (lhs.size() == rhs.size()) {
            std::string str;
            for (size_t i = 0; i < lhs.size(); i++) {
                std::string l = getType(lhs[i]);
                std::string r = getType(rhs[i]);
                if (l != r) {
                    return false;
                }
                str += l + ", ";
            }
            std::cout << "sig match: " << str << '\n';
            return true;
        }
        return false;
    }

    void MemberResolverDefault::transformMemberArguments() {
        auto &params = cmd->params;

        const auto convertName = [](VariableData &var) {
            const std::string &id = var.identifier();
            if (id.size() >= 2 && id[0] == 'p' && std::isupper(id[1])) {
                var.setIdentifier(strFirstLower(id.substr(1)));
            }
        };

        for (VariableData &p : params) {
            if ((/*ctx.exp || */ ctx.returnSingle) && p.isArrayOut()) {
                continue;
            }
            transformToArray(p);
        }

        for (VariableData &p : params) {
            const std::string &type = p.original.type();

            if (p.isArray()) {
                convertName(p);
                continue;
            }
            if (p.isStructOrUnion()) {
                convertName(p);
                if (!p.isOutParam()) {
                    p.convertToReference();
                    // dbgfield += "// conv: " + p.identifier() + "\n";
                    p.setConst(true);
                }
            }

            if (p.isOptional() && !p.isOutParam()) {
                if (!p.isPointer() && p.original.isPointer()) {
                    p.convertToOptionalWrapper();
                }
            }
        }

        if (ctx.structureChain) {
            for (VariableData &p : params) {
                if (p.isOutParam() && p.isStruct()) {
                    structChainType = p.type();
                    structChainIdentifier = p.identifier();
                    std::string type = "StructureChain";
                    std::string templ;
                    if (p.isArrayOut()) {
                        p.setNamespace(Namespace::NONE);
                        templ = "typename StructureChain";
                    }
                    else {
                        type += "<X, Y, Z...>";
                        templ = "typename X, typename Y, typename... Z";
                    }
                    const auto &t = p.getTemplate();
                    if (!t.empty()) {
                        templ += ", " + t;
                    }
                    p.setTemplate(templ);
                    p.setType(type);
                    p.setIdentifier("structureChain");
                    break;
                }
            }
        }

        if (ctx.ns == Namespace::RAII && cmd->createsHandle()) {
            const auto &var = cmd->getLastHandleVar();
            if (var) {
                if (name.original != "vkGetSwapchainImagesKHR") {
                    var->toRAII();
                    var->setDbgTag(var->getDbgTag() + "(toR1)");
                }

                auto &h         = gen.findHandle(var->original.type());
                bool  converted = false;
                if (h.ownerRaii) {
                    for (VariableData &p : params) {
                        if (p.original.type() == h.ownerRaii->original.type()) {
                            p.toRAII();
                            p.setDbgTag(p.getDbgTag() + "(toR2)");
                            converted = true;
                            break;
                        }
                    }
                }
                if (!converted) {
                    if (h.parent) {
                        for (VariableData &p : params) {
                            if (p.original.type() == h.parent->name.original) {
                                p.toRAII();
                                p.setDbgTag(p.getDbgTag() + "(toR3)");
                                break;
                            }
                        }
                    }
                }
            } else {
                std::cerr << "warning: " << name.original << " creates handle but var was not found" << '\n';
            }
        }

        bool toTemplate = false;
        for (VariableData &p : cmd->outParams) {
            p.removeLastAsterisk();
            // std::cerr << "transformMemberArguments set" << '\n';
            p.setIgnoreProto(true);
            p.setIgnorePass(true);
            p.setConst(false);
            auto &vars = p.getArrayVars();
            if (vars.empty()) {
                if (p.fullType(gen) == "void") {
                    if (toTemplate) {
                        std::cerr << "Warning: multile void returns" << '\n';
                    }

                    std::string templ = "DataType";
                    if (isInContainter(usedTemplates, templ)) {
                        std::cerr << "Warning: "
                                  << "same templates used in " << name << '\n';
                    }
                    usedTemplates.push_back(templ);
                    p.setType(templ);
                    p.setTemplate("typename " + templ);
                    p.setTemplateDataType(templ);
                    toTemplate = true;
                }
            }
        }

        if (ctx.addVectorAllocator) {
            for (VariableData &p : cmd->outParams) {
                if (p.isArrayOut()) {
                    const auto &type = p.getAllocatorType();
                    auto id = strFirstLower(type);
                    p.setStdAllocator(id);

                    VariableData &var = addVar(cmd->params.end(), type, id);
                    var.setReference(true);
                }
            }
        }


        if (ctx.templateVector && !ctx.returnSingle) {
            int count = 0;
            for (VariableData &p : cmd->outParams) {
                if (p.isArrayOut()) {
                    if (!p.getTemplateDataType().empty()) {
                        continue;
                    }
                    std::string type = "Vec";
                    if (count > 0) {
                        type += std::to_string(count);
                    }
                    p.setTemplateAssignment(" = std::vector<" + p.namespaceString(gen) + p.type() + ">");
                    p.setTemplate("typename " + type);
                    p.setFullType("", type, "");
                    p.setNamespace(Namespace::NONE);
                    p.setSpecialType(VariableData::TYPE_TEMPL_VECTOR);
                    count++;
                }
            }
        }

        returnType = createReturnType();
    }

    std::string MemberResolverDefault::getSuperclassArgument(const String &superclass) const {
        std::string output;
        for (const VariableData &p : cmd->params) {
            if (!p.getIgnoreProto() && p.original.type() == superclass.original) {
                output = p.identifier();
            }
        }
        if (output.empty()) {
            if (superclass == cls->superclass) {
                output = "*m_" + strFirstLower(superclass);
            } else if (superclass == cls->name) {
                output = "*this";
            } else {
                std::cerr << "warning: can't create superclass argument" << '\n';
            }
        }

        if (gen.getConfig().dbg.methodTags) {
            return "/*{SC " + superclass + ", " + cls->superclass + " }*/" + output;
        }
        return output;
    }

    VariableData * MemberResolverDefault::generateVariables(std::string &output,  std::string &returnId, bool &returnsRAII, bool dbg) {
        const auto &cfg = gen.getConfig();
        bool          hasPoolArg     = false;
        VariableData *vectorSizeVar = {};
        // VariableData *inputSizeVar  = {};

        if (!cmd->outParams.empty()) {
            std::set<VariableData *> countVars;
            for (const VariableData &v : cmd->outParams) {
                // output += "/*Var: " + v.fullType(gen) + "  " + (v.isArrayOut()? "ARRAY" : "") + (v.isArrayIn()? "IN" : "") + " VS" + std::to_string(cmd->outParams.size()) + "*/\n";
                if (v.isArray()) {
                    auto *const var = v.getLengthVar();
                    if (!v.isLenAttribIndirect()) {

                        bool insert = true;
                        if (!var->getIgnoreProto()) {
                            // output += "/*    inputSizeVar: " + var->fullType(gen) + "*/\n";
//                            inputSizeVar = var;
                            auto t = v.getTemplateDataType();
                            if (!t.empty()) {
                                output += vkgen::format("      VULKAN_HPP_ASSERT( {0} % sizeof( {1} ) == 0 );\n", var->identifier(), t);
                            }
                        }
                        else {
                            // output += "/*    ignore: " + var->fullType(gen) + " " + var->identifier() +  "*/\n";
                            for (auto &v : var->getArrayVars()) {
                                if (v->specialType == VariableData::TYPE_ARRAY_PROXY || v->specialType == VariableData::TYPE_ARRAY_PROXY_NO_TEMPORARIES) {
                                    insert = false;
                                    break;
                                }
                                // output += "/*      av: " + v->fullType(gen) + " " + v->identifier() +  "*/\n";
                            }
                        }
                        if (insert && !countVars.contains(var)) {
                            countVars.insert(var);
                            var->removeLastAsterisk();
                            vectorSizeVar = var;
                        }
                    }
                    else {
                        // output += "/*    indirect: " + var->fullType(gen) + "*/\n";
                    }
                    if (v.getNamespace() == Namespace::RAII) {
                        returnsRAII = true;
                        if (dbg) {
                            output += "/*RTR*/\n";
                        }
                    }
                }
            }

            if (countVars.size() > 1) {
                std::cerr << "generate member: multiple count vars in " << name.original << '\n';
            }

//            for (const VariableData &v : cmd->params) {
//                if (v.isArrayIn()) {
//                    const auto &var = v.getLengthVar();
//                    if (countVars.contains(&(*var))) {
//                        inputSizeVar = &(*var);
//                        output += "/* inputSizeVar2: " + var->fullType(gen) + "*/\n";
//                        break;
//                    }
//                }
//            }

            if (cmd->outParams.size() > 1) {
                output += "      " + returnType + " data_";
                if (returnsRAII) {
                    std::cerr << "warning: unhandled return RAII type " << name << '\n';
                }
                returnId = "data_";

                std::vector<std::string> init;
                bool                     hasInitializer = false;
                for (const VariableData &v : cmd->outParams) {
                    auto &i = init.emplace_back(std::move(v.getLocalInit()));
                    if (!i.empty()) {
                        hasInitializer = true;
                    }
                }
                if (hasInitializer) {
                    if (init.size() > 1) {
                        output += "( std::piecewise_construct";
                        for (const auto &i : init) {
                            output += ", std::forward_as_tuple( ";
                            output += i.empty() ? "0" : i;
                            output += " )";
                        }
                        output += " )";
                        if (dbg) {
                            output += "/*L2*/";
                        }
                    }
                    else {
                        output += "( " + init[0] + " )";
                        if (dbg) {
                            output += "/*L3*/";
                        }
                    }
                }

                if (dbg) {
                    output += "/*pair def*/";
                }
                output += ";\n";

                std::string src = "data_.first";
                if (!structChainType.empty()) {
                    src = "\n        data_.first.template get<" + gen.m_ns + "::" + structChainType + ">()";
                }
                for (VariableData &v : cmd->outParams) {
                    v.createLocalReferenceVar(gen, "      ", src, output);
                    src = "data_.second";
                }
            } else {
                VariableData &v = cmd->outParams[0];
                returnId        = v.identifier();
                v.createLocalVar(gen, "      ", dbg ? "/*var def*/" : "", output);

                if (!structChainType.empty()) {
                    std::string type = gen.m_ns + "::" + structChainType;
                    output += "      " + type + " &" + structChainIdentifier + " =\n";
                    output += "        structureChain.template get<" + type + ">();\n";
                }
            }
            for (VariableData *v : countVars) {
                if (v->getIgnoreProto() && !v->getIgnoreFlag()) {
                    v->createLocalVar(gen, "      ", dbg ? "/*count def*/" : "", output);
                }
            }

            if (cmd->pfnReturn == Command::PFNReturnCategory::VK_RESULT) {
                output += "      " + declareReturnVar();
            }
        }

        return vectorSizeVar;
    }

    void MemberResolverDefault::generateMemberBodyArray(std::string &output, std::string &returnId, bool &returnsRAII,  VariableData *vectorSizeVar, bool dbg) {

        const auto &cfg = gen.getConfig();

            std::string id   = cmd->outParams[0].get().identifier();
            std::string size = vectorSizeVar->identifier();
            std::string call = generatePFNcall();

            std::string resizeCode;
            std::string downsizeCode;
            for (VariableData &v : cmd->outParams) {
                if (v.isArray()) {
                    v.setAltPFN("nullptr");
                    std::string arg = size;
                    const auto &id  = v.identifier();
                    auto       *var = v.getLengthVar();
                    if (var) {
                        arg = var->identifier();
                    }
                    if (v.getSpecialType() == VariableData::TYPE_EXP_VECTOR) {
                        resizeCode += "          " + id + ".resize_optim( " + arg + " );\n";
                        downsizeCode += "      " + id + ".confirm( " + arg + " );\n";
                    } else {
                        resizeCode += "          " + id + ".resize( " + arg + " );\n";
                        downsizeCode += "      if (" + arg + " < " + id + ".size()) " + (cfg.gen.branchHint? "VULKAN_HPP_UNLIKELY " : "") + "{\n";
                        downsizeCode += "        " + id + ".resize( " + arg + " );\n";
                        downsizeCode += "      }\n";
                    }
                }
            }

            std::string callNullptr = generatePFNcall();

            if (cmd->pfnReturn == Command::PFNReturnCategory::VK_RESULT) {
                std::string cond = successCodesCondition(resultVar.identifier());

                std::string resultSuccess    = cfg.gen.internalVkResult ? "VK_SUCCESS" : "Result::eSuccess";
                std::string resultIncomplete = cfg.gen.internalVkResult ? "VK_INCOMPLETE" : "Result::eIncomplete";

                output += vkgen::format(R"(
    do {{
      {0}
      if (result == {1} && {2}) {3}{{
)",
                                        callNullptr,
                                        resultSuccess,
                                        size,
                                        cfg.gen.branchHint? "VULKAN_HPP_LIKELY " : "");

                output += resizeCode;
                output += vkgen::format(R"(
        {0}
      }}
    }} while (result == {1});
)",
                                        call,
                                        resultIncomplete);

            } else {
                output += "      " + callNullptr + "\n";
                output += resizeCode;
                output += "      " + call + "\n";
            }

            output += generateCheck();

            output += downsizeCode;
    }

    std::string MemberResolverDefault::generateMemberBody() {
        std::string output;
        const auto &cfg = gen.getConfig();
        const bool  dbg = cfg.dbg.methodTags;
        if (dbg) {
            output += "// MemberResolverDefault \n";
        }
        bool immediate = returnType != "void" && cmd->pfnReturn != Command::PFNReturnCategory::VOID && cmd->outParams.empty() && !usesResultValueType();

        bool          returnsRAII    = false;
        std::string   returnId;

        VariableData *vectorSizeVar = generateVariables(output, returnId, returnsRAII, dbg);


        bool          hasPoolArg     = false;

        if (vectorSizeVar) {
             generateMemberBodyArray(output, returnId, returnsRAII, vectorSizeVar, dbg);
        }
        else {
            output += "      " + generatePFNcall(immediate && !constructor) + "\n";
            if (cmd->pfnReturn != Command::PFNReturnCategory::VOID) {
                output += generateCheck();
            }
        }

        const auto createEmplaceRAII = [&]() {
            std::string output;
            for (const VariableData &v : cmd->outParams) {
                const std::string &id     = v.identifier();
                const auto        &handle = gen.findHandle(v.original.type());

                if (!constructor) {
                    output += "      " + v.getReturnType(gen) + " _" + id + ";";
                    if (dbg) {
                        // output += "/*var ref*/";
                    }
                    output += '\n';
                }
                std::string iter = strFirstLower(v.type());
                std::string dst  = constructor ? "this->" : "_" + id + ".";
                std::string arg  = getSuperclassArgument(handle.superclass);
                std::string poolArg;
                if (handle.secondOwner) {
                    poolArg += ", " + createArgumentWithType(handle.secondOwner->type());
                    if (dbg) {
                        output += "/*pool: " + poolArg + " */\n";
                    }
                    hasPoolArg = true;
                }

                output += vkgen::format(R"(
    {0}reserve({1}.size());
    for (auto const &{2} : {3}) {{
      {0}emplace_back({4}, {2}{5});
    }}
)",
                                        dst,
                                        id,
                                        iter,
                                        id,
                                        arg,
                                        poolArg);
            }

            return output;
        };

        if (returnsRAII) {
            if (cmd->outParams.size() > 1) {
                std::cerr << "Warning: unhandled returning multiple RAII types. " << name << '\n';
            }
            /*
            for (const VariableData &v : cmd->outParams) {
                const std::string &id = v.identifier();
                const auto &handle = gen.findHandle(v.original.type());

                if (!constructor) {
                    output += "    " + v.getReturnType() + " _" + id + ";";
                    if (dbg) {
                        // output += "//var ref";
                    }
                    output += '\n';
                }
                std::string iter = strFirstLower(v.type());
                std::string dst = constructor? "this->" : "_" + id + ".";
                std::string arg = getSuperclassArgument(handle.superclass);
                std::string poolArg;
                if (handle.secondOwner) {
                    poolArg += ", " + createArgumentWithType(handle.secondOwner->type());
                    if (dbg) {
                        output += "// pool: " + poolArg + " \n";
                    }
                    hasPoolArg = true;
                }

                output += format(R"(
{0}reserve({1}.size());
for (auto const &{2} : {3}) {
  {0}emplace_back({4}, {2}{5});
}
)",                     dst, id, iter, id, arg, poolArg);

            }*/

            const VariableData &v = cmd->outParams[0];
            output += createEmplaceRAII();
            returnId = "_" + v.identifier();
        }

        if (!returnId.empty() && !immediate && !constructor) {
            output += "      return " + generateReturnValue(returnId) + ";";
            if (dbg) {
                // output += "/*.R*/";
            }
            output += '\n';
        }

        const auto createInternalCall = [&]() {
            auto       &var   = cmd->outParams[0].get();
            std::string type  = var.namespaceString(gen, true) + var.type();
            std::string ctype = var.original.type();
            var.setIgnorePFN(true);
            var.getLengthVar()->setIgnorePFN(true);

            std::string func = "createArray";
            if (cmd->pfnReturn == Command::PFNReturnCategory::VOID) {
                func += "VoidPFN";
            }

            std::string pfn      = getDispatchPFN();
            std::string msg      = createCheckMessageString();
            std::string pfnType  = "PFN_" + name.original;
            std::string sizeType = var.getLengthVar()->type();
            // output = "#if 0\n" + output + "#endif\n";
            std::string output;
            output += "internal::" + func;
            output += "<" + type + ", " + ctype + ", " + sizeType + ", " + pfnType + ">";
            output += "(" + pfn + ", " + msg;

            for (const VariableData &p : cmd->params) {
                if (!p.getIgnorePFN()) {
                    output += ", " + createPFNArguments();
                    break;
                }
            }
            output += ")";
            return output;
        };

//        if (arrayVariation && gen.getConfig().gen.internalFunctions && cmd->outParams.size() == 1) {
//            if (returnsRAII) {
//                if (hasPoolArg) {
//                    ////                        output += "// TODO createHandlesWithPoolRAII " + t + "\n";
//                } else {
//                    if (!inputSizeVar && !usesResultValue() && cmd->outParams.size() == 1) {
//                        VariableData &var = cmd->outParams[0];
//                        output            = "";
//                        // output += "// TODO createHandlesRAII \n";
//                        var.createLocalVar(gen, "      ", dbg ? "/*var def*/" : "", output, createInternalCall());
//                        output += createEmplaceRAII();
//                        if (!returnId.empty() && !immediate && !constructor) {
//                            output += "      return " + generateReturnValue(returnId) + ";";
//                            if (dbg) {
//                                // output += "/*.R*/";
//                            }
//                            output += '\n';
//                        }
//                    }
//                }
//            } else {
//                if (!inputSizeVar && !usesResultValue()) {
//                    output = "";
//                    output += "      return ";
//                    output += createInternalCall();
//                    output += ";\n";
//                }
//            }
//        }

        return output;
    }

    MemberResolverDefault::MemberResolverDefault(const Generator &gen, ClassCommand &d, MemberContext &ctx, bool constructor)
      : MemberResolver(gen, d, ctx, constructor) {
        // std::cerr << "MemberResolverDefault()  " << d.src->name.original << '\n';
        transformMemberArguments();
    }

    MemberResolverDefault::~MemberResolverDefault() {}

//    void MemberResolverDefault::generate(UnorderedFunctionOutput &decl, UnorderedFunctionOutput &def) {
//        MemberResolver::generate(decl, def);
//    }

    MemberResolverStaticDispatch::MemberResolverStaticDispatch(const Generator &gen, ClassCommand &d, MemberContext &ctx) : MemberResolver(gen, d, ctx) {
        returnType = cmd->type;
        name       = name.original;
        dbgtag     = "static dispatch";
    }

    std::string MemberResolverStaticDispatch::temporary() const {
        std::string protoArgs = createProtoArguments(true);
        std::string args      = createStaticPassArguments(true);
        std::string proto     = cmd->type + " " + name.original + "(" + protoArgs + ")";
        std::string call;
        if (cmd->pfnReturn != Command::PFNReturnCategory::VOID) {
            call += "return ";
        }

        call += "::" + name + "(" + args + ");";

        return vkgen::format(R"(
    {0} const VULKAN_HPP_NOEXCEPT {{
      {1}
    }}
)",
                             proto,
                             call);
    }

    std::string MemberResolverStaticDispatch::generateMemberBody() {
        return "";
    }

    MemberResolverClearRAII::MemberResolverClearRAII(const Generator &gen, ClassCommand &d, MemberContext &ctx) : MemberResolver(gen, d, ctx) {
        for (VariableData &p : cmd->params) {
            p.setIgnoreFlag(true);
            p.setIgnoreProto(true);
            if (p.type() == "uint32_t") {
                p.setAltPFN("1");
            }
        }

        dbgtag = "raii clear";
    }

    std::string MemberResolverClearRAII::temporary(const std::string &handle) const {
        std::string call;
        call            = "        if (" + handle + ") {\n";
        std::string src = getDispatchSource();
        call += "          " + src + cls->dtorCmd->name.original + "(" + createPFNArguments() + ");\n";
        call += "        }\n";
        return call;
    }

    std::string MemberResolverClearRAII::generateMemberBody() {
        return "";
    }

    MemberResolverStaticVector::MemberResolverStaticVector(const Generator &gen, ClassCommand &d, MemberContext &ctx) : MemberResolverDefault(gen, d, ctx) {
        int converted = 0;
        for (VariableData &v : cmd->outParams) {
            if (v.isArray()) {
                const auto &type = v.namespaceString(gen) + v.type();

                std::string size = "N";
                if (converted > 0) {
                    size += std::to_string(converted);
                }

                if (v.isLenAttribIndirect()) {
                    v.setNamespace(Namespace::STD);
                    v.setFullType("", "array<" + type + ", " + size + ">", "");
                    v.setSpecialType(VariableData::TYPE_EXP_ARRAY);
                } else {
                    v.setNamespace(Namespace::VK);
                    v.setFullType("", "Vector<" + type + ", " + size + ">", "");
                    v.setSpecialType(VariableData::TYPE_EXP_VECTOR);
                }
                auto temp = v.getTemplate();
                if (!temp.empty()) {
                    temp += ", ";
                }
                temp += "size_t " + size;
                v.setTemplate(temp);

                converted++;
            }
        }
        if (converted > 1) {
            dbgfield += "      // static vector multiple return\n";
            //                multiple = true;
        }

        returnType = createReturnType();

        dbgtag = "static vector";
    }

    MemberResolverVectorRAII::MemberResolverVectorRAII(const Generator &gen, ClassCommand &d, MemberContext &refCtx) : MemberResolverDefault(gen, d, refCtx) {
        // cmd->pfnReturn = PFNReturnCategory::VOID;
        ctx.useThis = true;

        dbgtag = "RAII vector";
    }

    std::string MemberResolverVectorRAII::generateMemberBody() {
        std::string args = createPassArguments(true);
        return "      return " + last->namespaceString(gen) + last->type() + "s(" + args + ");\n";
    }

    MemberResolverCtor::MemberResolverCtor(const Generator &gen, ClassCommand &d, MemberContext &refCtx)
      : MemberResolverDefault(gen, d, refCtx, true), _name("") {
        _name = gen.convertCommandName(name.original, cls->superclass);
        name  = std::string(cls->name);

        if (cmd->params.empty()) {
            std::cerr << "error: MemberResolverCtor" << '\n';
            hasDependencies = false;
            return;
        }

        if (ctx.returnSingle) {
            for (VariableData &p : cmd->params) {
                if (p.isArrayIn()) {
                    p.setSpecialType(VariableData::Type::TYPE_DEFAULT);
                    p.setReference(true);
                    auto *var = p.getLengthVar();
                    var->setAltPFN("1");
                    // std::cout << ">> " << var->identifier() << "\n";
                }
            }
        }

        /*
        VariableData *parent = &cmd->params.begin()->get();

        ownerInParent = false;
        if (parent->original.type() != cls->superclass.original) {
            ownerInParent = true;
        }

        std::string id = parent->identifier();
        pfnSourceOverride = id;
        if (ownerInParent) {
            pfnSourceOverride += ".get" + cls->superclass + "()";
        }

        bool hasParentArgument = false;
        for (const VariableData &p : cmd->params) {
            if (p.original.type() == cls->superclass.original) {
                // p->setIgnorePass(true);
                src = p.identifier();
            } else if (cls->parent && p.original.type() == cls->parent->name.original) {
                // this handle is not top level
                if (src.empty()) {
                    src           = p.identifier() + ".get" + cls->superclass + "()";
                    ownerInParent = false;
                }
            }
        }

        if (src.empty()) {  // VkInstance
            src           = "defaultContext";
            ownerInParent = false;
        }


        pfnSourceOverride = src;
        if (ownerInParent) {
            pfnSourceOverride += "->getDispatcher()->";
        } else {
            pfnSourceOverride += ".getDispatcher()->";
        }
        */

        superclassSource = getSuperclassSource();

        pfnSourceOverride = superclassSource.getDispatcher() + "->";

        src = (cls && cls->isSubclass)? pfnSourceOverride : "*" + superclassSource.getDispatcher();

        specifierInline   = true;
        specifierExplicit = true;
        specifierConst    = false;

        if (!last || last->isArray()) {
            hasDependencies = false;
            return;
        }

        dbgtag = "raii constructor";
    }

    MemberResolverCtor::SuperclassSource MemberResolverCtor::getSuperclassSource() const {
        SuperclassSource s;
        const auto      &type = cls->superclass;
        // pfnSourceOverride += ".get" + cls->superclass + "()";
        for (const VariableData &v : cmd->params) {
            if (!v.getIgnoreProto() && v.type() == type) {
                s.src       = v.identifier();
                s.isPointer = v.isPointer();
                return s;
            }
        }
        for (const VariableData &v : cmd->params) {
            if (!v.getIgnoreProto() && v.getNamespace() == Namespace::RAII && v.isHandle()) {
                s.src = v.identifier();
                s.src += v.isPointer() ? "->" : ".";
                s.src += "get" + type + "()";
                s.isPointer  = v.isPointer();
                s.isIndirect = true;
                return s;
            }
        }
        // not found, use default
        s.src = "/*NOT FOUND: " + type + "*/";
        std::cerr << "/*NOT FOUND: " << type << "*/" << '\n';
        for (const VariableData &v : cmd->params) {
            std::cerr << "  > " << v.type() << "  " << v.getIgnoreProto() << '\n';
        }
        return s;
    }

    std::string MemberResolverCtor::generateMemberBody() {
        std::string output;

        const std::string &owner = cls->ownerhandle;
        if (!owner.empty() && !constructorInterop && !ctx.exp) {
            output += "      " + owner + " = " + superclassSource.getSuperclassAssignment() + ";\n";
        }

        std::string call = generatePFNcall();

        output += "      " + call + "\n";
        output += generateCheck();

        if (!cls->isSubclass && !constructorInterop) {
            const auto &superclass = cls->superclass;
            const auto &cfg = gen.getConfig();
            bool unique = !cfg.gen.expApi || cfg.gen.dispatchTableAsUnique;
            unique &= !(cls->name == "Instance" && cfg.gen.raii.staticInstancePFN);
            unique &= !(cls->name == "Device" && cfg.gen.raii.staticDevicePFN);

            if (unique) {
                output += vkgen::format("      m_dispatcher.reset( new {2}Dispatcher( {1}, {3} ) );\n",
                                        superclass,
                                        src,
                                        cls->name,
                                        cls->vkhandle.toArgument(gen));
            } else {
                // std::string first = cfg.gen.integrateVma? getDispatchDeref() : getDispatchSource() + "vkGet" + cls->name + "ProcAddr";
                output += vkgen::format(
                  // "      m_dispatcher = {0}Dispatcher( {1}vkGet{0}ProcAddr, {2} );\n", cls->name, getDispatchSource(), cls->vkhandle.toArgument(gen));
                  "      m_dispatcher = {0}Dispatcher( {1}, {2} );\n", cls->name, src, cls->vkhandle.toArgument(gen));
            }
        }

        return output;
    }

    MemberResolverVectorCtor::MemberResolverVectorCtor(const Generator &gen, ClassCommand &d, MemberContext &refCtx) : MemberResolverCtor(gen, d, refCtx) {
        if (cmd->params.empty()) {
            std::cerr << "error: MemberResolverVectorCtor" << '\n';
            hasDependencies = false;
            return;
        }
        hasDependencies = true;

        clsname += "s";
        name += "s";
        // last = cmd->getLastPointerVar();

        specifierInline   = true;
        specifierExplicit = false;
        specifierConst    = false;
        dbgtag            = "vector constructor";
    }

    std::string MemberResolverVectorCtor::generateMemberBody() {
        return MemberResolverDefault::generateMemberBody();
    }

    MemberResolverUniqueCtor::MemberResolverUniqueCtor(Generator &gen, ClassCommand &d, MemberContext &refCtx) : MemberResolverDefault(gen, d, refCtx, true) {
        name = "Unique" + cls->name;

        InitializerBuilder init("        ");
        const auto        &vars = getFilteredProtoVars();
        // base class init
        for (const VariableData &p : vars) {
            if (p.type() == cls->name) {
                init.append(cls->name, p.identifier());
                break;
            }
        }

        cls->foreachVars(VariableData::Flags::CLASS_VAR_UNIQUE, [&](const VariableData &v) {
            for (const VariableData &p : vars) {
                if (p.type() == v.type() || (p.getSpecialType() == VariableData::TYPE_DISPATCH && v.getSpecialType() == VariableData::TYPE_DISPATCH)) {
                    init.append(v.identifier(), p.toVariable(gen, v));
                }
            }
        });

        initializer = init.string();

        specifierInline    = false;
        specifierExplicit  = true;
        specifierConst     = false;
        ctx.generateInline = true;
        dbgtag             = "unique constructor";
    }

    std::string MemberResolverUniqueCtor::generateMemberBody() {
        return "";
    }

    MemberResolverCreateHandleRAII::MemberResolverCreateHandleRAII(Generator &gen, ClassCommand &d, MemberContext &refCtx)
      : MemberResolverDefault(gen, d, refCtx) {
        ctx.useThis = true;

        dbgtag = "create handle raii";
    }

    std::string MemberResolverCreateHandleRAII::generateMemberBody() {
        const bool returnsSubclass = gen.findHandle(last->original.type()).isSubclass;

        if (last->isArray() && !ctx.returnSingle) {
            return MemberResolverDefault::generateMemberBody();
        } else if (gen.getConfig().gen.expApi && cls->isSubclass && !returnsSubclass) {
            //                        last->setIdentifier("handle");
            //                        last->setAltPFN("std::bit_cast<" + last->original.type() + "*>(&handle)");
            //                        std::string output = "    " + last->type() + " handle;\n";
            //                        output += "    " + generatePFNcall();
            //                        output += generateCheck();
            //                        output += "    return " + returnType + "(handle);\n";
            cmd->params[0].get().setNamespace(Namespace::VK);
            return vkgen::format(R"(      return {0}(*{1}, {2});
)",
                                 returnType,
                                 cls->ownerhandle,
                                 createPassArgumentsRAII());
        } else {
            return vkgen::format(R"(      return {0}({1});
)",
                                 returnType,
                                 createPassArgumentsRAII());
        }
    }

    MemberResolverCreate::MemberResolverCreate(const Generator &gen, ClassCommand &d, MemberContext &refCtx) : MemberResolverDefault(gen, d, refCtx) {
        dbgtag      = cmd->nameCat == Command::NameCategory::ALLOCATE ? "allocate" : "create";
        ctx.useThis = true;

        if (ctx.returnSingle) {
            std::string tag = gen.strRemoveTag(name);
            auto        pos = name.find_last_of('s');
            if (pos != std::string::npos) {
                name.erase(pos);
            } else {
                std::cerr << "MemberResolverCreate single no erase! " << name << std::endl;
            }
            name += tag;

            auto var = last->getLengthVar();
            if (var) {
                if (!last->isLenAttribIndirect()) {
                    var->setAltPFN("1");
                }
                for (VariableData *v : var->getArrayVars()) {
                    if (v->isArrayIn()) {
                        v->convertToConstReference();
                        auto id  = v->identifier();
                        auto pos = id.find_last_of('s');
                        if (pos != std::string::npos) {
                            id.erase(pos);
                            v->setIdentifier(id);
                        }
                    }
                }
            }
        }
    }

    std::string MemberResolverCreate::generateMemberBody() {
        std::string output;

        if (last->isArray() && !ctx.returnSingle) {
            return MemberResolverDefault::generateMemberBody();
        }
        if (ctx.isStatic) {
            // output += "      " + last->original.type() + " = " + cmd->name.original + "\n";
            const auto &id = last->identifier();
            output += "      " + last->original.type() + " " + id + ";\n";
            output += "      " + generatePFNcall();
            output += "      " + generateCheck();

            returnValue = last->fullType(gen) + "(" + id + ")";
        }
        else {
            if (ctx.returnSingle && last->isLenAttribIndirect()) {
                auto rhs = last->getLenAttribRhs();
                if (!rhs.empty()) {
                    auto var = last->getLengthVar();
                    if (var) {
                        output += "      VULKAN_HPP_ASSERT( " + var->identifier() + "." + rhs + " == 1 );\n";
                    }
                }
            }
            // output += "/* MemberResolverCreate */\n";
            if (ctx.ns == Namespace::RAII || true) {
                last->setIgnorePFN(true);
                std::string args;

                if (!cmd->params.empty()) {
                    auto &first = cmd->params.begin()->get();
                    // output += "  // " + first.original.type() + " == " + cls->name.original + "\n";
                    if (first.original.type() == cls->name.original) {
                        first.setIgnorePFN(true);
                    }
                    else {
                        if (!cls->isSubclass) {
                            args = "*this, ";
                        }
                        // args = "*this, ";
                    }
                }
                args += createPassArguments(true);
                output += "      return " + last->fullType(gen) + "(" + args + ");\n";
            } else {
                const std::string &call = generatePFNcall();
                std::string        id   = last->identifier();
                output += "      " + last->fullType(gen) + " " + id + ";\n";
                output += "      " + call + "\n";
                output += generateCheck();
                returnValue = generateReturnValue(id);
            }
        }

        return output;
    }

    MemberResolverCreateUnique::MemberResolverCreateUnique(const Generator &gen, ClassCommand &d, MemberContext &refCtx) : MemberResolverCreate(gen, d, refCtx) {
        if (ctx.returnSingle) {
            returnType = gen.m_ns + "::Unique" + last->type();
        }
        else {
            returnType = "Unique" + last->type();
        }

        if (last->isHandle()) {
            const auto &handle = gen.findHandle(last->original.type());
            isSubclass         = handle.isSubclass;
            if (handle.poolFlag) {
                const auto &parent = handle.parent->name;
                for (const VariableData &p : cmd->params) {
                    if (p.isStruct()) {
                        if (p.getIgnoreProto()) {
                            continue;
                        }
                        const auto &s = gen.structs[p.original.type()];
                        for (const auto &m : s.members) {
                            if (m->type() == parent) {
                                poolSource = p.identifier() + "." + m->identifier();
                            }
                        }
                    }
                }

            }
        }
        if (last->isArrayOut() && !ctx.returnSingle) {

            const auto &id = last->identifier();
            uniqueVector = std::make_unique<VariableData>(*last);

            uniqueVector->setType(returnType);
            uniqueVector->original.setIdentifier(id);
            uniqueVector->setIdentifier("unique" + strFirstUpper(id));


            returnType = uniqueVector->fullType(gen);
            returnValue = "std::move(" + uniqueVector->identifier() + ")";

        }

        // original = name;
        name += "Unique";
        dbgtag = "create unique";

        constructor = true;
//        VariableData &first = *cmd->params.begin();
//        first.setIgnorePass(true);
    }

    std::string MemberResolverCreateUnique::generateMemberBody() {
        std::string output;

        const auto &cfg = gen.getConfig();
        std::string deleter;
        if (isSubclass) {
            deleter += "*this";
        }
        if (!poolSource.empty()) {
            if (!deleter.empty()) {
                deleter += ", ";
            }
            deleter += poolSource;
        }
        if (cfg.gen.allocatorParam && allocatorVar && !allocatorVar->getIgnoreProto()) {
            if (!deleter.empty()) {
                deleter += ", ";
            }
            deleter += "allocator";
        }
        if (!cfg.gen.expApi && cfg.gen.dispatchParam) {
            if (!deleter.empty()) {
                deleter += ", ";
            }
            deleter += "d";
        }

        if (uniqueVector) {
            const bool dbg = false;

            output += MemberResolverDefault::generateMemberBody();

            output += '\n';

            uniqueVector->createLocalVar(gen, "      ", dbg ? "/*unique var def*/" : "", output);
            output += uniqueVector->generateVectorReserve(gen, "      ");

            output += vkgen::format(R"(
      for ( const auto &handle : {0} )
      {{
        {1}.emplace_back(handle, {{{2}}});
      }}
)",                 uniqueVector->original.identifier(), uniqueVector->identifier(), deleter);

            return output;
        }

        std::string args;

        if (ctx.isStatic) {
            // output += "      " + last->original.type() + " = " + cmd->name.original + "\n";
            const auto &id = last->identifier();
            output += "      " + last->original.type() + " " + id + ";\n";
            output += "      " + generatePFNcall();
            output += "      " + generateCheck();

            args += id + ", {";
            args += deleter;
            args += "}";
        }
        else {
            args += "{";
            args += deleter;
            args += "}, ";
            if (!isSubclass && !cls->isSubclass) {
                args += "*this, ";
            }
            args += createPassArguments(false);
        }

        returnValue        = generateReturnValue(returnType + "(" + args + ")");
        return output;
    }

    MemberGeneratorExperimental::MemberGeneratorExperimental(const Generator &gen, ClassCommand &m, UnorderedFunctionOutputX &decl, UnorderedFunctionOutputGroup &out, bool isStatic)
      : gen(gen), m(m), decl(decl), out(out) {
        ctx.ns              = Namespace::VK;
        if (isStatic) {
            ctx.isStatic = true;
        }
        else {
            if (gen.getConfig().gen.expApi) {
                ctx.disableDispatch = true;
                ctx.exp             = true;
                //
            }
            if (m.cls && !m.cls->name.empty()) {
                if (m.cls->isSubclass) {
                    ctx.insertSuperclassVar = true;
                }
            } else {
                ctx.insertSuperclassVar = true;
                ctx.isStatic            = true;
                ctx.generateInline      = true;
            }
        }
    }

    void MemberGeneratorExperimental::generateStructChain() {
        if (!m.src->isStructChain()) {
            return;
        }

        // out.def += "    // Chain: " + m.src->name + " (" + m.src->name.original + ")\n";
        ctx.structureChain = true;
        generate<MemberResolverDefault>();
        if (m.src->returnsVector()) {
            ctx.addVectorAllocator = true;
            generate<MemberResolverDefault>();
            ctx.addVectorAllocator = false;
        }
        ctx.structureChain = false;
    }

    void MemberGeneratorExperimental::generateDefault() {

        generateStructChain();

        if (m.src->returnsVector()) {
            // ctx.templateVector = true;
        }
        generate<MemberResolverDefault>();
        if (m.src->returnsVector()) {
            // ctx.templateVector = true;
            ctx.addVectorAllocator = true;
            generate<MemberResolverDefault>();
            ctx.addVectorAllocator = false;
        }
        if (m.src->returnsVector() && gen.cfg.gen.functionsVecAndArray) {
            ctx.templateVector = false;
            ctx.generateInline = false;
            generate<MemberResolverStaticVector>();
            //                    {
            //                        ctx.addVectorAllocator = true;
            //                        std::vector<Protect>       protects;
            //                        MemberResolverStaticVector resolver{ gen, m, ctx };
            //                        generate(resolver, protects);
            //                    }
        }

    }

    void MemberGeneratorExperimental::generateCreate() {

        generate<MemberResolverCreate>();
        const bool vector = m.src->returnsVector();
        if (vector) {
            // out.sFuncs += " // RETURN SINGLE\n";
            ctx.addVectorAllocator = true;
            generate<MemberResolverCreate>();
            ctx.addVectorAllocator = false;
            ctx.returnSingle = true;
            generate<MemberResolverCreate>();
            ctx.returnSingle = false;
        }
        auto *last = m.src->getLastHandleVar();
        if (!last) {
            std::cerr << "no last handle: " << m.src->name.original << '\n';
            return;
        }
        const auto &h = gen.findHandle(last->original.type());
        if (ctx.ns == Namespace::VK && h.uniqueVariant()) {
            std::array<Protect, 1> p = {Protect{"VULKAN_HPP_NO_SMART_HANDLE", false}};

            generate<MemberResolverCreateUnique>(p);
            if (vector) {
                // std::cout << m.name << "\n";
                ctx.addVectorAllocator = true;
                generate<MemberResolverCreateUnique>(p);
                ctx.addVectorAllocator = false;
                ctx.returnSingle = true;
                generate<MemberResolverCreateUnique>(p);
                ctx.returnSingle = false;
            }
        }
    }

    void MemberGeneratorExperimental::generateDestroy(ClassCommand &m, MemberContext &ctx, const std::string &name) {
        generate<MemberResolverDefault>();

        const auto &orig = m.src->name.original;
        if (orig != "vkDestroyInstance" && orig != "vkDestroyDevice" && m.src->hasOverloadedDestroy()) {
            // std::cerr << "skip generate destroy overload: " << m.src->name.original << '\n';
            return;
        }

        const auto &type = m.cls->name;

        m.src->prepare();
#ifndef NDEBUG
        bool ok;
        m.src->check(ok);
#endif
        auto *last = m.src->getLastHandleVar();
        if (!last) {
            std::cerr << "no last handle: " << m.src->name.original << '\n';
            return;
        }

        const auto &lastType = last->type();
        // deprecated if
        if (last->original.type() == m.src->name.original) {
            std::cerr << "can't generate destroy: " << last->original.type() << " != " << m.src->name.original << '\n';
            return;
        }

        auto second = MemberResolverDefault{ gen, m, ctx };
        // TODO move to ctx
        const auto getFirst = [&]() -> VariableData * {
            const auto &vars = second.getFilteredProtoVars();
            if (vars.empty()) {
                return nullptr;
            }
            return &vars.begin()->get();
        };
        auto first = getFirst();
        if (!first || !first->isHandle()) {
            // std::cerr << "can't generate destroy: drop  " << m.cls->name << ": " << last->type() << "  " << m.src->name.original << '\n';
            return;
        }
        first->setOptional(false);

        second.name   = name;
        second.dbgtag = "d. overload";

        std::vector<Protect> protects;
        generate(second, protects);
    }


    void MemberGeneratorExperimental::generate() {
        if (!m.src->canGenerate() || !m.src->top) {
            return;
        }

        if (m.raiiOnly && ctx.ns != Namespace::RAII) {
            return;
        }

//        std::cout << "    // C2: " + m.src->name + " (" + m.src->name.original + ") ";
//        if (m.src->isIndirect()) std::cout << "INDIRECT";
//        std::cout <<  "\n";

        // out.sFuncs += "    // C: " + m.src->name + " (" + m.src->name.original + ")\n";

        if (m.src->canTransform()) {
            generatePass();
        }

        m.src->prepare();
//        auto *last = m.src->getLastVar();  // last argument
//        if (!last) {
//            // std::cout << "null access getPrimaryResolver" << '\n';
//            // return;
//        }
//        if (gen.getConfig().gen.expApi && last->isOutParam() && last->isHandle() && !gen.findHandle(last->original.type()).isSubclass) {
//            std::cout << "skip C: " << m.cls->name << "::" << m.name.original << "\n";
//            return;
//        }

        switch (m.src->nameCat) {
            case Command::NameCategory::ALLOCATE:
            case Command::NameCategory::CREATE:
                generateCreate();
                return;
            case Command::NameCategory::DESTROY: generateDestroy(m, ctx, "destroy"); return;
            case Command::NameCategory::FREE: generateDestroy(m, ctx, "free"); return;
            default:
                generateDefault();
                return;
        }
    }

}  // namespace vkgen