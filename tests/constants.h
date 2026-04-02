/*
 * constants.h  –  プロジェクト共通の #define 定数
 * 他のヘッダからincludeされ、配列サイズ等に使用される
 */
#pragma once

/* 基本定数 */
#define MAX_SAMPLES   (128)
#define NAME_LEN      (32)
#define CHANNEL_NUM   (4)

/* 式による定数: コンパイル時に計算される */
#define FRAME_SIZE    (MAX_SAMPLES * CHANNEL_NUM)   /* 128 * 4 = 512  */

/* 2のべき乗 */
#define QUEUE_DEPTH   (1 << 8)                      /* 256             */

/* バージョン情報 */
#define VERSION_MAJOR (2)
#define VERSION_MINOR (1)
#define VERSION_PATCH (0)
