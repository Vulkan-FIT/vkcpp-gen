#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <string>

static bool caseInsensitivePredicate(char a, char b) {
    return std::toupper(a) == std::toupper(b);
}

// case insensitve search for substring
static bool strContains(const std::string &string,
                        const std::string &substring) {
    auto it = std::search(string.begin(), string.end(), substring.begin(),
                          substring.end(), caseInsensitivePredicate);
    return it != string.end();
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

#endif // STRINGUTILS_HPP
