#pragma once
#include "ast_nodes.hpp"
#include <string>
#include <ostream>

struct GeneratorOptions {
    std::string library_name;   // e.g. "libfoo.so" / "foo.dll"
    std::string indent = "    "; // 4 spaces
};

class Generator {
public:
    explicit Generator(const GeneratorOptions& opts) : opts_(opts) {}
    virtual ~Generator() = default;

    // Generate code and write to stream.
    virtual void generate(const TranslationUnit& tu, std::ostream& out) = 0;

protected:
    GeneratorOptions opts_;
};
