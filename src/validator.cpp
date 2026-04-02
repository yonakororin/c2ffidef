#include "validator.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

// ============================================================
// Internal helpers
// ============================================================

// Run a shell command and capture its combined stdout+stderr.
// Returns the process exit status (0 = success).
static int run_command(const std::string& cmd, std::string& output) {
    output.clear();
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) {
        output = "(popen failed)";
        return -1;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    return pclose(pipe);
}

// Find a tool in PATH; returns true and sets `path` if found.
static bool find_tool(const std::string& name, std::string& path) {
    std::string out;
    int rc = run_command("which " + name, out);
    if (rc != 0 || out.empty()) return false;
    // trim whitespace
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
        out.pop_back();
    path = out;
    return !path.empty();
}

// RAII temp file: creates file with given content and deletes it on destruction.
class TempFile {
public:
    TempFile(const std::string& suffix, const std::string& content) {
        std::string tmpl = "/tmp/c2ffidef_XXXXXX" + suffix;
        buf_.assign(tmpl.begin(), tmpl.end());
        buf_.push_back('\0');
        int fd = mkstemps(buf_.data(), static_cast<int>(suffix.size()));
        if (fd < 0) throw std::runtime_error("mkstemps failed");
        if (!content.empty()) {
            size_t written = write(fd, content.c_str(), content.size());
            (void)written;
        }
        close(fd);
        path_ = buf_.data();
    }
    ~TempFile() { unlink(path_.c_str()); }

    const std::string& path() const { return path_; }

private:
    std::vector<char> buf_;
    std::string       path_;
};

// ============================================================
// PHP FFI validation
// ============================================================

// Extract the raw C declarations from between <<<'CDEF'\n ... \nCDEF,
static std::string extract_cdef(const std::string& php_code) {
    const std::string BEGIN = "<<<'CDEF'\n";
    const std::string END   = "\nCDEF,";

    auto start = php_code.find(BEGIN);
    if (start == std::string::npos) return "";
    start += BEGIN.size();

    auto end = php_code.find(END, start);
    if (end == std::string::npos) return "";

    return php_code.substr(start, end - start);
}

ValidationResult validate_php(const std::string& php_code) {
    ValidationResult result;
    result.target = "PHP FFI (C syntax)";

    std::string cdef = extract_cdef(php_code);
    if (cdef.empty()) {
        result.status  = ValidationResult::Status::FAILED;
        result.output  = "Could not extract CDEF block from generated PHP code.";
        return result;
    }

    // Find a C compiler
    std::string cc_path;
    for (const char* candidate : {"cc", "gcc", "clang"}) {
        if (find_tool(candidate, cc_path)) break;
    }
    if (cc_path.empty()) {
        result.status = ValidationResult::Status::SKIPPED;
        result.output = "No C compiler (cc/gcc/clang) found in PATH.";
        return result;
    }
    result.tool = cc_path;

    // Write C declarations to a temp file.
    // Wrap in an extern block so it is valid as a .c translation unit.
    std::string c_src = "/* c2ffidef: PHP FFI cdef validation */\n" + cdef + "\n";
    TempFile tmp(".c", c_src);

    std::string cmd = cc_path + " -fsyntax-only -x c -std=c11 -w " + tmp.path();
    std::string out;
    int rc = run_command(cmd, out);

    if (rc == 0) {
        result.status = ValidationResult::Status::OK;
    } else {
        result.status = ValidationResult::Status::FAILED;
        result.output = out;
    }
    return result;
}

// ============================================================
// Python ctypes validation
// ============================================================

// Build a Python script that replaces the CDLL call with a mock object,
// so the structure/type definitions can be validated without the actual library.
static std::string build_python_validation_script(const std::string& python_code) {
    // Mock library: silently accepts any attribute access and assignment.
    const std::string MOCK = R"(
# ── validation mock: replaces ctypes.CDLL ──────────────────────────────────
class _LibMock:
    """Accepts _lib.func.argtypes / .restype assignments without a real .so"""
    class _FuncMock:
        argtypes = []
        restype  = None
    def __getattr__(self, _name):
        return _LibMock._FuncMock()
# ───────────────────────────────────────────────────────────────────────────
)";

    std::string script;
    bool injected = false;

    std::istringstream ss(python_code);
    std::string line;
    while (std::getline(ss, line)) {
        // Replace:  _lib = ctypes.CDLL("...")
        if (!injected && line.find("_lib = ctypes.CDLL(") != std::string::npos) {
            script += MOCK;
            script += "_lib = _LibMock()\n";
            injected = true;
            continue;
        }
        script += line + "\n";
    }

    // If we never found the CDLL line, prepend the mock anyway
    if (!injected) {
        script = MOCK + "_lib = _LibMock()\n" + script;
    }

    return script;
}

ValidationResult validate_python(const std::string& python_code) {
    ValidationResult result;
    result.target = "Python ctypes";

    std::string py_path;
    for (const char* candidate : {"python3", "python"}) {
        if (find_tool(candidate, py_path)) break;
    }
    if (py_path.empty()) {
        result.status = ValidationResult::Status::SKIPPED;
        result.output = "No Python interpreter (python3/python) found in PATH.";
        return result;
    }
    result.tool = py_path;

    std::string script = build_python_validation_script(python_code);
    TempFile tmp(".py", script);

    std::string cmd = py_path + " " + tmp.path();
    std::string out;
    int rc = run_command(cmd, out);

    if (rc == 0) {
        result.status = ValidationResult::Status::OK;
    } else {
        result.status = ValidationResult::Status::FAILED;
        result.output = out;
    }
    return result;
}

// ============================================================
// Pretty-print
// ============================================================

bool print_validation_results(const std::vector<ValidationResult>& results) {
    bool all_ok = true;
    fprintf(stderr, "\n");
    for (const auto& r : results) {
        switch (r.status) {
        case ValidationResult::Status::OK:
            fprintf(stderr, "[PASS] %s  (tool: %s)\n",
                    r.target.c_str(), r.tool.c_str());
            break;
        case ValidationResult::Status::FAILED:
            fprintf(stderr, "[FAIL] %s  (tool: %s)\n",
                    r.target.c_str(), r.tool.c_str());
            if (!r.output.empty()) {
                // indent each line of output
                std::istringstream ss(r.output);
                std::string line;
                while (std::getline(ss, line))
                    fprintf(stderr, "       %s\n", line.c_str());
            }
            all_ok = false;
            break;
        case ValidationResult::Status::SKIPPED:
            fprintf(stderr, "[SKIP] %s  (%s)\n",
                    r.target.c_str(), r.output.c_str());
            break;
        }
    }
    return all_ok;
}
