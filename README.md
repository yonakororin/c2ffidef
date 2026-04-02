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
- `#define` 定数の自動解決（算術式・シフト演算を含む）
- 複数ヘッダのインクルード連鎖に対応（transitive include）
- C++ ヘッダの `extern "C"` ブロック対応
- 生成コードの構文検証（`--validate`）
- DEB / RPM パッケージ生成（CPack）

---

## ビルド要件

| ツール | バージョン |
|--------|-----------|
| C++コンパイラ | C++17対応（GCC 9+ / Clang 9+） |
| CMake | 3.16以上 |
| libclang | 15〜20（distro 付属のものを自動検出） |

### インストール（Debian / Ubuntu）

```bash
sudo apt install cmake g++ libclang-18-dev
```

### インストール（RHEL / Fedora / Oracle Linux / Rocky Linux / AlmaLinux）

```bash
# Fedora
sudo dnf install cmake gcc-c++ clang-devel

# RHEL 9 / Oracle Linux 9 / Rocky / AlmaLinux
sudo dnf install cmake gcc-c++ llvm-toolset clang-devel

# RHEL 8 系（モジュール有効化が必要）
sudo dnf module enable llvm-toolset
sudo dnf install cmake gcc-c++ clang-devel
```

> **Note:** dnf 系では `libclang-dev` の代わりに `clang-devel` パッケージを使用します。
> libclang のパスとリソースディレクトリ（`stddef.h` 等）はビルド時・実行時に自動検出されます。

---

## ビルド

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

ビルド後、`build/c2ffidef` が生成されます。

---

## パッケージ生成（DEB / RPM）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# DEB と RPM を両方生成
cmake --build build --target package

# 個別生成
cd build
cpack -G DEB   # → c2ffidef_<ver>_<arch>.deb
cpack -G RPM   # → c2ffidef-<ver>-1.<arch>.rpm
```

| | DEB | RPM |
|---|---|---|
| 依存 | `libclang1-18\|17\|16\|15` | `clang-libs >= 15` |
| インストール先 | `/usr/bin/c2ffidef` | `/usr/bin/c2ffidef` |

---

## 使い方

```
c2ffidef [OPTIONS] <header.h>

Options:
  -l <library>   ロードするライブラリ名           (default: libfoo.so)
  -o <file>      出力ファイル                      (default: stdout)
  -t <target>    出力言語: php | python | all       (default: all)
  -I <dir>       インクルードディレクトリを追加（繰り返し可）
  -D <macro>     プリプロセッサマクロを定義（繰り返し可）
  --lang <lang>  言語モード: auto | c | c++         (default: auto)
                 auto = .hpp/.hh/.hxx → c++, それ以外 → c
  --main-only    トップレベルのヘッダのみ出力
                 （インクルードされたユーザヘッダを除外）
  --validate     生成コードを cc / python3 で検証
                 結果は stderr に出力、失敗時は exit code 2
  -h, --help     ヘルプを表示
```

### 例

```bash
# PHP FFI コードを生成してファイルに保存
./build/c2ffidef -l libmylib.so -t php -o ffi.php mylib.h

# Python ctypes コードを標準出力に表示
./build/c2ffidef -l libmylib.so -t python mylib.h

# 生成後に構文検証
./build/c2ffidef -l libmylib.so --validate mylib.h > /dev/null

# C++ ヘッダ（extern "C" ブロックを解析）
./build/c2ffidef -l libmylib.so -t python mylib.hpp

# インクルードパスとマクロを指定
./build/c2ffidef -I/usr/local/include -DENABLE_FEATURE -l libmylib.so mylib.h

# インクルードされたヘッダを含めず、トップファイルのみ出力
./build/c2ffidef --main-only -t python mylib.h
```

---

## 出力例

以下のヘッダを入力とします。

```c
// mylib.h
#include <stdint.h>

#define MAX_ITEMS  (64)

typedef struct { float x, y, z; } Vec3;
typedef enum { COLOR_RED=0, COLOR_GREEN=1, COLOR_BLUE=2 } Color;

typedef struct {
    Vec3     points[MAX_ITEMS];   // #define が配列サイズに展開される
    uint32_t count;
} PointBuffer;

Vec3*  vec3_add(const Vec3* a, const Vec3* b);
void   vec3_free(Vec3* v);
int    log_message(const char* fmt, ...);
```

### PHP FFI (`-t php`)

```php
<?php

$ffi = FFI::cdef(<<<'CDEF'
/* Standard integer types */
typedef unsigned char      uint8_t;
typedef unsigned int       uint32_t;
// ...

typedef struct Vec3 Vec3;
typedef struct PointBuffer PointBuffer;

enum Color { COLOR_RED = 0, COLOR_GREEN = 1, COLOR_BLUE = 2 };
typedef enum Color Color;

struct Vec3 { float x; float y; float z; };

struct PointBuffer {
    Vec3     points[64];      /* MAX_ITEMS = 64 に展開 */
    uint32_t count;
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

# enum Color
COLOR_RED = 0
COLOR_GREEN = 1
COLOR_BLUE = 2
Color = ctypes.c_int  # enum

class Vec3(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float), ("z", ctypes.c_float)]

class PointBuffer(ctypes.Structure):
    _fields_ = [
        ("points", Vec3 * 64),    # MAX_ITEMS = 64 に展開
        ("count",  ctypes.c_uint32),
    ]

_lib.vec3_add.argtypes = [ctypes.POINTER(Vec3), ctypes.POINTER(Vec3)]
_lib.vec3_add.restype  = ctypes.POINTER(Vec3)

_lib.vec3_free.argtypes = [ctypes.POINTER(Vec3)]
_lib.vec3_free.restype  = None

_lib.log_message.argtypes = [ctypes.c_char_p]  # variadic
_lib.log_message.restype  = ctypes.c_int
```

---

## `extern "C"` 対応

C++ ライブラリヘッダ（`.hpp`）の `extern "C"` ブロック内の宣言を抽出できます。
C++ の `namespace` / `class` は自動的に無視されます。

```cpp
// mylib.hpp
namespace Internal { class Foo {}; }  // → 無視

extern "C" {
    typedef struct { float x, y; } Vec2;
    Vec2* vec2_add(const Vec2* a, const Vec2* b);  // → 出力される
}

extern "C" int get_version(void);  // 単体指定も対応
```

```bash
./build/c2ffidef -t python mylib.hpp   # 拡張子で自動的に c++ モード
./build/c2ffidef --lang c++ -t python mylib.h  # 明示指定も可能
```

---

## `--validate` オプション

生成されたコードが構文的に正しいかどうかを自動検証します。

| 対象 | 検証方法 |
|------|---------|
| PHP FFI | cdef ブロックの C 宣言を `cc -fsyntax-only` でコンパイル |
| Python ctypes | `ctypes.CDLL(...)` をモックに差し替えて `python3` で実行 |

```bash
./build/c2ffidef --validate mylib.h > /dev/null
# 標準出力: 生成コード
# 標準エラー: 検証結果

# exit code
#   0 = 全 PASS（または検証ツール未インストールで SKIP）
#   2 = 1つ以上 FAIL
```

出力例：

```
[PASS] PHP FFI (C syntax)  (tool: /usr/bin/cc)
[PASS] Python ctypes  (tool: /usr/bin/python3)
```

---

## `#define` 定数の解決

`#define` で定義された定数は libclang のプリプロセッサが評価するため、
算術式・シフト演算を含む複雑な式も正しく展開されます。

```c
#define MAX_SAMPLES  (128)
#define CHANNEL_NUM  (4)
#define FRAME_SIZE   (MAX_SAMPLES * CHANNEL_NUM)  // → 512
#define QUEUE_DEPTH  (1 << 8)                     // → 256

typedef struct {
    float    frames[FRAME_SIZE];   // → float frames[512]
    void*    queue[QUEUE_DEPTH];   // → void* queue[256]
} AudioBuf;
```

---

## 複数ヘッダのインクルード対応

デフォルトでは `#include` で取り込まれた全ユーザヘッダの宣言も出力に含まれます。
システムヘッダ（`<stdint.h>` 等）は常に除外されます。

```
constants.h  ─── #define MAX_SAMPLES, NAME_LEN ...
geometry.h   ─── #include "constants.h"  →  TrackBuffer[MAX_SAMPLES]
audio.h      ─── #include "constants.h"  →  AudioFrame[FRAME_SIZE]
test.h       ─── #include "geometry.h"
             └── #include "audio.h"
```

```bash
# 全ヘッダの宣言を含めて出力（デフォルト）
./build/c2ffidef -t python tests/test.h

# test.h に直接書かれた宣言のみ出力
./build/c2ffidef --main-only -t python tests/test.h
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

| C型 | PHP FFI | Python ctypes |
|-----|---------|---------------|
| `uint8_t` | cdef 先頭で自動 typedef | `ctypes.c_uint8` |
| `int32_t` | cdef 先頭で自動 typedef | `ctypes.c_int32` |
| `size_t` | cdef 先頭で自動 typedef | `ctypes.c_size_t` |
| `int64_t` | cdef 先頭で自動 typedef | `ctypes.c_int64` |

---

## アーキテクチャ

```
src/
├── ast_nodes.hpp       — CType / RecordDecl / FunctionDecl などのAST定義
├── parser.hpp/cpp      — libclang によるヘッダ解析
│                         （言語自動判定・extern "C" 対応・resource-dir 自動検出）
├── generator.hpp       — Generator 基底クラス
├── php_ffi_gen.hpp/cpp — PHP FFI::cdef() コード生成
├── ctypes_gen.hpp/cpp  — Python ctypes コード生成
├── validator.hpp/cpp   — 生成コードの構文検証
└── main.cpp            — CLI エントリポイント

tests/
├── constants.h         — #define 定数群
├── geometry.h          — Vec3 / Particle / TrackBuffer  (constants.h 使用)
├── audio.h             — AudioFrame / AudioChannel      (constants.h 使用)
├── collections.h       — Node / RingQueue / KVPair      (constants.h 使用)
├── test.h              — 上記 4 ファイルを include した総合テスト
└── cpplib.hpp          — extern "C" ブロックを含む C++ ヘッダのテスト
```

---

## 制限事項

- C++ の `namespace` / `class` / `template` は出力されません（`extern "C"` 内のみ対象）
- `__attribute__` 等のコンパイラ拡張は無視される場合があります
- 可変長引数関数の Python ctypes `argtypes` は宣言済み引数のみ出力されます（`...` は省略）
- `union` の Python ctypes 出力は `ctypes.Union` を使用しますが、フィールドが重複する場合は手動調整が必要なことがあります

---

## ライセンス

MIT
