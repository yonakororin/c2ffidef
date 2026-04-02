/*
 * audio.h  –  音声処理型定義
 * constants.h の定数 (MAX_SAMPLES, CHANNEL_NUM, FRAME_SIZE) を使用
 */
#pragma once
#include "constants.h"
#include <stdint.h>

/* ── チャンネル設定 ──────────────────────────────────────────────────────── */
typedef enum {
    AUDIO_MONO   = 1,
    AUDIO_STEREO = 2,
    AUDIO_QUAD   = 4
} AudioMode;

/* ── 1チャンネル分のサンプルバッファ ────────────────────────────────────── */
typedef struct {
    float    samples[MAX_SAMPLES];  /* #define MAX_SAMPLES (128)          */
    uint32_t sample_rate;
    char     name[NAME_LEN];        /* #define NAME_LEN (32)              */
} AudioChannel;

/* ── インターリーブ済みフレーム: FRAME_SIZE = MAX_SAMPLES * CHANNEL_NUM ── */
typedef struct {
    float     frames[FRAME_SIZE];   /* #define FRAME_SIZE (128 * 4 = 512) */
    uint32_t  channel_mask;
    AudioMode mode;
} AudioFrame;

/* ── バージョン情報 (constants.h の VERSION_* を使用) ────────────────────── */
typedef struct {
    uint8_t major;   /* VERSION_MAJOR = 2 */
    uint8_t minor;   /* VERSION_MINOR = 1 */
    uint8_t patch;   /* VERSION_PATCH = 0 */
    uint8_t reserved;
} AudioVersion;
