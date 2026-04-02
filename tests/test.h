/*
 * test.h  –  c2ffidef 総合テストヘッダ
 *
 * 複数のユーザヘッダをincludeし、それぞれで定義された型・定数を利用する。
 *
 *   constants.h   ── #define 定数群
 *   geometry.h    ── Vec3 / Particle / TrackBuffer  (constants.h を include)
 *   audio.h       ── AudioFrame / AudioChannel      (constants.h を include)
 *   collections.h ── Node / RingQueue / KVPair      (constants.h を include)
 *
 * 期待する解決:
 *   MAX_SAMPLES  = 128
 *   NAME_LEN     = 32
 *   CHANNEL_NUM  = 4
 *   FRAME_SIZE   = 512   (MAX_SAMPLES * CHANNEL_NUM)
 *   QUEUE_DEPTH  = 256   (1 << 8)
 */
#pragma once
#include "geometry.h"
#include "audio.h"
#include "collections.h"
#include <stdint.h>
#include <stddef.h>

/* ── test.h 独自の型 ─────────────────────────────────────────────────────── */

/* 複数ヘッダの型を組み合わせた構造体 */
typedef struct {
    TrackBuffer  track;     /* geometry.h */
    AudioFrame   audio;     /* audio.h    */
    RingQueue    queue;     /* collections.h */
    uint32_t     flags;
} Scene;

/* ビットフィールド */
typedef struct {
    unsigned int r : 5;
    unsigned int g : 6;
    unsigned int b : 5;
} RGB565;

/* Union */
typedef union {
    int32_t  i;
    float    f;
    uint8_t  bytes[4];
} FloatBits;

/* 関数ポインタ */
typedef void (*Callback)(int event_id, void* user_data);
typedef ResultCode (*ProcessFn)(const AudioFrame* frame, void* ctx);  /* collections.h の ResultCode */

/* ── 関数宣言: geometry.h の型 ───────────────────────────────────────────── */
Vec3*        vec3_add(const Vec3* a, const Vec3* b);
float        vec3_dot(const Vec3* a, const Vec3* b);
void         vec3_free(Vec3* v);
void         mat4_identity(Mat4* m);
TrackBuffer* track_buffer_create(const char* label);
void         track_buffer_destroy(TrackBuffer* buf);
int          track_buffer_push(TrackBuffer* buf, Vec3 point);

/* ── 関数宣言: audio.h の型 ──────────────────────────────────────────────── */
AudioFrame*  audio_frame_alloc(AudioMode mode);
void         audio_frame_free(AudioFrame* frame);
int          audio_frame_mix(AudioFrame* dst, const AudioFrame* src, float gain);
AudioVersion audio_get_version(void);

/* ── 関数宣言: collections.h の型 ────────────────────────────────────────── */
Node*        node_new(int value);
void         node_push(Node** head, int value);
int          node_pop(Node** head);
ResultCode   ring_queue_push(RingQueue* q, void* entry);
void*        ring_queue_pop(RingQueue* q);
ResultCode   kv_lookup(const KVPair* pairs, size_t count,
                       const char* key, uint64_t* out_value);

/* ── 関数宣言: Scene / 汎用 ──────────────────────────────────────────────── */
Scene*       scene_create(void);
void         scene_destroy(Scene* s);
void         set_callback(Callback cb, void* user_data);
void         set_process_fn(ProcessFn fn, void* ctx);
int          log_message(const char* fmt, ...);
void         reset_all(void);
