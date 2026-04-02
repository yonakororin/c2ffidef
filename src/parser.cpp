#include "parser.hpp"
#include <clang-c/Index.h>
#include <stdexcept>
#include <cstring>
#include <unordered_set>
#include <unordered_map>
#include <cassert>
#include <cstdio>
#include <sys/stat.h>

// ============================================================
// Helpers
// ============================================================

static std::string cx_to_str(CXString s) {
    std::string result = clang_getCString(s);
    clang_disposeString(s);
    return result;
}

static std::string get_cursor_spelling(CXCursor c) {
    return cx_to_str(clang_getCursorSpelling(c));
}

static std::string get_type_spelling(CXType t) {
    return cx_to_str(clang_getTypeSpelling(t));
}

// ============================================================
// Type conversion: CXType → CTypePtr
// ============================================================

static CTypePtr convert_type(CXType t);

static CTypePtr convert_type(CXType t) {
    // Strip const/volatile at the top level
    bool is_const    = clang_isConstQualifiedType(t);
    bool is_volatile = clang_isVolatileQualifiedType(t);

    auto result = std::make_shared<CType>();
    result->is_const    = is_const;
    result->is_volatile = is_volatile;

    switch (t.kind) {
    case CXType_Void:           result->kind = TypeKind::Void;       break;
    case CXType_Bool:           result->kind = TypeKind::Bool;       break;
    case CXType_Char_U:
    case CXType_Char_S:         result->kind = TypeKind::Char;       break;
    case CXType_SChar:          result->kind = TypeKind::SChar;      break;
    case CXType_UChar:          result->kind = TypeKind::UChar;      break;
    case CXType_Short:          result->kind = TypeKind::Short;      break;
    case CXType_UShort:         result->kind = TypeKind::UShort;     break;
    case CXType_Int:            result->kind = TypeKind::Int;        break;
    case CXType_UInt:           result->kind = TypeKind::UInt;       break;
    case CXType_Long:           result->kind = TypeKind::Long;       break;
    case CXType_ULong:          result->kind = TypeKind::ULong;      break;
    case CXType_LongLong:       result->kind = TypeKind::LongLong;   break;
    case CXType_ULongLong:      result->kind = TypeKind::ULongLong;  break;
    case CXType_Int128:         result->kind = TypeKind::Int128;     break;
    case CXType_UInt128:        result->kind = TypeKind::UInt128;    break;
    case CXType_Float:          result->kind = TypeKind::Float;      break;
    case CXType_Double:         result->kind = TypeKind::Double;     break;
    case CXType_LongDouble:     result->kind = TypeKind::LongDouble; break;
    case CXType_Float128:       result->kind = TypeKind::Float128;   break;

    case CXType_Pointer: {
        result->kind    = TypeKind::Pointer;
        CXType pointee  = clang_getPointeeType(t);
        if (pointee.kind == CXType_FunctionProto ||
            pointee.kind == CXType_FunctionNoProto) {
            // function pointer: store as FunctionProto under pointee
            result->pointee = convert_type(pointee);
        } else {
            result->pointee = convert_type(pointee);
        }
        break;
    }

    case CXType_ConstantArray: {
        result->kind       = TypeKind::ConstantArray;
        result->array_size = clang_getArraySize(t);
        result->pointee    = convert_type(clang_getArrayElementType(t));
        break;
    }

    case CXType_IncompleteArray: {
        result->kind    = TypeKind::IncompleteArray;
        result->pointee = convert_type(clang_getArrayElementType(t));
        break;
    }

    case CXType_Record: {
        result->kind = TypeKind::Record;
        CXCursor decl = clang_getTypeDeclaration(t);
        result->name  = get_cursor_spelling(decl);
        // anonymous structs/unions get a synthetic name from spelling
        if (result->name.empty()) {
            result->name = get_type_spelling(t);
        }
        break;
    }

    case CXType_Enum: {
        result->kind = TypeKind::Enum;
        CXCursor decl = clang_getTypeDeclaration(t);
        result->name  = get_cursor_spelling(decl);
        if (result->name.empty()) {
            result->name = get_type_spelling(t);
        }
        break;
    }

    case CXType_Typedef: {
        result->kind = TypeKind::Typedef;
        // Use cursor spelling to get the bare typedef name WITHOUT cv-qualifiers.
        // clang_getTypeSpelling() includes "const " which would double-up with is_const.
        CXCursor decl = clang_getTypeDeclaration(t);
        std::string decl_name = get_cursor_spelling(decl);
        result->name = decl_name.empty() ? get_type_spelling(t) : decl_name;
        break;
    }

    case CXType_Elaborated: {
        // Elaborated type: "struct Foo", "enum Bar", etc.
        // Unwrap to the named type and preserve cv qualifiers.
        CXType named = clang_Type_getNamedType(t);
        auto inner = convert_type(named);
        inner->is_const    = inner->is_const    || is_const;
        inner->is_volatile = inner->is_volatile || is_volatile;
        return inner;
    }

    case CXType_FunctionProto:
    case CXType_FunctionNoProto: {
        result->kind      = TypeKind::FunctionProto;
        result->fn_return = convert_type(clang_getResultType(t));
        int n = clang_getNumArgTypes(t);
        for (int i = 0; i < n; ++i) {
            result->fn_params.push_back(convert_type(clang_getArgType(t, i)));
        }
        break;
    }

    default:
        result->kind = TypeKind::Unexposed;
        result->name = get_type_spelling(t);
        break;
    }

    return result;
}

// ============================================================
// Visitor state
// ============================================================

struct VisitorState {
    TranslationUnit&             tu;
    const ParseOptions&          opts;
    std::string                  main_file;
    std::unordered_set<std::string> seen_records;
    std::unordered_set<std::string> seen_enums;
    std::unordered_set<std::string> seen_typedefs;
    std::unordered_set<std::string> seen_functions;
};

// ============================================================
// Struct / Union visitor
// ============================================================

struct FieldVisitorState {
    RecordDecl& record;
};

static CXChildVisitResult field_visitor(CXCursor cursor, CXCursor /*parent*/,
                                        CXClientData data) {
    auto* state = reinterpret_cast<FieldVisitorState*>(data);

    if (clang_getCursorKind(cursor) == CXCursor_FieldDecl) {
        FieldDecl field;
        field.name      = get_cursor_spelling(cursor);
        field.type      = convert_type(clang_getCursorType(cursor));
        field.bit_width = clang_getFieldDeclBitWidth(cursor); // -1 if not bit-field
        state->record.fields.push_back(std::move(field));
    }

    return CXChildVisit_Continue;
}

static RecordDecl build_record(CXCursor cursor) {
    RecordDecl rec;
    rec.name     = get_cursor_spelling(cursor);
    rec.is_union = (clang_getCursorKind(cursor) == CXCursor_UnionDecl);
    rec.is_forward_decl = !clang_isCursorDefinition(cursor);

    if (!rec.is_forward_decl) {
        FieldVisitorState fstate{rec};
        clang_visitChildren(cursor, field_visitor,
                            reinterpret_cast<CXClientData>(&fstate));
    }
    return rec;
}

// ============================================================
// Enum visitor
// ============================================================

struct EnumVisitorState {
    EnumDecl& ed;
};

static CXChildVisitResult enum_const_visitor(CXCursor cursor, CXCursor /*parent*/,
                                             CXClientData data) {
    if (clang_getCursorKind(cursor) == CXCursor_EnumConstantDecl) {
        auto* state = reinterpret_cast<EnumVisitorState*>(data);
        EnumConstant ec;
        ec.name  = get_cursor_spelling(cursor);
        ec.value = clang_getEnumConstantDeclValue(cursor);
        state->ed.constants.push_back(std::move(ec));
    }
    return CXChildVisit_Continue;
}

static EnumDecl build_enum(CXCursor cursor) {
    EnumDecl ed;
    ed.name            = get_cursor_spelling(cursor);
    ed.is_forward_decl = !clang_isCursorDefinition(cursor);

    if (!ed.is_forward_decl) {
        EnumVisitorState es{ed};
        clang_visitChildren(cursor, enum_const_visitor,
                            reinterpret_cast<CXClientData>(&es));
    }
    return ed;
}

// ============================================================
// Check if a cursor should be included in output
// ============================================================

static bool should_include_cursor(CXCursor cursor, const std::string& main_file,
                                  const ParseOptions& opts) {
    CXSourceLocation loc = clang_getCursorLocation(cursor);

    // Always skip system headers when requested
    if (opts.skip_system_includes && clang_Location_isInSystemHeader(loc)) {
        return false;
    }

    CXFile file;
    clang_getSpellingLocation(loc, &file, nullptr, nullptr, nullptr);
    if (!file) return false;

    // In main_file_only mode restrict to the top-level file
    if (opts.main_file_only) {
        std::string fname = cx_to_str(clang_getFileName(file));
        return fname == main_file;
    }

    // Default: include all user (non-system) files
    return true;
}

// ============================================================
// Top-level visitor
// ============================================================

static CXChildVisitResult top_visitor(CXCursor cursor, CXCursor /*parent*/,
                                      CXClientData data) {
    auto* state = reinterpret_cast<VisitorState*>(data);

    if (!should_include_cursor(cursor, state->main_file, state->opts)) {
        return CXChildVisit_Continue;
    }

    CXCursorKind kind = clang_getCursorKind(cursor);

    switch (kind) {
    // ── extern "C" { ... } block ────────────────────────────────────────────
    // Recurse into linkage-spec nodes so that declarations inside
    // extern "C" {} are processed exactly like top-level C declarations.
    case CXCursor_LinkageSpec:
        clang_visitChildren(cursor, top_visitor,
                            reinterpret_cast<CXClientData>(state));
        return CXChildVisit_Continue;

    case CXCursor_StructDecl:
    case CXCursor_UnionDecl: {
        std::string name = get_cursor_spelling(cursor);
        // Skip anonymous structs that are part of a typedef (handled via typedef)
        if (name.empty()) break;
        // Only process the definition (or first forward decl if no definition)
        if (!clang_isCursorDefinition(cursor)) {
            // Only add forward decl if we haven't seen this name yet
            if (state->seen_records.count(name)) break;
        }
        state->seen_records.insert(name);
        state->tu.records.push_back(build_record(cursor));
        break;
    }

    case CXCursor_EnumDecl: {
        std::string name = get_cursor_spelling(cursor);
        if (name.empty()) break;
        if (!clang_isCursorDefinition(cursor)) {
            if (state->seen_enums.count(name)) break;
        }
        state->seen_enums.insert(name);
        state->tu.enums.push_back(build_enum(cursor));
        break;
    }

    case CXCursor_TypedefDecl: {
        std::string name = get_cursor_spelling(cursor);
        if (state->seen_typedefs.count(name)) break;
        state->seen_typedefs.insert(name);

        TypedefDecl td;
        td.name = name;

        // Get the underlying type via clang_getTypedefDeclUnderlyingType.
        // May return CXType_Elaborated ("struct Foo") on modern clang, so unwrap it.
        CXType underlying = clang_getTypedefDeclUnderlyingType(cursor);
        CXType resolved   = underlying;
        if (underlying.kind == CXType_Elaborated) {
            resolved = clang_Type_getNamedType(underlying);
        }

        // If the underlying type is an anonymous or named record/enum, handle specially
        if (resolved.kind == CXType_Record || resolved.kind == CXType_Enum) {
            CXCursor decl = clang_getTypeDeclaration(resolved);
            std::string decl_name = get_cursor_spelling(decl);

            if (decl_name.empty()) {
                // Anonymous: adopt the record/enum with the typedef name
                if (resolved.kind == CXType_Record) {
                    RecordDecl rec = build_record(decl);
                    rec.name = name;
                    if (!state->seen_records.count(name)) {
                        state->seen_records.insert(name);
                        state->tu.records.push_back(std::move(rec));
                    }
                } else {
                    EnumDecl ed = build_enum(decl);
                    ed.name = name;
                    if (!state->seen_enums.count(name)) {
                        state->seen_enums.insert(name);
                        state->tu.enums.push_back(std::move(ed));
                    }
                }
                td.underlying = convert_type(resolved);
                td.underlying->name = name;
            } else {
                td.underlying = convert_type(resolved);
                // Ensure the struct/enum itself is recorded
                if (resolved.kind == CXType_Record &&
                    !state->seen_records.count(decl_name)) {
                    state->seen_records.insert(decl_name);
                    state->tu.records.push_back(build_record(decl));
                } else if (resolved.kind == CXType_Enum &&
                           !state->seen_enums.count(decl_name)) {
                    state->seen_enums.insert(decl_name);
                    state->tu.enums.push_back(build_enum(decl));
                }
            }
        } else {
            td.underlying = convert_type(underlying);
        }

        state->tu.typedefs.push_back(std::move(td));
        break;
    }

    case CXCursor_FunctionDecl: {
        std::string name = get_cursor_spelling(cursor);
        if (state->seen_functions.count(name)) break;
        state->seen_functions.insert(name);

        FunctionDecl fd;
        fd.name        = name;
        fd.return_type = convert_type(clang_getCursorResultType(cursor));
        fd.is_variadic = clang_isFunctionTypeVariadic(clang_getCursorType(cursor));

        int n = clang_Cursor_getNumArguments(cursor);
        for (int i = 0; i < n; ++i) {
            CXCursor param = clang_Cursor_getArgument(cursor, i);
            ParamDecl pd;
            pd.name = get_cursor_spelling(param);
            pd.type = convert_type(clang_getCursorType(param));
            fd.params.push_back(std::move(pd));
        }

        state->tu.functions.push_back(std::move(fd));
        break;
    }

    default:
        break;
    }

    return CXChildVisit_Continue;
}

// ============================================================
// Resource directory auto-detection
// ============================================================

// libclang needs its own builtin headers (stddef.h, stdint.h, ...).
// On Debian/Ubuntu they live inside /usr/lib/llvm-<ver>/lib/clang/<ver>/include/.
// On RHEL/Fedora/Oracle Linux they live inside /usr/lib/clang/<ver>/include/
// or /usr/lib64/llvm<ver>/lib/clang/<ver>/include/.
//
// If the user did not pass -resource-dir explicitly, we try to find it
// automatically so that system headers resolve correctly on all distros.

static bool path_exists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

// Ask the system `clang` binary where its resource dir is.
static std::string resource_dir_from_clang_binary() {
    FILE* pipe = popen("clang --print-resource-dir 2>/dev/null", "r");
    if (!pipe) return {};
    char buf[512] = {};
    if (fgets(buf, sizeof(buf), pipe)) {
        pclose(pipe);
        std::string s = buf;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        if (path_exists(s + "/include/stddef.h")) return s;
    } else {
        pclose(pipe);
    }
    return {};
}

// Search well-known filesystem paths for the clang resource directory.
static std::string resource_dir_from_filesystem() {
    // candidate patterns, checked in order
    // Debian/Ubuntu: /usr/lib/llvm-<ver>/lib/clang/<ver>
    for (int ver : {20, 19, 18, 17, 16, 15}) {
        std::string p = "/usr/lib/llvm-" + std::to_string(ver) +
                        "/lib/clang/" + std::to_string(ver);
        if (path_exists(p + "/include/stddef.h")) return p;
    }
    // RHEL/Fedora/Oracle Linux: /usr/lib/clang/<ver>  (unversioned llvm dir)
    for (int ver : {20, 19, 18, 17, 16, 15}) {
        std::string p = "/usr/lib/clang/" + std::to_string(ver);
        if (path_exists(p + "/include/stddef.h")) return p;
    }
    // RHEL/Oracle Linux: /usr/lib64/llvm<ver>/lib/clang/<ver>
    for (int ver : {20, 19, 18, 17, 16, 15}) {
        std::string p = "/usr/lib64/llvm" + std::to_string(ver) +
                        "/lib/clang/" + std::to_string(ver);
        if (path_exists(p + "/include/stddef.h")) return p;
    }
    return {};
}

// Returns the resource-dir string to pass as -resource-dir, or empty if
// -resource-dir is already present in extra_args or none is found.
static std::string detect_resource_dir(const ParseOptions& opts) {
    // If the caller already specified -resource-dir, don't override it.
    for (const auto& a : opts.extra_args) {
        if (a.find("-resource-dir") != std::string::npos) return {};
    }
    std::string dir = resource_dir_from_clang_binary();
    if (dir.empty()) dir = resource_dir_from_filesystem();
    return dir;
}

// ============================================================
// Public API
// ============================================================

TranslationUnit parse_header(const std::string& filepath,
                             const ParseOptions& opts) {
    CXIndex index = clang_createIndex(0, 0);
    if (!index) throw std::runtime_error("clang_createIndex failed");

    // Auto-detect clang resource directory so that builtin headers
    // (stddef.h, stdint.h, ...) are found on all distros.
    std::string resource_dir     = detect_resource_dir(opts);
    std::string resource_dir_arg = resource_dir.empty()
                                   ? std::string{}
                                   : "-resource-dir=" + resource_dir;

    // Determine language mode.
    // In "auto" mode, use C++ for common C++ header extensions.
    auto ends_with = [](const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    bool use_cpp = false;
    if (opts.language == "c++") {
        use_cpp = true;
    } else if (opts.language == "auto") {
        for (const char* ext : {".hpp", ".hh", ".hxx", ".h++", ".cpp", ".cxx"}) {
            if (ends_with(filepath, ext)) { use_cpp = true; break; }
        }
    }

    std::vector<const char*> args;
    args.push_back("-x");
    args.push_back(use_cpp ? "c++" : "c");
    if (!resource_dir_arg.empty())
        args.push_back(resource_dir_arg.c_str());
    for (auto& a : opts.extra_args)
        args.push_back(a.c_str());

    unsigned parse_flags = CXTranslationUnit_SkipFunctionBodies |
                           CXTranslationUnit_DetailedPreprocessingRecord;

    CXTranslationUnit tu = clang_parseTranslationUnit(
        index, filepath.c_str(),
        args.data(), static_cast<int>(args.size()),
        nullptr, 0,
        parse_flags);

    if (!tu) {
        clang_disposeIndex(index);
        throw std::runtime_error("Failed to parse: " + filepath);
    }

    // Check for fatal diagnostics
    unsigned n_diag = clang_getNumDiagnostics(tu);
    for (unsigned i = 0; i < n_diag; ++i) {
        CXDiagnostic diag = clang_getDiagnostic(tu, i);
        if (clang_getDiagnosticSeverity(diag) == CXDiagnostic_Fatal) {
            std::string msg = cx_to_str(clang_formatDiagnostic(
                diag, CXDiagnostic_DisplaySourceLocation));
            clang_disposeDiagnostic(diag);
            clang_disposeTranslationUnit(tu);
            clang_disposeIndex(index);
            throw std::runtime_error("Fatal clang error: " + msg);
        }
        clang_disposeDiagnostic(diag);
    }

    // Resolve the real path of the main file
    CXFile main_file = clang_getFile(tu, filepath.c_str());
    std::string main_file_path = main_file
        ? cx_to_str(clang_getFileName(main_file))
        : filepath;

    TranslationUnit result;
    VisitorState state{result, opts, main_file_path, {}, {}, {}, {}};

    CXCursor root = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(root, top_visitor,
                        reinterpret_cast<CXClientData>(&state));

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);

    return result;
}
