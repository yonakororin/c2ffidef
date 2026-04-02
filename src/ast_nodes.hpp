#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>

// ============================================================
// C type representation
// ============================================================

enum class TypeKind {
    Void,
    Bool,
    Char,        // char (signedness unspecified)
    SChar,       // signed char
    UChar,       // unsigned char
    Short,
    UShort,
    Int,
    UInt,
    Long,
    ULong,
    LongLong,
    ULongLong,
    Int128,
    UInt128,
    Float,
    Double,
    LongDouble,
    Float128,
    Pointer,
    ConstantArray,
    IncompleteArray,
    Record,       // struct or union (referenced by name)
    Enum,         // enum (referenced by name)
    Typedef,      // typedef alias (referenced by name)
    FunctionProto,
    Unexposed,    // fallback
};

struct CType {
    TypeKind kind = TypeKind::Unexposed;
    std::string  name;                       // for Record/Enum/Typedef/Unexposed
    bool         is_const    = false;
    bool         is_volatile = false;

    // Pointer / Array element type
    std::shared_ptr<CType> pointee;

    // ConstantArray size
    long long array_size = 0;

    // FunctionProto
    std::shared_ptr<CType>              fn_return;
    std::vector<std::shared_ptr<CType>> fn_params;

    // helpers
    bool is_void_ptr() const {
        return kind == TypeKind::Pointer && pointee && pointee->kind == TypeKind::Void;
    }
    bool is_char_ptr() const {
        return kind == TypeKind::Pointer && pointee &&
               (pointee->kind == TypeKind::Char || pointee->kind == TypeKind::SChar ||
                pointee->kind == TypeKind::UChar);
    }
};

using CTypePtr = std::shared_ptr<CType>;

// ============================================================
// Struct / Union
// ============================================================

struct FieldDecl {
    std::string name;
    CTypePtr    type;
    int         bit_width = -1;  // -1 means not a bit-field
};

struct RecordDecl {
    std::string            name;
    bool                   is_union = false;
    std::vector<FieldDecl> fields;
    bool                   is_forward_decl = false; // no body
};

// ============================================================
// Enum
// ============================================================

struct EnumConstant {
    std::string name;
    long long   value = 0;
};

struct EnumDecl {
    std::string               name;
    std::vector<EnumConstant> constants;
    bool                      is_forward_decl = false;
};

// ============================================================
// Typedef
// ============================================================

struct TypedefDecl {
    std::string name;         // alias name
    CTypePtr    underlying;   // underlying type
};

// ============================================================
// Function
// ============================================================

struct ParamDecl {
    std::string name;   // may be empty for unnamed params
    CTypePtr    type;
};

struct FunctionDecl {
    std::string            name;
    CTypePtr               return_type;
    std::vector<ParamDecl> params;
    bool                   is_variadic = false;
};

// ============================================================
// Translation Unit  (top-level container)
// ============================================================

struct TranslationUnit {
    std::vector<RecordDecl>   records;
    std::vector<EnumDecl>     enums;
    std::vector<TypedefDecl>  typedefs;
    std::vector<FunctionDecl> functions;
};
