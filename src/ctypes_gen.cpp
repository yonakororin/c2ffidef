#include "ctypes_gen.hpp"
#include <ostream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

// ============================================================
// Well-known C typedef → ctypes mapping
// Covers <stdint.h>, <stddef.h>, <sys/types.h>, etc.
// ============================================================

static const std::unordered_map<std::string, std::string> KNOWN_TYPEDEFS = {
    // stdint.h
    {"int8_t",    "ctypes.c_int8"},
    {"int16_t",   "ctypes.c_int16"},
    {"int32_t",   "ctypes.c_int32"},
    {"int64_t",   "ctypes.c_int64"},
    {"uint8_t",   "ctypes.c_uint8"},
    {"uint16_t",  "ctypes.c_uint16"},
    {"uint32_t",  "ctypes.c_uint32"},
    {"uint64_t",  "ctypes.c_uint64"},
    {"int_least8_t",    "ctypes.c_int8"},
    {"int_least16_t",   "ctypes.c_int16"},
    {"int_least32_t",   "ctypes.c_int32"},
    {"int_least64_t",   "ctypes.c_int64"},
    {"uint_least8_t",   "ctypes.c_uint8"},
    {"uint_least16_t",  "ctypes.c_uint16"},
    {"uint_least32_t",  "ctypes.c_uint32"},
    {"uint_least64_t",  "ctypes.c_uint64"},
    {"int_fast8_t",     "ctypes.c_int8"},
    {"int_fast16_t",    "ctypes.c_int32"},
    {"int_fast32_t",    "ctypes.c_int32"},
    {"int_fast64_t",    "ctypes.c_int64"},
    {"uint_fast8_t",    "ctypes.c_uint8"},
    {"uint_fast16_t",   "ctypes.c_uint32"},
    {"uint_fast32_t",   "ctypes.c_uint32"},
    {"uint_fast64_t",   "ctypes.c_uint64"},
    {"intmax_t",   "ctypes.c_int64"},
    {"uintmax_t",  "ctypes.c_uint64"},
    {"intptr_t",   "ctypes.c_ssize_t"},
    {"uintptr_t",  "ctypes.c_size_t"},
    // stddef.h / sys/types.h
    {"size_t",     "ctypes.c_size_t"},
    {"ssize_t",    "ctypes.c_ssize_t"},
    {"ptrdiff_t",  "ctypes.c_ssize_t"},
    {"wchar_t",    "ctypes.c_wchar"},
    {"off_t",      "ctypes.c_int64"},
    {"pid_t",      "ctypes.c_int32"},
    {"uid_t",      "ctypes.c_uint32"},
    {"gid_t",      "ctypes.c_uint32"},
    {"mode_t",     "ctypes.c_uint32"},
    {"time_t",     "ctypes.c_int64"},
};

// ============================================================
// Primitive type mapping
// ============================================================

static std::string primitive_ctypes(TypeKind k) {
    switch (k) {
    case TypeKind::Void:       return "None";
    case TypeKind::Bool:       return "ctypes.c_bool";
    case TypeKind::Char:       return "ctypes.c_char";
    case TypeKind::SChar:      return "ctypes.c_byte";
    case TypeKind::UChar:      return "ctypes.c_ubyte";
    case TypeKind::Short:      return "ctypes.c_short";
    case TypeKind::UShort:     return "ctypes.c_ushort";
    case TypeKind::Int:        return "ctypes.c_int";
    case TypeKind::UInt:       return "ctypes.c_uint";
    case TypeKind::Long:       return "ctypes.c_long";
    case TypeKind::ULong:      return "ctypes.c_ulong";
    case TypeKind::LongLong:   return "ctypes.c_longlong";
    case TypeKind::ULongLong:  return "ctypes.c_ulonglong";
    case TypeKind::Int128:     return "ctypes.c_int64 * 2  # __int128 approximation";
    case TypeKind::UInt128:    return "ctypes.c_uint64 * 2  # unsigned __int128 approximation";
    case TypeKind::Float:      return "ctypes.c_float";
    case TypeKind::Double:     return "ctypes.c_double";
    case TypeKind::LongDouble: return "ctypes.c_longdouble";
    case TypeKind::Float128:   return "ctypes.c_longdouble  # __float128 approximation";
    default:                   return "ctypes.c_void_p  # unknown";
    }
}

// ============================================================
// ctypes type expression
// ============================================================

std::string CtypesGenerator::ctypes_type(const CTypePtr& t) const {
    if (!t) return "None";

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
    case TypeKind::Float128:
        return primitive_ctypes(t->kind);

    case TypeKind::Pointer: {
        if (!t->pointee) return "ctypes.c_void_p";

        // void* → c_void_p
        if (t->pointee->kind == TypeKind::Void) {
            return "ctypes.c_void_p";
        }
        // char* → c_char_p
        if (t->pointee->kind == TypeKind::Char ||
            t->pointee->kind == TypeKind::SChar) {
            return "ctypes.c_char_p";
        }
        // function pointer
        if (t->pointee->kind == TypeKind::FunctionProto) {
            const auto& fp = t->pointee;
            std::string ret = ctypes_type(fp->fn_return);
            std::string params;
            for (size_t i = 0; i < fp->fn_params.size(); ++i) {
                if (i) params += ", ";
                params += ctypes_type(fp->fn_params[i]);
            }
            return "ctypes.CFUNCTYPE(" + ret +
                   (params.empty() ? "" : ", " + params) + ")";
        }
        // other pointer
        return "ctypes.POINTER(" + ctypes_type(t->pointee) + ")";
    }

    case TypeKind::ConstantArray:
        return ctypes_type(t->pointee) + " * " + std::to_string(t->array_size);

    case TypeKind::IncompleteArray:
        return ctypes_type(t->pointee) + " * 0  # incomplete array";

    case TypeKind::Record:
        return t->name;

    case TypeKind::Typedef: {
        // Check well-known system typedefs first
        auto it = KNOWN_TYPEDEFS.find(t->name);
        if (it != KNOWN_TYPEDEFS.end()) return it->second;
        return t->name;
    }

    case TypeKind::Enum:
        return "ctypes.c_int  # enum " + t->name;

    case TypeKind::FunctionProto: {
        std::string ret = ctypes_type(t->fn_return);
        std::string params;
        for (size_t i = 0; i < t->fn_params.size(); ++i) {
            if (i) params += ", ";
            params += ctypes_type(t->fn_params[i]);
        }
        return "ctypes.CFUNCTYPE(" + ret +
               (params.empty() ? "" : ", " + params) + ")";
    }

    case TypeKind::Unexposed:
        return "ctypes.c_void_p  # " + t->name;

    default:
        return "ctypes.c_void_p  # unsupported";
    }
}

// ============================================================
// Emit helpers
// ============================================================

void CtypesGenerator::emit_record(const RecordDecl& r, std::ostream& out) const {
    const std::string& ind = opts_.indent;

    if (r.is_forward_decl) {
        // Forward declaration: emit class stub without _fields_
        std::string base = r.is_union ? "ctypes.Union" : "ctypes.Structure";
        out << "class " << r.name << "(" << base << "):\n";
        out << ind << "pass  # forward declaration\n\n";
        return;
    }

    std::string base = r.is_union ? "ctypes.Union" : "ctypes.Structure";
    out << "class " << r.name << "(" << base << "):\n";

    // Check for bitfields
    bool has_bitfields = false;
    for (const auto& f : r.fields) {
        if (f.bit_width >= 0) { has_bitfields = true; break; }
    }

    if (r.fields.empty()) {
        out << ind << "pass\n\n";
        return;
    }

    if (has_bitfields) {
        out << ind << "_fields_ = [\n";
        for (const auto& f : r.fields) {
            std::string fname = f.name.empty() ? "_padding" : f.name;
            out << ind << ind << "(\"" << fname << "\", "
                << ctypes_type(f.type);
            if (f.bit_width >= 0) {
                out << ", " << f.bit_width;
            }
            out << "),\n";
        }
        out << ind << "]\n\n";
    } else {
        out << ind << "_fields_ = [\n";
        for (const auto& f : r.fields) {
            std::string fname = f.name.empty() ? "_padding" : f.name;
            out << ind << ind << "(\"" << fname << "\", "
                << ctypes_type(f.type) << "),\n";
        }
        out << ind << "]\n\n";
    }
}

void CtypesGenerator::emit_enum(const EnumDecl& e, std::ostream& out) const {
    if (e.is_forward_decl || e.constants.empty()) return;

    out << "# enum " << e.name << "\n";
    for (const auto& c : e.constants) {
        out << c.name << " = " << c.value << "\n";
    }
    out << "\n";
}

void CtypesGenerator::emit_typedef(const TypedefDecl& td, std::ostream& out) const {
    if (!td.underlying) return;

    // Skip self-alias for structs/unions: class is already defined
    if (td.underlying->kind == TypeKind::Record &&
        td.underlying->name == td.name) {
        return;
    }

    // Enum self-alias ("typedef enum Foo Foo") is emitted in the generate()
    // ordering step before struct definitions; skip it here.
    if (td.underlying->kind == TypeKind::Enum &&
        td.underlying->name == td.name) {
        return;
    }

    // Skip if this typedef name is a well-known system type already handled
    // (it would be emitted as a ctypes type expression inline, not as a class)
    if (KNOWN_TYPEDEFS.count(td.name)) return;

    // All other cases: emit assignment
    std::string rhs = ctypes_type(td.underlying);
    out << td.name << " = " << rhs << "\n\n";
}

void CtypesGenerator::emit_function(const FunctionDecl& f, std::ostream& out) const {
    // argtypes
    out << "_lib." << f.name << ".argtypes = [";
    for (size_t i = 0; i < f.params.size(); ++i) {
        if (i) out << ", ";
        out << ctypes_type(f.params[i].type);
    }
    if (f.is_variadic) {
        // ctypes doesn't support variadic argtypes declaration — leave it empty
        // or use a comment
        out << "]  # variadic; omit extra args or use restype only\n";
    } else {
        out << "]\n";
    }

    // restype
    std::string restype = ctypes_type(f.return_type);
    if (restype == "None") {
        out << "_lib." << f.name << ".restype  = None\n";
    } else {
        out << "_lib." << f.name << ".restype  = " << restype << "\n";
    }
    out << "\n";
}

// ============================================================
// CtypesGenerator
// ============================================================

CtypesGenerator::CtypesGenerator(const GeneratorOptions& opts)
    : Generator(opts) {}

void CtypesGenerator::generate(const TranslationUnit& tu, std::ostream& out) {
    out << "import ctypes\nimport ctypes.util\n\n";
    out << "_lib = ctypes.CDLL(\"" << opts_.library_name << "\")\n\n";

    // Collect names of structs that appear BOTH as forward decl and as definition.
    // For these, we emit:
    //   class Foo(ctypes.Structure): pass      ← forward decl
    //   Foo._fields_ = [...]                   ← separate assignment after all classes
    std::unordered_set<std::string> forward_names;
    for (const auto& r : tu.records)
        if (r.is_forward_decl) forward_names.insert(r.name);

    // 1. Forward declaration stubs (struct/union only)
    for (const auto& r : tu.records) {
        if (r.is_forward_decl) {
            emit_record(r, out);
        }
    }

    // 2. Enum constants + enum typedef aliases.
    //    Must come before struct definitions that reference enum typedef names
    //    in their _fields_ (e.g. "("color", ColorChannel)").
    for (const auto& e : tu.enums) {
        emit_enum(e, out);
    }
    for (const auto& td : tu.typedefs) {
        if (!td.underlying) continue;
        if (td.underlying->kind == TypeKind::Enum &&
            td.underlying->name == td.name) {
            out << td.name << " = ctypes.c_int  # enum\n\n";
        }
    }

    // 3. Struct/union definitions (can now reference enum typedef names)
    for (const auto& r : tu.records) {
        if (!r.is_forward_decl) {
            if (forward_names.count(r.name)) {
                // Already have the class stub; emit _fields_ assignment instead
                const std::string& ind = opts_.indent;
                out << r.name << "._fields_ = [\n";
                for (const auto& f : r.fields) {
                    std::string fname = f.name.empty() ? "_padding" : f.name;
                    out << ind << "(\"" << fname << "\", " << ctypes_type(f.type);
                    if (f.bit_width >= 0) out << ", " << f.bit_width;
                    out << "),\n";
                }
                out << "]\n\n";
            } else {
                emit_record(r, out);
            }
        }
    }

    // 4. Non-enum typedefs (struct aliases, function pointers, primitives)
    for (const auto& td : tu.typedefs) {
        if (!td.underlying) continue;
        // Skip enum self-aliases: already emitted in step 2
        if (td.underlying->kind == TypeKind::Enum &&
            td.underlying->name == td.name) continue;
        emit_typedef(td, out);
    }

    // Functions
    for (const auto& f : tu.functions) {
        emit_function(f, out);
    }
}
