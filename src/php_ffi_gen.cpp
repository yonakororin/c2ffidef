#include "php_ffi_gen.hpp"
#include <ostream>
#include <sstream>
#include <functional>
#include <unordered_set>
#include <stdexcept>

// ============================================================
// Type → C declaration string (used inside cdef)
// PHP FFI accepts standard C syntax in cdef(), so we reconstruct it.
// ============================================================

static std::string primitive_c_str(TypeKind k) {
    switch (k) {
    case TypeKind::Void:       return "void";
    case TypeKind::Bool:       return "_Bool";
    case TypeKind::Char:       return "char";
    case TypeKind::SChar:      return "signed char";
    case TypeKind::UChar:      return "unsigned char";
    case TypeKind::Short:      return "short";
    case TypeKind::UShort:     return "unsigned short";
    case TypeKind::Int:        return "int";
    case TypeKind::UInt:       return "unsigned int";
    case TypeKind::Long:       return "long";
    case TypeKind::ULong:      return "unsigned long";
    case TypeKind::LongLong:   return "long long";
    case TypeKind::ULongLong:  return "unsigned long long";
    case TypeKind::Int128:     return "__int128";
    case TypeKind::UInt128:    return "unsigned __int128";
    case TypeKind::Float:      return "float";
    case TypeKind::Double:     return "double";
    case TypeKind::LongDouble: return "long double";
    case TypeKind::Float128:   return "__float128";
    default:                   return "/* unknown */";
    }
}

// Returns the C declaration for the type, with optional declarator name.
// For pointer/array types the declarator name is embedded.
std::string PhpFfiGenerator::type_str(const CTypePtr& t, const std::string& name) const {
    if (!t) return "void" + (name.empty() ? "" : " " + name);

    std::string cv;
    if (t->is_const)    cv += "const ";
    if (t->is_volatile) cv += "volatile ";

    switch (t->kind) {
    case TypeKind::Void:
    case TypeKind::Bool:
    case TypeKind::Char:
    case TypeKind::SChar:
    case TypeKind::UChar:
    case TypeKind::Short:
    case TypeKind::UShort:
    case TypeKind::Int:
    case TypeKind::UInt:
    case TypeKind::Long:
    case TypeKind::ULong:
    case TypeKind::LongLong:
    case TypeKind::ULongLong:
    case TypeKind::Int128:
    case TypeKind::UInt128:
    case TypeKind::Float:
    case TypeKind::Double:
    case TypeKind::LongDouble:
    case TypeKind::Float128: {
        std::string s = cv + primitive_c_str(t->kind);
        return name.empty() ? s : s + " " + name;
    }

    case TypeKind::Record: {
        // Already has struct/union keyword embedded in clang type name?
        // We'll add the keyword ourselves.
        std::string s = cv + t->name;
        return name.empty() ? s : s + " " + name;
    }

    case TypeKind::Enum: {
        std::string s = cv + t->name;
        return name.empty() ? s : s + " " + name;
    }

    case TypeKind::Typedef: {
        std::string s = cv + t->name;
        return name.empty() ? s : s + " " + name;
    }

    case TypeKind::Pointer: {
        if (!t->pointee) {
            std::string s = cv + "void *";
            return name.empty() ? s : s + name;
        }
        if (t->pointee->kind == TypeKind::FunctionProto) {
            // function pointer: return_type (*name)(params)
            const auto& fp = t->pointee;
            std::string ret = type_str(fp->fn_return);
            std::string params;
            for (size_t i = 0; i < fp->fn_params.size(); ++i) {
                if (i) params += ", ";
                params += type_str(fp->fn_params[i]);
            }
            std::string decl_name = name.empty() ? "" : "(*" + name + ")";
            return ret + " " + decl_name + "(" + params + ")";
        }
        // Regular pointer
        std::string inner = type_str(t->pointee, "*" + name);
        return cv + inner;
    }

    case TypeKind::ConstantArray: {
        std::string suffix = "[" + std::to_string(t->array_size) + "]";
        return type_str(t->pointee, name + suffix);
    }

    case TypeKind::IncompleteArray: {
        return type_str(t->pointee, name + "[]");
    }

    case TypeKind::FunctionProto: {
        // bare function type (rare, mostly used inside pointer context)
        std::string ret = type_str(t->fn_return);
        std::string params;
        for (size_t i = 0; i < t->fn_params.size(); ++i) {
            if (i) params += ", ";
            params += type_str(t->fn_params[i]);
        }
        return ret + " " + name + "(" + params + ")";
    }

    case TypeKind::Unexposed: {
        std::string s = cv + t->name;
        return name.empty() ? s : s + " " + name;
    }

    default:
        return "/* unsupported */ void" + (name.empty() ? "" : " " + name);
    }
}

// ============================================================
// Emit helpers
// ============================================================

void PhpFfiGenerator::emit_record(const RecordDecl& r, std::ostream& out) const {
    const std::string& ind = opts_.indent;
    if (r.is_forward_decl) {
        out << (r.is_union ? "union " : "struct ") << r.name << ";\n";
        return;
    }
    out << (r.is_union ? "union " : "struct ") << r.name << " {\n";
    for (const auto& f : r.fields) {
        out << ind;
        if (f.bit_width >= 0) {
            out << type_str(f.type, f.name.empty() ? "" : f.name)
                << " : " << f.bit_width << ";\n";
        } else {
            out << type_str(f.type, f.name) << ";\n";
        }
    }
    out << "};\n";
}

void PhpFfiGenerator::emit_enum(const EnumDecl& e, std::ostream& out) const {
    const std::string& ind = opts_.indent;
    if (e.is_forward_decl) {
        out << "enum " << e.name << ";\n";
        return;
    }
    out << "enum " << e.name << " {\n";
    for (size_t i = 0; i < e.constants.size(); ++i) {
        const auto& c = e.constants[i];
        out << ind << c.name << " = " << c.value;
        if (i + 1 < e.constants.size()) out << ",";
        out << "\n";
    }
    out << "};\n";
}

void PhpFfiGenerator::emit_typedef(const TypedefDecl& td, std::ostream& out) const {
    if (!td.underlying) return;
    out << "typedef " << type_str(td.underlying, td.name) << ";\n";
}

void PhpFfiGenerator::emit_function(const FunctionDecl& f, std::ostream& out) const {
    std::string params;
    for (size_t i = 0; i < f.params.size(); ++i) {
        if (i) params += ", ";
        params += type_str(f.params[i].type, f.params[i].name);
    }
    if (f.is_variadic) {
        if (!params.empty()) params += ", ";
        params += "...";
    }
    if (params.empty()) params = "void";
    out << type_str(f.return_type) << " " << f.name << "(" << params << ");\n";
}

// ============================================================
// PhpFfiGenerator
// ============================================================

PhpFfiGenerator::PhpFfiGenerator(const GeneratorOptions& opts)
    : Generator(opts) {}

// Well-known stdint / stddef typedefs that PHP FFI cdef() needs explicitly.
static const char* PHP_STDINT_PREAMBLE = R"(/* Standard integer types */
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef long               intptr_t;
typedef unsigned long      uintptr_t;
typedef unsigned long      size_t;
typedef long               ptrdiff_t;
typedef long               ssize_t;
)";

// Check whether any type in the TU references standard typedefs that need a preamble.
static bool needs_stdint(const TranslationUnit& tu) {
    static const std::unordered_set<std::string> STDINT_NAMES = {
        "int8_t","int16_t","int32_t","int64_t",
        "uint8_t","uint16_t","uint32_t","uint64_t",
        "intptr_t","uintptr_t","size_t","ptrdiff_t","ssize_t",
    };

    std::function<bool(const CTypePtr&)> has_stdint = [&](const CTypePtr& t) -> bool {
        if (!t) return false;
        if (t->kind == TypeKind::Typedef && STDINT_NAMES.count(t->name)) return true;
        if (has_stdint(t->pointee)) return true;
        if (has_stdint(t->fn_return)) return true;
        for (auto& p : t->fn_params) if (has_stdint(p)) return true;
        return false;
    };

    for (auto& r : tu.records)
        for (auto& f : r.fields)
            if (has_stdint(f.type)) return true;
    for (auto& f : tu.functions) {
        if (has_stdint(f.return_type)) return true;
        for (auto& p : f.params) if (has_stdint(p.type)) return true;
    }
    return false;
}

void PhpFfiGenerator::generate(const TranslationUnit& tu, std::ostream& out) {
    out << "<?php\n\n";
    out << "$ffi = FFI::cdef(<<<'CDEF'\n";

    // 1. Standard type preamble (if needed)
    if (needs_stdint(tu)) {
        out << PHP_STDINT_PREAMBLE << "\n";
    }

    // 2. Forward struct/union typedefs (NOT enum — enums will be emitted in full below).
    //    This allows struct bodies to reference typedef names like "Vec3", "Node".
    bool wrote_fwd = false;
    for (const auto& td : tu.typedefs) {
        if (!td.underlying) continue;
        if (td.underlying->kind == TypeKind::Record &&
            td.underlying->name == td.name) {
            bool is_union = false;
            for (const auto& r : tu.records)
                if (r.name == td.name) { is_union = r.is_union; break; }
            out << "typedef " << (is_union ? "union " : "struct ")
                << td.name << " " << td.name << ";\n";
            wrote_fwd = true;
        }
    }
    if (wrote_fwd) out << "\n";

    // 3. Enum definitions + their typedefs (before struct bodies that may reference them)
    for (const auto& e : tu.enums) {
        if (!e.is_forward_decl) {
            emit_enum(e, out);
            out << "\n";
        }
    }
    for (const auto& td : tu.typedefs) {
        if (!td.underlying) continue;
        if (td.underlying->kind == TypeKind::Enum &&
            td.underlying->name == td.name) {
            out << "typedef enum " << td.name << " " << td.name << ";\n\n";
        }
    }

    // 4. Struct/union definitions (full bodies)
    for (const auto& r : tu.records) {
        if (!r.is_forward_decl) {
            emit_record(r, out);
            out << "\n";
        }
    }

    // 5. Non-same-name typedefs (function pointers, primitive aliases, cross-name aliases)
    bool wrote_td = false;
    for (const auto& td : tu.typedefs) {
        if (!td.underlying) continue;
        // Skip same-name struct/enum aliases (already handled above)
        if ((td.underlying->kind == TypeKind::Record ||
             td.underlying->kind == TypeKind::Enum) &&
            td.underlying->name == td.name) continue;
        emit_typedef(td, out);
        wrote_td = true;
    }
    if (wrote_td) out << "\n";

    // 6. Function declarations
    for (const auto& f : tu.functions) {
        emit_function(f, out);
    }

    out << "CDEF, \"" << opts_.library_name << "\");\n";
}
