/*
 * testlib.cpp  –  C++ ソースファイルからの関数定義抽出テスト
 *
 * 確認ポイント:
 *   - extern "C" ブロック内の定義だけが出力されること
 *   - C++ クラス・namespace・static は出力されないこと
 *   - extern "C" 単体指定の関数も出力されること
 *   - #define 定数を使った構造体フィールドも正しく解決されること
 */
#include "constants.h"   /* #define MAX_SAMPLES, NAME_LEN, FRAME_SIZE ... */
#include "audio.h"       /* AudioFrame, AudioChannel, AudioMode            */
#include <stdint.h>
#include <cstring>

/* ── C++ 専用（FFI に不要・出力されないこと）────────────────────────────── */
namespace audio_internal {
    class Processor {
        float gain_ = 1.0f;
    public:
        float process(float s) { return s * gain_; }
    };
}

static void cpp_internal_reset() { /* static: 出力されない */ }

template<typename T>
T cpp_clamp(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ── extern "C" ブロック: まとめて公開 ──────────────────────────────────── */
extern "C" {

/* AudioFrame 操作（audio.h の型 + FRAME_SIZE = 512 を使用）*/
AudioFrame* audio_frame_create(AudioMode mode) {
    (void)mode; return nullptr;
}

void audio_frame_destroy(AudioFrame* frame) { (void)frame; }

int audio_frame_mix(AudioFrame* dst, const AudioFrame* src, float gain) {
    (void)dst; (void)src; (void)gain; return 0;
}

/* チャンネル操作（MAX_SAMPLES = 128 を使用）*/
void audio_channel_clear(AudioChannel* ch) {
    if (ch) std::memset(ch->samples, 0, sizeof(ch->samples));
}

float audio_channel_peak(const AudioChannel* ch) {
    if (!ch) return 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < MAX_SAMPLES; i++) {
        float v = ch->samples[i];
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    return peak;
}

} /* extern "C" */

/* ── extern "C" 単体指定 ─────────────────────────────────────────────────── */
extern "C" int audio_get_version(void) { return 1; }
extern "C" void audio_reset(void) {}

/* ── 通常の C++ 関数（出力されないこと）─────────────────────────────────── */
float cpp_only_function(float x) { return x * 2.0f; }
