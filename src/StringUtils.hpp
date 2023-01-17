#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <algorithm>
#include <string>
#include <vector>

static bool caseInsensitivePredicate(char a, char b) {
    return std::toupper(a) == std::toupper(b);
}

// case insensitve search for substring
static bool strContains(const std::string &str, const std::string &substr) {
    auto it = std::search(str.begin(), str.end(), substr.begin(), substr.end(),
                          caseInsensitivePredicate);
    return it != str.end();
}

static void strStripPrefix(std::string &str, const std::string_view &prefix) {
    if (prefix.size() > str.size()) {
        return;
    }
    if (str.substr(0, prefix.size()) == prefix) {
        str.erase(0, prefix.size());
    }
}

static void strAddPrefix(std::string &str, const std::string_view &prefix) {
    if (!str.starts_with(prefix)) {
        str = prefix.data() + str;
    }
}

static void strStripSuffix(std::string &str, const std::string_view &suffix) {
    if (suffix.size() > str.size()) {
        return;
    }
    if (str.substr(str.size() - suffix.size(), std::string::npos) == suffix) {
        str.erase(str.size() - suffix.size(), std::string::npos);
    }
}

static void strStripVk(std::string &str) {
    strStripPrefix(str, "Vk");
    strStripPrefix(str, "vk");
}

static std::string strStripVk(const std::string &str) {
    std::string out = str;
    strStripVk(out);
    return out;
}

static std::string addVkPrefix(const std::string &str) {
    std::string out = str;
    strAddPrefix(out, "Vk");
    return out;
}

static std::string stripVkPrefix(const std::string &str) {
    std::string out = str;
    strStripVk(out);
    return out;
}

static std::string camelToSnake(const std::string &str) {
    std::string out;
    for (char c : str) {
        if (std::isupper(c) && !out.empty()) {
            out += '_';
        }
        out += std::toupper(c);
    }
    return out;
}

static std::string convertSnakeToCamel(const std::string &str) {
    std::string out;
    bool flag = false;
    for (char c : str) {
        if (c == '_') {
            flag = true;
            continue;
        }
        out += flag ? std::toupper(c) : std::tolower(c);
        flag = false;
    }
    return out;
}

static std::vector<std::string_view> split2(const std::string_view &str, const std::string_view &delim) {
    std::vector<std::string_view> out;
    auto start = 0U;
    auto end = str.find(delim);
    while (end != std::string::npos) {
        std::string_view token = str.substr(start, end - start);
        out.push_back(token);
        start = end + delim.size();
        end = str.find(delim, start);
    }
    std::string_view token = str.substr(start, end - start);
    out.push_back(token);
    return out;
}

static std::vector<std::string> split(const std::string &str, const std::string &delim) {
    std::vector<std::string> out;
    auto start = 0U;
    auto end = str.find(delim);
    while (end != std::string::npos) {
        std::string token = str.substr(start, end - start);
        out.push_back(token);
        start = end + delim.size();
        end = str.find(delim, start);
    }
    std::string token = str.substr(start, end - start);
    out.push_back(token);
    return out;
}

static std::string strFirstLower(const std::string_view &str) {
    std::string out = str.data();
    if (!out.empty()) {
        out[0] = std::tolower(out[0]);
    }
    return out;
}

static std::string strFirstUpper(const std::string_view &str) {
    std::string out = str.data();
    if (!out.empty()) {
        out[0] = std::toupper(out[0]);
    }
    return out;
}

static std::string toCppStyle(const std::string &str, bool firstCapital = false) {
    std::string out = stripVkPrefix(str);
    return firstCapital? strFirstUpper(out) : strFirstLower(out);
}

static std::string matchTypePointers(const std::string &from, const std::string &to) {
    int cfrom = std::count(from.begin(), from.end(), '*');
    int cto = std::count(to.begin(), to.end(), '*');
    if (cfrom > cto) {
        return "*";
    }
    if (cfrom < cto) {
        return "&";
    }
    return "";
}

class String : public std::string {

public:
    std::string original;
    String& operator=(const std::string &rhs) {
        std::string::assign(rhs);
        return *this;
    }
/*
    explicit operator const char*() const {
      return std::string::data();
    }
*/

    String(const std::string &src) {
        reset(src);
    }

    String(const std::string &src, bool firstCapital) {
        convert(src, firstCapital);
    }

    void reset(const std::string &src = "") {
        original = src;
        std::string::assign(src);
    }

    void convert(const std::string &src, bool firstCapital = false) {
        original = src;
        std::string::assign(toCppStyle(src, firstCapital));
    }
};

class Argument {
public:
    std::string type;
    std::string id;
    std::string assignment;

    Argument(const std::string &type, const std::string &id, const std::string &assignment = "") :
        type(type), id(id), assignment(assignment)
    {

    }

    operator std::string() const noexcept {
        return type + id + assignment;
    }

    bool empty() const {
        return type.empty() && id.empty();
    }

};

class InitializerBuilder {
    std::string init;
    std::string indent;

public:
    InitializerBuilder(const std::string &indent) : indent(indent) {}

    void append(const std::string &id, const std::string &assignment) {
        if (!init.empty()) {
            init += indent + ", ";
        }
        init += id + "(" + assignment + ")\n";
    }

    void appendRaw(const std::string &str) {
        init += str;
    }

    std::string string() const noexcept {
        if (init.empty()) {
            return "";
        }
        return "\n" + indent + ": " + init;
    }

};

class ArgumentBuilder {

    std::string str;
    std::string init;
    bool declaration;

public:
    std::vector<Argument> args;

    ArgumentBuilder(bool declaration) noexcept : declaration(declaration) {}

    void append(const std::string &type, const std::string &id, const std::string &assignment = "", const std::string &initId = "", bool ref = false) {
        if (!str.empty()) {
            str += ", ";
        }
        str += type + id;
        if (declaration) {
            str += assignment;
        }
        if (!initId.empty()) {
            if (!init.empty()) {
                init += ", ";
            }
            init += initId + "(" + (ref? "&" : "") + id + ")";
        }
        args.push_back(Argument(type, id, assignment));
    }

    void append(const Argument &arg, const std::string &initId = "", bool ref = false) {
        append(arg.type, arg.id, arg.assignment, initId, ref);
    }

    operator std::string() const noexcept {
        return str;
    }

    std::string string() const noexcept {
        return str;
    }

    std::string initializer() const noexcept {
        if (init.empty()) {
            return "";
        }
        return " : " + init;
    }

};

#endif // STRINGUTILS_HPP/
