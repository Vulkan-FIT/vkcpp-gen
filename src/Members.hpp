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

#ifndef GENERATOR_MEMBERS_HPP
#define GENERATOR_MEMBERS_HPP

#include "Output.hpp"
#include "Registry.hpp"

namespace vkgen
{

    using namespace vkr;

    class Generator;

    struct MemberContext
    {
        Namespace ns                         = {};
        bool      useThis                    = {};
        bool      isStatic                   = {};
        bool      inUnique                   = {};
        bool      generateInline             = {};
        bool      disableSubstitution        = {};
        bool      disableDispatch            = {};
        bool      disableArrayVarReplacement = {};
        bool      disableAllocatorRemoval    = {};
        bool      returnSingle               = {};
        bool      insertClassVar             = {};
        bool      insertSuperclassVar        = {};
        bool      commentOut                 = {};
        bool      templateVector             = {};
        bool      exp                        = {};
    };

    class MemberResolver
    {
      public:
        using Var = VariableData;

        const Generator                           &gen;
        const Handle                              *cls = {};
        Command                                   *cmd = {};
        String                                     name;
        std::string                                clsname;
        std::string                                pfnSourceOverride;
        std::string                                pfnNameOverride;
        std::map<std::string, std::string>         varSubstitution;
        std::vector<std::unique_ptr<VariableData>> tempVars;

        std::string              returnType;
        std::string              returnValue;
        std::string              initializer;
        VariableData             resultVar;
        VariableData            *last         = {};
        VariableData            *allocatorVar = {};
        std::vector<std::string> usedTemplates;  // TODO rewrite

        std::string guard;

        MemberContext ctx;
        bool          constructor          = {};
        bool          constructorInterop   = {};
        bool          specifierInline      = {};
        bool          specifierExplicit    = {};
        bool          specifierConst       = {};
        bool          specifierConstexpr   = {};
        bool          specifierConstexpr14 = {};

        static bool filterProto(const VariableData &v, bool same) {
            return !v.getIgnoreProto();
        }

        static bool filterPass(const VariableData &v, bool same) {
            return !v.getIgnorePass();
        }

        static bool filterRAII(const VariableData &v, bool same) {
            return !v.getIgnorePass() && (!v.getIgnoreFlag() || same);
        }

        static bool filterIngnoreFlag(const VariableData &v, bool same) {
            return !v.getIgnoreFlag();
        }

        bool filterSuperclass(const VariableData &v, bool same) const {
            return v.original.type() == cls->superclass.original;
        }

        static bool filterPFN(const VariableData &v, bool same) {
            return !v.getIgnorePFN();
        }

        std::string createArguments(std::function<bool(const VariableData &, bool)>  filter,
                                    std::function<std::string(const VariableData &)> function,
                                    bool                                             proto,
                                    bool                                             pfn) const;

        std::string createArgument(std::function<bool(const VariableData &, bool)>  filter,
                                   std::function<std::string(const VariableData &)> function,
                                   bool                                             proto,
                                   bool                                             pfn,
                                   const VariableData                              &var) const;

        std::string createArgument(const VariableData &var, bool sameType, bool pfn) const;

        void reset();

        std::string declareReturnVar(const std::string &assignment = "");

        virtual std::string generateMemberBody();

        std::string castTo(const std::string &type, const std::string &src) const;

        bool useDispatchLoader() const;

        bool isTemplated() const;

        std::string getDispatchDeref() const;

        std::string getDispatchSource() const;

        std::string getDispatchPFN() const;

        std::string generatePFNcall(bool immediateReturn = false);

        std::string assignToResult(const std::string &assignment);

        std::string generateReturnValue(const std::string &identifier);

        std::string createCheckMessageString() const;

        std::string generateCheck();

        bool usesResultValue() const;

        bool usesResultValueType() const;

        std::string generateReturnType() const;

        std::string createReturnType() const;

        std::string generateNodiscard();

        std::string getSpecifiers(bool decl);

        std::string getProto(const std::string &indent, const std::string &prefix, bool declaration, bool &usesTemplate);

        std::string getDbgtag(const std::string &prefix, bool bypass = false);

        std::string createProtoArguments(bool declaration = false);

        void transformToArray(VariableData &var);

        void prepareParams();


        void setOptionalAssignments();

        bool checkMethod() const;

      public:
        std::string dbgtag;
        std::string dbgfield;

        bool hasDependencies = {};

        bool _indirect  = {};
        bool _indirect2 = {};

        MemberResolver(const Generator &gen, const ClassCommand &d, MemberContext &c, bool constructor = {});

        virtual ~MemberResolver();

        virtual void generate(UnorderedFunctionOutput &decl, UnorderedFunctionOutput &def);

        void generateX(std::string &def);

        void generateX(std::string &decl, std::string &def);

        template <typename... Args>
        VariableData &addVar(decltype(cmd->params)::iterator pos, Args &&...args) {
            auto &var = tempVars.emplace_back(std::make_unique<VariableData>(args...));

            //            std::stringstream s;
            //            s << std::hex << var.get();
            // dbgfield += "    // alloc: <" + s.str() + ">\n";
            cmd->params.insert(pos, std::ref(*var.get()));
            cmd->prepared = false;
            return *var.get();
        }

        void disableFirstOptional();

        int returnSuccessCodes() const;

        std::string createArgumentWithType(const std::string &type) const;

        std::string successCodesCondition(const std::string &id, const std::string &indent = "      ") const;

        std::string successCodesList(const std::string &indent) const;

        bool isIndirect() const;

        Signature createSignature() const;

        std::string createProtoArguments(bool useOriginal = false, bool declaration = false) const;

        std::string createPFNArguments(bool useOriginal = false) const;

        std::string createPassArgumentsRAII() const;

        std::string createPassArguments(bool hasAllocVar) const;

        std::string createStaticPassArguments(bool hasAllocVar) const;

        std::vector<std::reference_wrapper<VariableData>> getFilteredProtoVars() const;

        std::string createArgument(const VariableData &var, bool useOrignal) const;

        bool returnsTemplate();

        void setGenInline(bool value);

        void addStdAllocators();

        std::string generateDeclaration();

        std::string generateDefinition(bool genInline, bool bypass = false);

        bool compareSignature(const MemberResolver &o) const;
    };

    class MemberResolverDefault : public MemberResolver
    {
      private:
        void transformMemberArguments();

      protected:
        std::string getSuperclassArgument(const String &superclass) const;

        std::string generateMemberBody() override;

      public:
        MemberResolverDefault(const Generator &gen, ClassCommand &d, MemberContext &ctx, bool constructor = {});

        virtual ~MemberResolverDefault();

        void generate(UnorderedFunctionOutput &decl, UnorderedFunctionOutput &def) override;
    };

    class MemberResolverDbg final : public MemberResolverDefault
    {
      public:
        MemberResolverDbg(const Generator &gen, ClassCommand &d, MemberContext &ctx) : MemberResolverDefault(gen, d, ctx) {
            dbgtag = "disabled";
        }

        void generate(UnorderedFunctionOutput &decl, UnorderedFunctionOutput &def) override {
            decl += "/*\n";
            decl += generateDeclaration();
            decl += "*/\n";
        }
    };

    class MemberResolverStaticDispatch final : public MemberResolver
    {
      public:
        MemberResolverStaticDispatch(const Generator &gen, ClassCommand &d, MemberContext &ctx);

        std::string temporary() const;

        std::string generateMemberBody() override;
    };

    class MemberResolverClearRAII final : public MemberResolver
    {
      public:
        MemberResolverClearRAII(const Generator &gen, ClassCommand &d, MemberContext &ctx);

        std::string temporary(const std::string &handle) const;

        std::string generateMemberBody() override;
    };

    class MemberResolverStaticVector final : public MemberResolverDefault
    {
      public:
        MemberResolverStaticVector(const Generator &gen, ClassCommand &d, MemberContext &ctx);
    };

    class MemberResolverPass final : public MemberResolver
    {
      public:
        MemberResolverPass(const Generator &gen, ClassCommand &d, MemberContext &ctx) : MemberResolver(gen, d, ctx) {
            ctx.disableSubstitution = true;
            dbgtag                  = "pass";
        }

        virtual std::string generateMemberBody() override {
            return "      " + generatePFNcall(true) + "\n";
        }
    };

    class MemberResolverVectorRAII final : public MemberResolverDefault
    {
      protected:
      public:
        MemberResolverVectorRAII(const Generator &gen, ClassCommand &d, MemberContext &refCtx);

        std::string generateMemberBody() override;
    };

    class MemberResolverCtor : public MemberResolverDefault
    {
      protected:
        String      _name;
        std::string src;
        bool        ownerInParent;

        struct SuperclassSource
        {
            std::string src;
            bool        isPointer  = {};
            bool        isIndirect = {};

            std::string getSuperclassAssignment() const {
                std::string out;
                if (isIndirect || !isPointer) {
                    out += "&";
                }
                out += src;
                return out;
            }

            std::string getDispatcher() const {
                std::string out = src;
                if (!isIndirect && isPointer) {
                    out += "->";
                } else {
                    out += ".";
                }
                out += "getDispatcher()";
                return out;
            }

        } superclassSource;

      public:
        MemberResolverCtor(const Generator &gen, ClassCommand &d, MemberContext &refCtx);

        SuperclassSource getSuperclassSource() const;

        std::string generateMemberBody() override;
    };

    class MemberResolverVectorCtor final : public MemberResolverCtor
    {
      public:
        MemberResolverVectorCtor(const Generator &gen, ClassCommand &d, MemberContext &refCtx);

        std::string generateMemberBody() override;
    };

    class MemberResolverUniqueCtor final : public MemberResolverDefault
    {
      public:
        MemberResolverUniqueCtor(Generator &gen, ClassCommand &d, MemberContext &refCtx);

        std::string generateMemberBody() override;
    };

    class MemberResolverCreateHandleRAII final : public MemberResolverDefault
    {
      public:
        MemberResolverCreateHandleRAII(Generator &gen, ClassCommand &d, MemberContext &refCtx);

        std::string generateMemberBody() override;
    };

    class MemberResolverCreate : public MemberResolverDefault
    {
      public:
        MemberResolverCreate(Generator &gen, ClassCommand &d, MemberContext &refCtx);

        std::string generateMemberBody() override;
    };

    class MemberResolverCreateUnique final : public MemberResolverCreate
    {
        bool isSubclass = {};

      public:
        MemberResolverCreateUnique(Generator &gen, ClassCommand &d, MemberContext &refCtx);

        std::string generateMemberBody() override;
    };

    class MemberGeneratorExperimental
    {
        const Generator          &gen;
        ClassCommand             &m;
        UnorderedFunctionOutputX &outputDecl;
        UnorderedFunctionOutputX &outputDef;
        UnorderedFunctionOutputX &outputTemplate;
        MemberContext             ctx{};

        template <typename T>
        void generate(T &res, std::vector<Protect> &protects) {
            auto protect = m.src->getProtect();
            if (!protect.empty()) {
                protects.emplace_back(std::string(protect), true);
            }
            // bool allowInline = true;
            // if (m.cls && m.cls->isSubclass) {
                // allowInline = false;
            // }

            std::string decl;
            std::string def;
//            if (res.isTemplated()) {
//                res.generateX(decl);
//            } else {
                res.generateX(decl, def);
//            }
            if (!def.empty()) {
                auto &dst = (res.isTemplated()? outputTemplate : outputDef).get(protects);
                dst += std::move(def);
            }
            if (!decl.empty()) {
                auto &dst = outputDecl.get(protects);
                dst += std::move(decl);
            }
        }

        void generatePass() {
            std::vector<Protect> protects;
            protects.emplace_back("VULKAN_HPP_EXPERIMENTAL_CSTYLE", false);
            MemberResolverPass resolver{ gen, m, ctx };
            generate(resolver, protects);
        }

        void generateDefault() {
            std::vector<Protect>  protects;
            MemberResolverDefault resolver{ gen, m, ctx };
            generate(resolver, protects);
        }

        void generateDestroyOverload(ClassCommand &m, MemberContext &ctx, const std::string &name) {
            generateDefault();

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
            auto last = m.src->getLastHandleVar();
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

      public:
        MemberGeneratorExperimental(const Generator &gen, ClassCommand &m, UnorderedFunctionOutputX &decl, UnorderedFunctionOutputX &def, UnorderedFunctionOutputX &templ)
          : gen(gen), m(m), outputDecl(decl), outputDef(def), outputTemplate(templ) {
            ctx.ns              = Namespace::VK;
            ctx.disableDispatch = true;
            ctx.exp             = true;
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

        void generate();
    };

    class MemberGenerator
    {
        Generator               &gen;
        vkr::ClassCommand       &m;
        MemberContext           &ctx;
        GenOutputClass          &out;
        UnorderedFunctionOutput &funcs;
        UnorderedFunctionOutput &funcsEnhanced;
        std::vector<Signature>   generated;

        enum class MemberGuard
        {
            NONE,
            UNIQUE
        };

        void generate(MemberResolver &resolver, MemberGuard g = MemberGuard::NONE, bool dbg = false) {
            const auto &sig = resolver.createSignature();
            for (const auto &g : generated) {
                if (g == sig) {
                    if (dbg) {
                        std::cerr << "generate(): signature conflict: " << sig.name << "(" << sig.args << ")" << '\n';
                    }
                    return;
                }
            }

            if (g != MemberGuard::NONE) {
                static const std::array<std::string, 2> guards{ "", "VULKAN_HPP_NO_SMART_HANDLE" };

                resolver.guard = guards[static_cast<int>(g)];
            } else {
                if (resolver.ctx.ns == Namespace::RAII) {
                    if (resolver.cmd->createsHandle()) {
                        resolver.guard = "VULKAN_HPP_EXPERIMENTAL_NO_RAII_CREATE_CMDS";
                    } else if (resolver._indirect || resolver._indirect2) {
                        if (resolver.cls->isSubclass) {
                            resolver.guard = "VULKAN_HPP_EXPERIMENTAL_NO_RAII_INDIRECT_SUB";
                        } else {
                            resolver.guard = "VULKAN_HPP_EXPERIMENTAL_NO_RAII_INDIRECT";
                        }
                    }
                }
            }
            if (generated.empty()) {
                resolver.generate(out.sFuncs, funcs);
            } else {
                resolver.generate(out.sFuncsEnhanced, funcsEnhanced);
            }

            generated.push_back(sig);
            // std::cout << "generated: " << resolver.dbgtag << '\n';
        }

        template <typename T>
        void generate(MemberGuard g = MemberGuard::NONE) {
            T resolver{ gen, m, ctx };
            generate(resolver, g);

            //            if (resolver->returnsVector()) {
            //                // std::cout << "> returns vector: " << ctx.name << '\n';
            //                resolver->addStdAllocators();
            //                gen(*resolver);
            //            }
        }

        void generateDestroyOverload(ClassCommand &m, MemberContext &ctx, const std::string &name) {
            generate<MemberResolverDefault>(MemberGuard::NONE);

            if (m.src->hasOverloadedDestroy()) {
                // std::cerr << "skip generate destroy overload: " << m.src->name.original << '\n';
                return;
            }

            const auto &type = m.cls->name;

            m.src->prepare();
#ifndef NDEBUG
            bool ok;
            m.src->check(ok);
#endif
            auto last = m.src->getLastHandleVar();
            if (!last) {
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
                std::cerr << "can't generate destroy: drop  " << m.cls->name << ": " << last->type() << "  " << m.src->name.original << '\n';
                return;
            }
            first->setOptional(false);

            second.name   = name;
            second.dbgtag = "d. overload";
            generate(second);
        }

      public:
        MemberGenerator(
          Generator &gen, ClassCommand &m, MemberContext &ctx, GenOutputClass &out, UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced)
          : gen(gen), m(m), ctx(ctx), out(out), funcs(funcs), funcsEnhanced(funcsEnhanced) {}

        void generate();
    };

}  // namespace vkgen

#endif  // GENERATOR_MEMBERS_HPP
