#pragma once
#include <string>
#include <vector>

struct ValidationResult {
    enum class Status { OK, FAILED, SKIPPED };

    Status      status  = Status::SKIPPED;
    std::string target;   // "PHP FFI (C syntax)" / "Python ctypes"
    std::string tool;     // tool used for validation (e.g. "cc", "python3")
    std::string output;   // compiler/interpreter output (empty on success)
};

// Validate PHP FFI output.
// Extracts the embedded C declarations and checks them with `cc -fsyntax-only`.
ValidationResult validate_php(const std::string& php_code);

// Validate Python ctypes output.
// Runs the generated code with `python3` after replacing CDLL with a mock object.
ValidationResult validate_python(const std::string& python_code);

// Pretty-print results to stderr.  Returns true if all results passed.
bool print_validation_results(const std::vector<ValidationResult>& results);
