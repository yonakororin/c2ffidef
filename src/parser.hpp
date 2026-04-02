#pragma once
#include "ast_nodes.hpp"
#include <string>
#include <vector>

struct ParseOptions {
    std::vector<std::string> extra_args;      // extra clang args (e.g. -I, -D)
    bool skip_system_includes = true;         // skip declarations from system headers
    bool main_file_only = false;              // if true, only emit decls from the top-level file
                                              // (ignore included user headers)
};

// Parse a C header file and return a TranslationUnit.
// Throws std::runtime_error on failure.
TranslationUnit parse_header(const std::string& filepath,
                             const ParseOptions& opts = {});
