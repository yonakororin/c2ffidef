# c2ffidef

CヘッダファイルをパースしてPHP FFI / Python ctypes 向けのバインディングコードを生成するC++ツールです。
内部で [libclang](https://clang.llvm.org/docs/Tooling.html) を使用し、プリプロセッサ展開済みの正確なASTに基づいてコードを出力します。

---

## 機能

- `struct` / `union`（通常・ビットフィールド・自己参照・ネスト）
- `enum`（値付き列挙定数）
- `typedef`（匿名struct/union/enum・関数ポインタ・プリミティブ別名）
- ポインタ（多重ポインタ・配列・`void *`・`char *`）
- 可変長引数関数（`...`）
- `const` / `volatile` 修飾子
- `uint8_t` / `int32_t` / `size_t` などの標準整数型の自動マッピング

---

## ビルド要件

| ツール | バージョン |
|--------|-----------|
| C++コンパイラ | C++17対応（GCC 9+ / Clang 9+） |
| CMake | 3.16以上 |
| libclang | 15 / 16 / 17 / **18**（推奨） |

### インストール（Debian / Ubuntu）

```bash
sudo apt install cmake g++ libclang-18-dev
```

### インストール（RHEL / Fedora / Rocky Linux / AlmaLinux）

```bash
# Fedora
sudo dnf install cmake gcc-c++ clang-devel

# RHEL 9 / Rocky / AlmaLinux（LLVM Toolset を使用）
sudo dnf install cmake gcc-c++ llvm-toolset clang-devel
```

> **Note:** dnf 系では `libclang-dev` の代わりに `clang-devel` パッケージを使用します。
> RHEL 8 系の場合は `llvm-toolset` モジュールを有効化してください。
> ```bash
> sudo dnf module enable llvm-toolset
> sudo dnf install cmake gcc-c++ clang-devel
> ```

---

## ビルド

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

ビルド後、`build/c2ffidef` が生成されます。

---

## 使い方

```
c2ffidef [OPTIONS] <header.h>

Options:
  -l <library>   ロードするライブラリ名  (default: libfoo.so)
  -o <file>      出力ファイル            (default: stdout)
  -t <target>    出力言語: php | python | all  (default: all)
  -I <dir>       インクルードディレクトリを追加（繰り返し可）
  -D <macro>     プリプロセッサマクロを定義（繰り返し可）
  -h, --help     ヘルプを表示
```

### 例

```bash
# PHP FFI コードを生成してファイルに保存
./build/c2ffidef -l libmylib.so -t php -o ffi.php mylib.h

# Python ctypes コードを標準出力に表示
./build/c2ffidef -l libmylib.so -t python mylib.h

# 両方を同時出力（stdout）
./build/c2ffidef -l libmylib.so mylib.h

# インクルードパスとマクロを指定
./build/c2ffidef -I/usr/local/include -DENABLE_FEATURE -l libmylib.so mylib.h
```

---

## 出力例

以下のヘッダを入力とします。

```c
// mylib.h
#include <stdint.h>

typedef struct { float x, y, z; } Vec3;

typedef enum { COLOR_RED=0, COLOR_GREEN=1, COLOR_BLUE=2 } Color;

Vec3*  vec3_add(const Vec3* a, const Vec3* b);
void   vec3_free(Vec3* v);
int    log_message(const char* fmt, ...);
```

### PHP FFI (`-t php`)

```php
<?php

$ffi = FFI::cdef(<<<'CDEF'
/* Standard integer types */
typedef unsigned char  uint8_t;
// ... (省略)

typedef struct Vec3 Vec3;

enum Color {
    COLOR_RED = 0,
    COLOR_GREEN = 1,
    COLOR_BLUE = 2
};
typedef enum Color Color;

struct Vec3 {
    float x;
    float y;
    float z;
};

Vec3 * vec3_add(const Vec3 *a, const Vec3 *b);
void vec3_free(Vec3 *v);
int log_message(const char *fmt, ...);
CDEF, "libmylib.so");
```

### Python ctypes (`-t python`)

```python
import ctypes
import ctypes.util

_lib = ctypes.CDLL("libmylib.so")

class Vec3(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_float),
        ("y", ctypes.c_float),
        ("z", ctypes.c_float),
    ]

# enum Color
COLOR_RED = 0
COLOR_GREEN = 1
COLOR_BLUE = 2

_lib.vec3_add.argtypes = [ctypes.POINTER(Vec3), ctypes.POINTER(Vec3)]
_lib.vec3_add.restype  = ctypes.POINTER(Vec3)

_lib.vec3_free.argtypes = [ctypes.POINTER(Vec3)]
_lib.vec3_free.restype  = None

_lib.log_message.argtypes = [ctypes.c_char_p]  # variadic
_lib.log_message.restype  = ctypes.c_int
```

---

## 型マッピング

### プリミティブ型

| C型 | PHP FFI | Python ctypes |
|-----|---------|---------------|
| `void` | `void` | `None` |
| `_Bool` | `_Bool` | `ctypes.c_bool` |
| `char` | `char` | `ctypes.c_char` |
| `signed char` | `signed char` | `ctypes.c_byte` |
| `unsigned char` | `unsigned char` | `ctypes.c_ubyte` |
| `short` | `short` | `ctypes.c_short` |
| `int` | `int` | `ctypes.c_int` |
| `long` | `long` | `ctypes.c_long` |
| `long long` | `long long` | `ctypes.c_longlong` |
| `float` | `float` | `ctypes.c_float` |
| `double` | `double` | `ctypes.c_double` |

### ポインタ・配列

| C型 | PHP FFI | Python ctypes |
|-----|---------|---------------|
| `T *` | `T *` | `ctypes.POINTER(T)` |
| `void *` | `void *` | `ctypes.c_void_p` |
| `char *` | `char *` | `ctypes.c_char_p` |
| `T[N]` | `T[N]` | `T * N` |
| `ret (*fn)(args)` | `ret (*fn)(args)` | `ctypes.CFUNCTYPE(ret, args)` |

### 標準整数型（自動解決）

| C型 | Python ctypes |
|-----|---------------|
| `uint8_t` | `ctypes.c_uint8` |
| `int32_t` | `ctypes.c_int32` |
| `size_t` | `ctypes.c_size_t` |
| `int64_t` | `ctypes.c_int64` |
| ... | ... |

PHP FFI では cdef 先頭に `typedef` 宣言を自動生成します。

---

## アーキテクチャ

```
src/
├── ast_nodes.hpp      — CType / RecordDecl / FunctionDecl などのAST定義
├── parser.hpp/cpp     — libclang によるヘッダ解析
├── generator.hpp      — Generator 基底クラス
├── php_ffi_gen.hpp/cpp— PHP FFI::cdef() コード生成
├── ctypes_gen.hpp/cpp — Python ctypes コード生成
└── main.cpp           — CLIエントリポイント
```

パーサは libclang の C API を通じて完全なASTを構築するため、マクロやインクルードを含む複雑なヘッダも正しく解析できます。

---

## 制限事項

- Cヘッダのみ対応（C++ヘッダは `-x c` として解析）
- `__attribute__` 等のコンパイラ拡張は無視される場合があります
- `union` の Python ctypes 出力は `ctypes.Union` を使用しますが、フィールドが重複する場合は手動調整が必要なことがあります
- 可変長引数関数の Python ctypes `argtypes` は第1引数までで打ち切られます

---

## ライセンス

MIT
