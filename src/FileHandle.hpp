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
#ifndef FILEHANDLE_HPP
#define FILEHANDLE_HPP

#include <fstream>
#include <queue>
#include <stdexcept>

static constexpr char const* INDENT {"    "};
static constexpr char const* ENDL   {"\n"};

// file class wrapper, handles identation in stateful manner
class FileHandle {
    std::ofstream file;
    int indent; // current ident level
    std::queue<int> queue; // history of indent changes

public:
    FileHandle() {
        indent = 0;
    }
    // tries to open file, throws if fails
    void open(const std::string &path) {
        close();
        file.open(path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Can't open file: " + path);
        }
    }

    void close() {
        if (file.is_open()) {
            file.close();
        }
    }

    std::ofstream& get() {
        return file;
    }
    // adds indent
    void pushIndent(int n = 1) {
        queue.push(n);
        indent += n;
    }
    // removes indent
    void popIndent() {
        if (!queue.empty()) {
            int n = queue.front();
            queue.pop();
            indent -= n;
        }
    }

    void writeLine(const std::string &s) {
        for (int i = 0; i < indent; ++i) {
            file << INDENT;
        }
        file << s << ENDL;
    }

};

#endif // FILEHANDLE_HPP
