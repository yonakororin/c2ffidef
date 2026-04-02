#pragma once
#include "ast_nodes.hpp"
#include <string>
#include <vector>

struct ParseOptions {
    std::vector<std::string> extra_args;      // extra clang args (e.g. -I, -D)
    bool skip_system_includes = true;         // skip declarations from system headers
    bool main_file_only = false;              // if true, only emit decls from the top-level file
                                              // (ignore included user headers)
    // Language mode.
    // "auto"  : use C++ for .hpp/.hh/.cxx/.cpp, C for everything else (default)
    // "c"     : always parse as C
    // "c++"   : always parse as C++
    std::string language = "auto";

    // If true, restrict *function* collection to the top-level file only.
    // Type definitions (struct/enum/typedef) are still collected from all
    // user headers so that referenced types are available in the output.
    // Automatically set to true when parsing .c / .cpp source files.
    bool functions_main_file_only = false;
};

// Parse a C header file and return a TranslationUnit.
// Throws std::runtime_error on failure.
TranslationUnit parse_header(const std::string& filepath,
                             const ParseOptions& opts = {});
