/*
 * cpplib.hpp  –  extern "C" を使った C++ ライブラリヘッダのテスト
 *
 * C++ コードからも C コードからも使えるようにするため、
 * 公開 API を extern "C" {} で囲んでいる典型的なパターン。
 */
#pragma once
#include "constants.h"   // #define MAX_SAMPLES, NAME_LEN, ...
#include <stdint.h>

// ── C++ 専用部分（出力されないことを確認する）──────────────────────────────
#ifdef __cplusplus
#include <cstddef>
namespace cpplib {
    class InternalBuffer {
        float data_[MAX_SAMPLES];
        int   size_;
    };
}
#endif

// ── extern "C" ブロック: 単一ブロック形式 ───────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    data[MAX_SAMPLES];   /* #define MAX_SAMPLES (128) */
    uint32_t length;
    char     name[NAME_LEN];      /* #define NAME_LEN (32)     */
} CBuffer;

typedef enum {
    CLIB_OK       =  0,
    CLIB_ERR_NULL = -1,
    CLIB_ERR_FULL = -2
} CLibStatus;

typedef void (*CLibCallback)(CLibStatus status, void* user_data);

CBuffer*    cbuffer_create(const char* name);
void        cbuffer_destroy(CBuffer* buf);
CLibStatus  cbuffer_push(CBuffer* buf, float value);
float       cbuffer_get(const CBuffer* buf, uint32_t index);
void        cbuffer_set_callback(CLibCallback cb, void* user_data);

#ifdef __cplusplus
}
#endif

// ── extern "C" 個別指定形式 ─────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" int  clib_get_version(void);
extern "C" void clib_reset(void);
#else
int  clib_get_version(void);
void clib_reset(void);
#endif
