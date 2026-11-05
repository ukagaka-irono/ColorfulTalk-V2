/*
 * カナ記号列のパーサ（V1 / V2 / V3 共通、整数のみ・動的メモリ不使用）
 * SPDX-License-Identifier: MIT
 */
#ifndef CT_KOE_H
#define CT_KOE_H

#include <stdint.h>
#include "colorful_talk_common.h"

/* 子音 */
enum { C_NONE = 0, C_K, C_G, C_S, C_SH, C_Z, C_J, C_T, C_D, C_TS, C_CH,
       C_N, C_H, C_HY, C_F, C_B, C_P, C_M, C_R, C_COUNT };
/* 半母音 */
enum { G_NONE = 0, G_Y, G_W };
/* 母音 */
enum { V_NONE = 0, V_A, V_I, V_U, V_E, V_O };
/* モーラの種類: 通常 / 撥音 / 促音 / 長音 */
enum { M_NORMAL = 0, M_N, M_Q, M_LONG };
/* 句の終わり方: 区切りのみ / 読点 / 句点 / 疑問 */
enum { PE_NONE = 0, PE_COMMA, PE_PERIOD, PE_QUESTION };
/* 調音様式・位置 */
enum { MN_NONE = 0, MN_STOP, MN_FRIC, MN_AFFR, MN_NASAL, MN_FLAP };
enum { PL_NONE = 0, PL_LAB, PL_ALV, PL_PAL, PL_VEL, PL_GLOT };

typedef struct {
    uint8_t  type, cons, glide, vowel;
    uint8_t  devoice;   /* 1 = 母音無声化 */
    uint8_t  forced;    /* '_' で明示 */
    uint16_t phrase;    /* 所属アクセント句 */
} CtMora;

typedef struct {
    uint16_t first, count;  /* モーラ範囲 */
    uint8_t  accent;        /* 0 = 平板, k = k モーラ目の後で下がる */
    uint8_t  end;           /* PE_* */
} CtPhrase;

typedef struct {
    CtMora*   mora;   uint16_t max_mora,   n_mora;
    CtPhrase* phrase; uint16_t max_phrase, n_phrase;
} CtKoe;

/* 記号列を解析する。失敗時は *err_pos に文字位置（0 始まり、文字単位）を入れる */
int ct_koe_parse(const char* utf8, CtKoe* k, int* err_pos);

/* 子音の情報: manner | place << 3 | voiced << 6 */
uint8_t ct_cons_info(int cons);
#define CT_MANNER(i) ((i) & 7)
#define CT_PLACE(i)  (((i) >> 3) & 7)
#define CT_VOICED(i) (((i) >> 6) & 1)

#endif
