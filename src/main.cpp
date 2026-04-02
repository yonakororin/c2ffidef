#include "parser.hpp"
#include "php_ffi_gen.hpp"
#include "ctypes_gen.hpp"
#include "validator.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

static void print_usage(const char* argv0) {
    std::cerr <<
        "Usage: " << argv0 << " [OPTIONS] <header.h>\n"
        "\n"
        "Options:\n"
        "  -l <library>   Shared library name (default: libfoo.so)\n"
        "  -o <file>      Output file (default: stdout)\n"
        "  -t <target>    Target language: php | python | all (default: all)\n"
        "  -I <dir>       Add include directory (can be repeated)\n"
        "  -D <macro>     Define preprocessor macro (can be repeated)\n"
        "  --no-system    Skip declarations from system headers (default: on)\n"
        "  --main-only    Only emit declarations defined in <header.h> itself;\n"
        "                 ignore included user headers (default: off)\n"
        "  --lang <lang>  Language mode: auto | c | c++ (default: auto)\n"
        "                 auto = c++ for .hpp/.hh/.hxx, c for .h\n"
        "  --validate     Validate the generated code with cc / python3.\n"
        "                 Results are printed to stderr; exit code 2 on failure.\n"
        "  -h, --help     Show this help\n"
        "\n"
        "Examples:\n"
        "  " << argv0 << " -t python tests/test.h\n"
        "  " << argv0 << " -t php --validate -o ffi.php mylib.h\n"
        "  " << argv0 << " -I/usr/local/include -l libbar.so --validate mylib.h\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string library  = "libfoo.so";
    std::string outfile;
    std::string target   = "all";
    std::string header;
    bool        do_validate = false;
    ParseOptions parse_opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-l" && i + 1 < argc) {
            library = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            outfile = argv[++i];
        } else if (arg == "-t" && i + 1 < argc) {
            target = argv[++i];
            if (target != "php" && target != "python" && target != "all") {
                std::cerr << "Error: unknown target '" << target
                          << "'. Use php, python, or all.\n";
                return 1;
            }
        } else if (arg.size() > 2 && arg.substr(0, 2) == "-I") {
            parse_opts.extra_args.push_back(arg);
        } else if (arg == "-I" && i + 1 < argc) {
            parse_opts.extra_args.push_back("-I" + std::string(argv[++i]));
        } else if (arg.size() > 2 && arg.substr(0, 2) == "-D") {
            parse_opts.extra_args.push_back(arg);
        } else if (arg == "-D" && i + 1 < argc) {
            parse_opts.extra_args.push_back("-D" + std::string(argv[++i]));
        } else if (arg == "--no-system") {
            parse_opts.skip_system_includes = true;
        } else if (arg == "--main-only") {
            parse_opts.main_file_only = true;
        } else if (arg == "--lang" && i + 1 < argc) {
            std::string lang = argv[++i];
            if (lang != "auto" && lang != "c" && lang != "c++") {
                std::cerr << "Error: --lang must be auto, c, or c++\n";
                return 1;
            }
            parse_opts.language = lang;
        } else if (arg == "--validate") {
            do_validate = true;
        } else if (arg[0] != '-') {
            if (!header.empty()) {
                std::cerr << "Error: multiple header files not supported.\n";
                return 1;
            }
            header = arg;
        } else {
            std::cerr << "Warning: unknown option '" << arg << "'\n";
        }
    }

    if (header.empty()) {
        std::cerr << "Error: no header file specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    // Parse
    TranslationUnit tu;
    try {
        tu = parse_header(header, parse_opts);
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    }

    GeneratorOptions gen_opts;
    gen_opts.library_name = library;

    // Generate into string buffers (needed for both output and validation)
    std::string php_code, python_code;
    try {
        if (target == "php" || target == "all") {
            std::ostringstream buf;
            PhpFfiGenerator gen(gen_opts);
            gen.generate(tu, buf);
            php_code = buf.str();
        }
        if (target == "python" || target == "all") {
            std::ostringstream buf;
            CtypesGenerator gen(gen_opts);
            gen.generate(tu, buf);
            python_code = buf.str();
        }
    } catch (const std::exception& e) {
        std::cerr << "Generation error: " << e.what() << "\n";
        return 1;
    }

    // Write output
    std::ofstream file_out;
    std::ostream* out = &std::cout;
    if (!outfile.empty()) {
        file_out.open(outfile);
        if (!file_out) {
            std::cerr << "Error: cannot open output file: " << outfile << "\n";
            return 1;
        }
        out = &file_out;
    }

    if (target == "php") {
        *out << php_code;
    } else if (target == "python") {
        *out << python_code;
    } else {
        *out << "# ===================== PHP FFI =====================\n\n";
        *out << php_code;
        *out << "\n\n# =================== Python ctypes =================\n\n";
        *out << python_code;
    }

    // Validate
    if (!do_validate) return 0;

    std::vector<ValidationResult> results;
    if (!php_code.empty())    results.push_back(validate_php(php_code));
    if (!python_code.empty()) results.push_back(validate_python(python_code));

    bool all_ok = print_validation_results(results);
    return all_ok ? 0 : 2;
}
