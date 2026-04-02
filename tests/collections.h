/*
 * collections.h  –  汎用データ構造
 * constants.h の QUEUE_DEPTH を使用
 */
#pragma once
#include "constants.h"
#include <stdint.h>

/* ── 前方宣言を伴う自己参照リスト ────────────────────────────────────────── */
struct Node;

typedef struct Node {
    int          value;
    struct Node* next;
} Node;

/* ── 固定長リングキュー: QUEUE_DEPTH = 1 << 8 = 256 ─────────────────────── */
typedef struct {
    void*    entries[QUEUE_DEPTH];  /* #define QUEUE_DEPTH (1 << 8 = 256) */
    uint32_t head;
    uint32_t tail;
    uint32_t size;
} RingQueue;

/* ── キー・バリューペア ──────────────────────────────────────────────────── */
typedef struct {
    char     key[NAME_LEN];         /* #define NAME_LEN (32)              */
    uint64_t value;
} KVPair;

/* ── 結果コード ──────────────────────────────────────────────────────────── */
typedef enum {
    RESULT_OK    =  0,
    RESULT_ERROR = -1,
    RESULT_FULL  = -2,
    RESULT_EMPTY = -3
} ResultCode;
