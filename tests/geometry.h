/*
 * geometry.h  –  幾何型定義
 * constants.h の定数を配列サイズに使用
 */
#pragma once
#include "constants.h"
#include <stdint.h>

/* ── 基本ベクタ ──────────────────────────────────────────────────────────── */
typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} Vec4;

/* ── 色 ──────────────────────────────────────────────────────────────────── */
typedef enum {
    COLOR_RED   = 0,
    COLOR_GREEN = 1,
    COLOR_BLUE  = 2,
    COLOR_ALPHA = 3
} ColorChannel;

/* ── 変換行列: NAME_LEN ではなく固定 4x4 ────────────────────────────────── */
typedef struct {
    float m[4][4];
} Mat4;

/* ── 軌跡バッファ: MAX_SAMPLES 点分のサンプルを保持 ──────────────────────── */
typedef struct {
    Vec3     points[MAX_SAMPLES];   /* #define MAX_SAMPLES (128) */
    uint32_t count;
    char     label[NAME_LEN];       /* #define NAME_LEN (32)     */
} TrackBuffer;

/* ── 粒子 ─────────────────────────────────────────────────────────────────── */
typedef struct {
    Vec3         position;
    Vec3         velocity;
    float        mass;
    ColorChannel color;
} Particle;
