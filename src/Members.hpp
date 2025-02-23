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
        bool      removeSuperclassVar        = {};
        bool      addVectorAllocator         = {};
        bool      commentOut                 = {};
        bool      templateVector             = {};
        bool      staticVector               = {};
        bool      structureChain             = {};
        bool      globalModeStatic           = {};
        bool      globalUseCAPI              = {};
        bool      exp                        = {};
        bool      suffixNoThrow              = {};
        bool      suffixThrow                = {};
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
        std::string                                structChainType;
        std::string                                structChainIdentifier;
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
        bool          isNothrow            = true;
        bool          enhanced             = true;
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

        std::string createReturnType();

        std::string generateNodiscard();

        std::string getSpecifiers(bool decl);

        std::string getProto(const std::string &indent, const std::string &prefix, const std::string &name, bool declaration, bool &usesTemplate);

        std::string getDbgtag(const std::string &prefix, bool bypass = false);

        std::string createProtoArguments(bool declaration = false);

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

        void generate(GuardedOutput &decl, GuardedOutputFuncs &def, const std::span<Protect> protects = {});

        template <typename... Args>
        VariableData &addVar(decltype(cmd->params)::iterator pos, Args &&...args) {
            auto &var = tempVars.emplace_back(std::make_unique<VariableData>(std::forward<Args>(args)...));

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

        std::string createAliasArguments() const;

        std::string createStaticPassArguments(bool hasAllocVar) const;

        std::vector<std::reference_wrapper<VariableData>> getFilteredProtoVars() const;

        std::string createArgument(const VariableData &var, bool useOrignal) const;

        bool returnsTemplate() const;

        bool returnsVkVector() const;

        bool hasVkParam() const;

        bool hasVkProxy() const;

        bool hasParam() const;

        void setGenInline(bool value);

        void addStdAllocators();

        std::string generateDeclaration();

        std::string generateDefinition(bool genInline, bool bypass = false);

        bool compareSignature(const MemberResolver &o) const;
    };

    class MemberResolverDefault : public MemberResolver
    {
      private:

        void transformVoidType(VariableData &var);
        bool transformToArray(VariableData &var);
        void transformToArrayIn(VariableData &var);
        void transformToArrayOut(VariableData &var);

        void transformMemberArguments();


      protected:
        std::string getSuperclassArgument(const String &superclass) const;

        VariableData *generateVariables(std::string &output, std::string &returnId, bool &returnsRAII, bool dbg = false);

        void generateMemberBodyArray(std::string &output, std::string &returnId, bool &returnsRAII, VariableData *vectorSizeVar, bool dbg = false);

        std::string generateMemberBody() override;

        // bool returnsResult();

      public:
        MemberResolverDefault(const Generator &gen, ClassCommand &d, MemberContext &ctx, bool constructor = {});

        virtual ~MemberResolverDefault();
    };

    class MemberResolverStaticDispatch final : public MemberResolver
    {
      public:
        MemberResolverStaticDispatch(const Generator &gen, ClassCommand &d, MemberContext &ctx);

        std::string temporary() const;

        std::string generateMemberBody() override;
    };

    class MemberResolverDestroy final : public MemberResolverDefault

    {
      public:
        MemberResolverDestroy(const Generator &gen, ClassCommand &d, MemberContext &ctx);

        // std::string generateMemberBody() override;
    };

    class MemberResolverClearRAII final : public MemberResolver
    {
      public:
        MemberResolverClearRAII(const Generator &gen, ClassCommand &d, MemberContext &ctx);

        std::string temporary(const std::string &handle) const;

        std::string generateMemberBody() override;
    };

//    class MemberResolverStaticVector final : public MemberResolverDefault
//    {
//      public:
//        MemberResolverStaticVector(const Generator &gen, ClassCommand &d, MemberContext &ctx);
//    };

    class MemberResolverPass final : public MemberResolver
    {
      public:
        MemberResolverPass(const Generator &gen, ClassCommand &d, MemberContext &ctx) : MemberResolver(gen, d, ctx) {
            ctx.disableSubstitution = true;
            enhanced                = false;
            dbgtag                  = "pass";
        }

        virtual std::string generateMemberBody() override {
            return "      " + generatePFNcall(true) + "\n";
        }
    };

    class MemberResolverPassReference final : public MemberResolver
    {
      public:
        MemberResolverPassReference(const Generator &gen, ClassCommand &d, MemberContext &ctx) : MemberResolver(gen, d, ctx) {
            ctx.disableSubstitution = true;
            enhanced                = false;
            dbgtag                  = "pass ref";

            for (VariableData &p : cmd->params) {
                if (p.isPointer() && !p.isArray() && p.original.type() != "void" && p.original.type() != "VkAllocationCallbacks") {
                    p.convertToReference();
                }
            }

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
        // String      _name;
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

    class MemberResolverInit : public MemberResolverCtor
    {
      public:
        MemberResolverInit(const Generator &gen, ClassCommand &d, MemberContext &refCtx);

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
        MemberResolverCreate(const Generator &gen, ClassCommand &d, MemberContext &refCtx);

        std::string generateMemberBody() override;
    };

    class MemberResolverCreateUnique final : public MemberResolverCreate
    {
        std::string poolSource;
        std::string createSource;
        std::unique_ptr<VariableData> uniqueVector;
        bool isSubclass = {};

      public:
        MemberResolverCreateUnique(const Generator &gen, ClassCommand &d, MemberContext &refCtx);

        std::string generateMemberBody() override;
    };

    class MemberGenerator
    {
        const Generator              &gen;
        ClassCommand                 &m;
        GuardedOutput                &decl;
        GuardedOutputFuncs           &out;
        bool noThrowGenerated = false;
        bool throwGenerated = false;
        std::string dbg;
      public:
        MemberContext                 ctx;
        bool isGlobalModeStatic = {};
      private:
        template <typename T>
        void generate(T &resolver, const std::span<Protect> protects = {}) {
//            if (ctx.globalModeStatic) {
//                bool p = resolver.hasVkParam();
//                if (ctx.globalUseCAPI && ((!p && !resolver.returnsVkVector()) || !resolver.hasParam())) {
//                    return;
//                }
//            }
            resolver.generate(out.decl, out, protects);
            if (resolver.isNothrow) {
                noThrowGenerated = true;
            }
            else {
                throwGenerated = true;
            }
        }

        template <typename T>
        void generate(const std::span<Protect> protects = {}) {
            T resolver{ gen, m, ctx };
            generate(resolver, protects);
        }

        void generatePass() {
            // protects.emplace_back("VULKAN_HPP_EXPERIMENTAL_CSTYLE", false);
            generate<MemberResolverPass>();

            if (m.src->flags & Command::CommandFlags::REFERENCE_PARAM) {
                generate<MemberResolverPassReference>();
            }
        }

        void generateStructChain();

        void generateDefault();

        void generateCreate();

        void generateDestroy(ClassCommand &m, MemberContext &ctx, const std::string &name);

      public:
        MemberGenerator(const Generator &gen, ClassCommand &m, GuardedOutput &decl, GuardedOutputFuncs &out, bool isStatic = false);

        void generate();
    };

} // namespace vkgen

#endif  // GENERATOR_MEMBERS_HPP
