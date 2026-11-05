/* ColorfulTalk V2: 声データ（.ctv）の形式  SPDX-License-Identifier: MIT */
#ifndef CT2_VOICE_H
#define CT2_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "ct_fx.h"

/*
 * 声データ（.ctv）の並び（数値はリトルエンディアン）
 *
 * ヘッダ（64 バイト）
 *   0  char[4]   マジック "CTV2"
 *   4  uint16    形式の版（1）
 *   6  uint16    ヘッダの大きさ（64）
 *   8  uint32    ファイル全体の大きさ
 *  12  uint32    CRC-32（この 4 バイトを 0 として全体から計算）
 *  16  char[16]  声の名前（ASCII, NUL 埋め）
 *  32  uint16×8  ブロックの位置（0 = なし）。[0] 声の基本、[1] 音源
 *  48  uint8×16  予約（0）
 *
 * ブロック 0: 声の基本（20 バイト）
 *   0 uint16 基準 F0 [Hz]        2 uint8 抑揚の幅 ×1/64     3 uint8 ささやき声 (0/1)
 *   4 uint16 声道倍率 Q12        6 uint8 B1 の追加 [Hz]     7 uint8 音源の高域減衰 Q8
 *   8 uint8  気息音 [dB]         9 uint8 ゆらぎ ×1/1024    10 uint16 出力ゲイン Q8
 *  12 uint8  声門が閉じる位置 ×1/64 周期                    13 uint8 音源の点数（64）
 *  14 uint8×6 予約
 *
 * ブロック 1: 音源（int16 × 音源の点数）LF モデルの声門体積流 1 周期 Q14
 */
#define CTV_MAGIC0 'C'
#define CTV_MAGIC1 'T'
#define CTV_MAGIC2 'V'
#define CTV_MAGIC3 '2'
#define CTV_FORMAT 1
#define CTV_HEADER 64
#define CTV_BASIC_SIZE 20
#define CTV_MAX_BLOCKS 8

uint32_t ct2_crc32(const uint8_t* p, size_t n, size_t skip_at);
/* 声データを検査して CtFxVoice に写す。size が 0 ならヘッダの大きさを信じる */
int ct2_voice_load(const void* voice, size_t size, CtFxVoice* out);

#endif
