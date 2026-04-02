#pragma once
#include "generator.hpp"

// Generates Python ctypes bindings.
//
// Output example:
//   import ctypes
//   import ctypes.util
//
//   _lib = ctypes.CDLL("libfoo.so")
//
//   class Point(ctypes.Structure):
//       _fields_ = [
//           ("x", ctypes.c_int),
//           ("y", ctypes.c_int),
//       ]
//
//   _lib.add.argtypes = [ctypes.c_int, ctypes.c_int]
//   _lib.add.restype  = ctypes.c_int
class CtypesGenerator : public Generator {
public:
    explicit CtypesGenerator(const GeneratorOptions& opts);
    void generate(const TranslationUnit& tu, std::ostream& out) override;

private:
    // Returns the ctypes type expression string.
    // forward_decls: names of structs already declared but not yet defined
    //                (used to detect forward-reference pointers).
    std::string ctypes_type(const CTypePtr& t) const;

    void emit_record(const RecordDecl& r, std::ostream& out) const;
    void emit_enum(const EnumDecl& e, std::ostream& out) const;
    void emit_typedef(const TypedefDecl& td, std::ostream& out) const;
    void emit_function(const FunctionDecl& f, std::ostream& out) const;
};
