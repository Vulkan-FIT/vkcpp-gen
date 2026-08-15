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

#include "Generator.hpp"
#include "Registry.hpp"
#ifdef GENERATOR_GUI
#  include "Gui.hpp"
#endif

#ifndef USE_PCH
#include <optional>
#include <stdexcept>
#include <vector>
#include <memory>
#include <iostream>
#include <stdexcept>
#endif

// holds arguments data
struct ArgOption
{
    std::string shortName;
    std::string longName;
    bool                       requiredValue = false;  // if true parser loads value with next argument
    bool                       set           = false;  // set to true if argument exists
    std::string                value;

    ArgOption() = default;
    ArgOption(const std::string &shortName, const std::string &longName, bool required = false)
        : shortName(shortName), longName(longName), requiredValue(required)
    {}
};

// simple class made for parsing command line arguments
class ArgParser
{
    using Option = ArgOption;
    std::vector<std::unique_ptr<Option>> options;

public:
    // ArgParser(std::initializer_list<Option> list) : options(list) {}

    template <typename... Args>
    Option& add(Args &&...args) {
        return *options.emplace_back(std::make_unique<Option>(std::forward<Args>(args)...));
    }

    // parses arguments. throws if there are less arguments than expected
    inline void parse(int argc, char **argv) {
        const auto getArg = [&](int index) {  // tries to access argument at index, throws on fail
            if (argc <= index) {
                throw std::runtime_error("Arguments out of range. See usage.");
            }
            return std::string_view(argv[index]);
        };

        for (int i = 0; i < argc; ++i) {
            for (const auto &o : options) {
                std::string_view arg = getArg(i);                                 // fetch current argument
                if ((arg == o->shortName) ||  // compares short option
                    (arg == o->longName))      // comapres long option
                {
                    if (o->requiredValue) {
                        o->value = getArg(++i);  // try to fetch next argument
                    }
                    o->set = true;
                }
            }
        }
    }
};


static constexpr char const *HELP_TEXT{
    R"(Usage:
    -r, --reg       path to source registry file    
    -d, --dest      path to destination directory
    -c, --config    path to configuration file)"
};

int main(int argc, char **argv)
{
    using namespace vkgen;

    try {
        ArgParser p;
        const auto &helpOption = p.add("-h", "--help");
        const auto &regOption = p.add("-r", "--reg", true );
        const auto &destOption = p.add("-d", "--dest", true );
        const auto &configOption = p.add("-c", "--config", true );
        const auto &rlOption = p.add("", "--readlog", true );
        const auto &resaveOption = p.add("", "--resave-config" );
        const auto &verboseOption = p.add("", "--verbose-config" );
        const auto &noguiOption = p.add("", "--nogui" );
        const auto &guifpsOption = p.add("", "--fps" );
        const auto &extensionOption = p.add("", "--ext" );
        const auto &dbgtagOption = p.add("", "--debug" );
#ifdef GENERATOR_TOOL
        const auto &toolOption = p.add("", "--tool" );
        const auto &analyzeOption = p.add("", "--analyze", true );
#endif

        p.parse(argc, argv);
        // help option
        if (helpOption.set) {
            std::cout << HELP_TEXT;
            return 0;
        }
#ifdef INST
        if (rlOption.set) {
            Inst::readLog(rlOption.value);
            return 0;
        }
#endif
        Registry::loadRegistryPath();

        Generator gen;
        gen.load(regOption.value);
#ifdef GENERATOR_EXTENSION
        vkgen::tools::test(gen);
        return 0;
#endif

        if (destOption.set) {
            gen.setOutputPath(destOption.value);
        }
        if (dbgtagOption.set) {
            gen.cfg.dbg.methodTags.data = true;
        }
        if (configOption.set) {
            gen.loadConfigFile(configOption.value);
        }
        if (resaveOption.set) {
            if (!configOption.set) {
                throw std::runtime_error("Missing arguments. See usage.");
            }
            if (gen.isLoaded()) {
                gen.saveConfigFile(configOption.value, verboseOption.set);
            }
            return 0;
        }

#ifdef GENERATOR_TOOL
        if (analyzeOption.set) {
            if (!loadRegistry()) {
                return 1;
            }
            vkgen::tools::analyzeCode(gen, analyzeOption.value);
            return 0;
        }
#endif
#ifdef GENERATOR_GUI
        if (!noguiOption.set) {
            GUI gui{ gen };
            gui.init();
            if (guifpsOption.set) {
                gui.showFps = true;
            }
#  ifdef GENERATOR_TOOL
            if (toolOption.set) {
                gui.showToolScreen = true;
            }
#  endif
            if (configOption.set) {
                gui.setConfigPath(configOption.value);
            }
            gui.run();
            return 0;
        }
#endif
        // argument check
        if (!destOption.set) {
            throw std::runtime_error("Missing arguments. See usage.");
        }
        if (!gen.isLoaded()) {
            throw std::runtime_error("Registry is not loaded.");
        }
        gen.generate();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
