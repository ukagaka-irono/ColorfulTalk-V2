/*
 * ColorfulTalk V2 — 声データを差し替えて声を切り替える日本語音声合成エンジン
 *
 * 入力: カナ＋アクセント記号の記号列（UTF-8, docs/koe-kana.md）
 * 声  : 声データ（.ctv, 16KB 以下）をポインタで渡す。ROM・フラッシュ・RAM のどこに置いてもよい
 * 出力: 16bit 符号付き PCM, モノラル, 8kHz
 * 整数演算のみ。16bit CPU から動く。
 * SPDX-License-Identifier: MIT
 */
#ifndef COLORFUL_TALK_V2_H
#define COLORFUL_TALK_V2_H

#include <stddef.h>
#include <stdint.h>
#include "colorful_talk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CT2_VERSION        "2.0.0"
#define CT2_SAMPLE_RATE    8000
#define CT2_VOICE_MAX_SIZE 16384   /* 声データの上限 */

/* 声データを検査する（形式・版・大きさ・CRC）。正しければ CT_OK、不正なら CT_ERR_VOICE */
CT_API int ct2_voice_check(const void* voice, size_t size);

/* 声データに入っている声の名前（最大 15 文字）を name に書く。name は 16 バイト以上 */
CT_API int ct2_voice_name(const void* voice, char* name);

/* ---- ストリーミング API（動的メモリ不使用） ----------------------------
 *   static uint8_t work[...];   // ct2_work_size() バイト以上, 4 バイト境界
 *   ct2_start(work, koe, voice, 100);
 *   while ((n = ct2_read(work, buf, 64)) > 0) { 再生(buf, n); }
 * 声データは ct2_start の中で検査して作業領域に写すので、ct2_start の後は捨ててよい。
 */
CT_API size_t ct2_work_size(void);
CT_API int    ct2_start(void* work, const char* koe_utf8, const void* voice, int speed); /* speed 50〜300 */
CT_API int    ct2_read(void* work, int16_t* buf, int n);                                 /* 0 で終了 */
CT_API int    ct2_error_pos(const void* work);

/* ---- PC 向けの便利関数（malloc を使う。CT_NO_WAV で外せる） -------------- */
CT_API unsigned char* ct2_synthe_wav(const char* koe_utf8, const void* voice, int speed, int* size, int* err);
CT_API void           ct2_free(void* buf);

#ifdef __cplusplus
}
#endif

#endif
