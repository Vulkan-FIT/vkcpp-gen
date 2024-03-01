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

#include "ArgumentsParser.hpp"
#include "Generator.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#ifdef GENERATOR_GUI
#    include "Gui.hpp"
#endif
#ifdef GENERATOR_EXTENSION
#include "tool/tool.hpp"
#endif

static constexpr char const *HELP_TEXT{
    R"(Usage:
    -r, --reg       path to source registry file    
    -d, --dest      path to destination directory
    -c, --config    path to configuration file)"
};

static bool loadDefaultRegistry(vkgen::Generator &gen) {
    std::string path = vkgen::Generator::findDefaultRegistryPath();
    if (path.empty()) {
        std::cerr << "Failed to detect vk.xml. See usage.\n";
        return false;
    }
    return gen.load(path);
}

int main(int argc, char **argv) {
    using namespace vkgen;

    try {
        ArgParser p;
        const auto &helpOption = p.add("-h", "--help");
        const auto &regOption = p.add("-r", "--reg", true );
        const auto &destOption = p.add("-d", "--dest", true );
        const auto &configOption = p.add("-c", "--config", true );
        const auto &rlOption = p.add("", "--readlog", true );
        const auto &resaveOption = p.add("", "--resave-config" );
        const auto &noguiOption = p.add("", "--nogui" );
        const auto &guifpsOption = p.add("", "--fps" );
        const auto &extensionOption = p.add("", "--ext" );
        const auto &dbgtagOption = p.add("", "--debug" );

        p.parse(argc, argv);
        // help option block
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

        Generator gen;

        const auto loadRegistry = [&] {
            if (regOption.set) {
                return gen.load(regOption.value);
            }
            return loadDefaultRegistry(gen);
        };
#ifdef GENERATOR_EXTENSION
        {
            if (loadRegistry()) {
                vkgen::tools::test(gen);
            }
            return 0;
        }
#endif
        const auto generate = [&] {
            // argument check
            if (!destOption.set) {
                throw std::runtime_error("Missing arguments. See usage.");
            }
            if (!loadRegistry()) {
                return;
            }
            if (configOption.set) {
                gen.loadConfigFile(configOption.value);
            }
            if (dbgtagOption.set) {
                gen.cfg.dbg.methodTags.data = true;
            }
            gen.generate();
        };

        if (destOption.set) {
            gen.setOutputFilePath(destOption.value);
        }
        if (resaveOption.set) {
            if (!configOption.set) {
                throw std::runtime_error("Missing arguments. See usage.");
            }
            if (loadRegistry()) {
                gen.loadConfigFile(configOption.value);
                gen.saveConfigFile(configOption.value);
            }
            return 0;
        }

#ifdef GENERATOR_GUI
        if (!noguiOption.set) {
            GUI gui{ gen };
            gui.init();
            if (guifpsOption.set) {
                gui.showFps = true;
            }
            if (configOption.set) {
                gui.setConfigPath(configOption.value);
            }
            loadRegistry();
            gui.run();
            return 0;
        }
#endif
        generate();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
