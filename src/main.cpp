/*
MIT License

Copyright (c) 2021 guritchi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <algorithm>
#include <functional>
#include <iostream>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <cstdlib>

#include "ArgumentsParser.hpp"
#include "Generator.hpp"
#include "Instrumentation.h"

#ifdef GENERATOR_GUI
#include "Gui.hpp"
#endif

static constexpr char const *HELP_TEXT{
    R"(Usage:
    -r, --reg       path to source registry file    
    -d, --dest      path to destination directory
    -c, --config    path to configuration file)"};

static void loadDefaultRegistry(Generator &gen) {
    std::string path = Generator::findDefaultRegistryPath();
    if (path.empty()) {
        throw std::runtime_error("Failed to detect vk.xml. See usage.");
    }
    gen.load(path);
}

int main(int argc, char **argv) {
    try {
        ArgOption helpOption{"-h", "--help"};
        ArgOption regOption{"-r", "--reg", true};
        ArgOption destOption{"-d", "--dest", true};
        ArgOption configOption{"-c", "--config", true};
        ArgOption rlOption{"", "--readlog", true};
        ArgOption resaveOption{"", "--resave-config"};
        ArgOption noguiOption{"", "--nogui"};
        ArgOption guifpsOption{"", "--fps"};
        ArgParser p({&helpOption, &regOption, &destOption, &configOption, &noguiOption, &guifpsOption, &rlOption, &resaveOption});

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

        const auto loadRegistry = [&]{
            if (regOption.set) {
                gen.load(regOption.value);
            }
            else {
               loadDefaultRegistry(gen);
            }            
        };

        const auto generate = [&]{
            // argument check
            if (!destOption.set) {
                throw std::runtime_error("Missing arguments. See usage.");
            }
            loadRegistry();
            if (configOption.set) {
                gen.loadConfigFile(configOption.value);
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
            loadRegistry();
            gen.loadConfigFile(configOption.value);
            gen.saveConfigFile(configOption.value);
            return 0;
        }

#ifdef GENERATOR_GUI
        if (!noguiOption.set) {
            GUI gui{gen};
            gui.init();
            if (guifpsOption.set) {
               gui.showFps = true;
            }
            if (configOption.set) {
                gui.setConfigPath(configOption.value);
            }
            try {
                loadRegistry();
            }
            catch (std::runtime_error) {
                std::cout << "warning: registry load failed" << std::endl;
                gen.unload();
            }            
            gui.run();
            return 0;
        }
#endif
        generate();

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
