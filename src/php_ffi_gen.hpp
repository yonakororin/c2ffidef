#pragma once
#include "generator.hpp"

// Generates PHP FFI::cdef() code.
//
// Output example:
//   $ffi = FFI::cdef(<<<'CDEF'
//   typedef struct { int x; int y; } Point;
//   int add(int a, int b);
//   CDEF, "libfoo.so");
class PhpFfiGenerator : public Generator {
public:
    explicit PhpFfiGenerator(const GeneratorOptions& opts);
    void generate(const TranslationUnit& tu, std::ostream& out) override;

private:
    std::string type_str(const CTypePtr& t, const std::string& name = "") const;
    void emit_record(const RecordDecl& r, std::ostream& out) const;
    void emit_enum(const EnumDecl& e, std::ostream& out) const;
    void emit_typedef(const TypedefDecl& td, std::ostream& out) const;
    void emit_function(const FunctionDecl& f, std::ostream& out) const;
};
