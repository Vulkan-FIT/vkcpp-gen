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

#include "ArgumentsParser.hpp"
#include "Generator.hpp"
#ifdef GENERATOR_GUI
#include "Gui.hpp"
#endif

static constexpr char const *HELP_TEXT{
    R"(Usage:
    -r, --reg       path to source registry file
    -s, --source    path to source directory
    -d, --dest      path to destination file)"};

int main(int argc, char **argv) {    
    try {
        ArgOption helpOption{"-h", "--help"};
        ArgOption xmlOption{"-r", "--reg", true};
        ArgOption destOption{"-d", "--dest", true};
        ArgParser p({&helpOption, &xmlOption, &destOption});

        p.parse(argc, argv);
        // help option block
        if (helpOption.set) {
            std::cout << HELP_TEXT;
            return 0;
        }

        Generator gen;        

        if (destOption.set) {
            gen.setOutputFilePath(destOption.value);
        }
        if (xmlOption.set) {
            gen.load(xmlOption.value);
        }

#ifdef GENERATOR_GUI
        GUI gui{gen};
        gui.init();
        gui.run();
#else
        // argument check
        if (!destOption.set || !xmlOption.set) {
            throw std::runtime_error("Missing arguments. See usage.");
        }
        gen.generate();
#endif

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
