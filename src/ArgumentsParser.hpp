// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//                                                           copies of the Software, and to permit persons to whom the Software is
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

#ifndef ARGUMENTSPARSER_H
#define ARGUMENTSPARSER_H

#include <optional>
#include <vector>
#include <stdexcept>

// holds arguments data
struct ArgOption {
    std::optional<std::string> shortName;
    std::optional<std::string> longName;
    bool requiredValue = false; // if true parser loads value with next argument
    bool set = false; // set to true if argument exists
    std::string value = {};
};

// simple class made for parsing command line arguments
class ArgParser {
    using Option = ArgOption;
    const std::vector<Option*> options;

public:
    ArgParser(std::initializer_list<Option*> list)
        : options(list)
    {}

    // parses arguments. throws if there are less arguments than expected
    inline void parse(int argc, char **argv) {

        const auto getArg = [&](int index) { // tries to access argument at index, throws on fail
            if (argc <= index) {
                throw std::runtime_error("Arguments out of range. See usage.");
            }
            return std::string_view(argv[index]);
        };

        for (int i = 0; i < argc; ++i) {
            for (Option *o : options) {
                std::string_view arg = getArg(i); // fetch current argument
                if ((o->shortName.has_value() && arg == o->shortName.value()) || // compares short name
                    (o->longName.has_value()  && arg == o->longName.value()))    // comapres long name
                {
                    if (o->requiredValue) {
                        o->value = getArg(++i); // try to fetch next argument
                    }
                    o->set = true;
                }
            }
        }
    }

};

#endif // ARGUMENTSPARSER_H
