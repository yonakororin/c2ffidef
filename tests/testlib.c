/*
 * testlib.c  –  C ソースファイルからの関数定義抽出テスト
 *
 * 確認ポイント:
 *   - static 関数は出力されないこと
 *   - extern 関数（定義）は出力されること
 *   - インクルードしたヘッダの宣言は出力されないこと（main_file_only 自動有効）
 *   - #define を配列サイズに使った構造体も正しく解決されること
 */
#include "constants.h"   /* #define MAX_SAMPLES, NAME_LEN, FRAME_SIZE ... */
#include "geometry.h"    /* Vec3, Particle, TrackBuffer                    */
#include <stdint.h>
#include <stddef.h>

/* ── ファイル内部専用（FFI に不要）──────────────────────────────────────── */
static float clamp(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static uint32_t fnv1a(const uint8_t* data, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) h = (h ^ data[i]) * 16777619u;
    return h;
}

/* ── 公開 API（FFI で使用する関数）─────────────────────────────────────── */

/* 基本演算 */
int add(int a, int b) { return a + b; }
float scale(float v, float factor) { return v * factor; }

/* Vec3 操作（geometry.h の型を使用）*/
Vec3 vec3_make(float x, float y, float z) {
    Vec3 v; v.x = x; v.y = y; v.z = z; return v;
}

float vec3_length(const Vec3* v) {
    return v->x * v->x + v->y * v->y + v->z * v->z;  /* sqrt 省略 */
}

/* #define MAX_SAMPLES (128) を使った配列サイズの構造体を扱う関数 */
void track_fill(TrackBuffer* buf, float val) {
    for (uint32_t i = 0; i < MAX_SAMPLES; i++) {
        buf->points[i].x = val;
        buf->points[i].y = val;
        buf->points[i].z = val;
    }
    buf->count = MAX_SAMPLES;
}

/* チェックサム計算（内部関数を利用するが、こちらは公開）*/
uint32_t buffer_checksum(const uint8_t* data, uint32_t len) {
    return fnv1a(data, len);
}

/* 可変長引数 */
int log_message(const char* fmt, ...) { (void)fmt; return 0; }

/* void 返り値 */
void reset_all(void) {}
