#pragma once
#include "ast_nodes.hpp"
#include <string>
#include <ostream>
#include <cctype>
#include <algorithm>

struct GeneratorOptions {
    std::string library_name;   // e.g. "libfoo.so" / "foo.dll"
    std::string indent = "    "; // 4 spaces
};

// Derive a safe identifier slug from a library name.
// "libfoo.so" → "foo", "libbar_baz.dylib" → "bar_baz", "mylib.dll" → "mylib"
inline std::string library_slug(const std::string& lib) {
    std::string s = lib;

    // Strip directory component
    auto slash = s.rfind('/');
    if (slash != std::string::npos) s = s.substr(slash + 1);

    // Strip known extensions
    for (const char* ext : {".so", ".dylib", ".dll"}) {
        std::string e(ext);
        if (s.size() > e.size() &&
            s.compare(s.size() - e.size(), e.size(), e) == 0)
        {
            s = s.substr(0, s.size() - e.size());
            break;
        }
    }

    // Strip "lib" prefix
    if (s.size() > 3 && s.substr(0, 3) == "lib") s = s.substr(3);

    // Replace non-alphanumeric characters with '_'
    for (char& c : s)
        if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';

    // Ensure it doesn't start with a digit
    if (!s.empty() && std::isdigit(static_cast<unsigned char>(s[0]))) s = "_" + s;

    return s.empty() ? "lib" : s;
}

class Generator {
public:
    explicit Generator(const GeneratorOptions& opts) : opts_(opts) {}
    virtual ~Generator() = default;

    // Generate code and write to stream.
    virtual void generate(const TranslationUnit& tu, std::ostream& out) = 0;

protected:
    GeneratorOptions opts_;
};
