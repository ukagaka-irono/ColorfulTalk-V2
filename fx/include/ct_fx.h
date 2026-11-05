/*
 * ColorfulTalk 固定小数点合成コア（V1 / V2 共通）
 * 整数演算のみ。int が 16bit の CPU でも同じ結果になるよう、幅の決まった型だけを使う。
 * SPDX-License-Identifier: MIT
 */
#ifndef CT_FX_H
#define CT_FX_H

#include <stdint.h>
#include "colorful_talk_common.h"
#include "ct_koe.h"

#define CTFX_FS          8000   /* 出力のサンプリング周波数 */
#define CTFX_FRAME       32     /* 1 フレームのサンプル数（4ms） */
#define CTFX_FRAME_MS    4
/* 1 回に渡せる記号列の長さ（作業領域の大きさが変わる。V1 は既定値、V2 は小さくする） */
#ifndef CTFX_MAX_MORA
#  define CTFX_MAX_MORA    256
#endif
#ifndef CTFX_MAX_PHRASE
#  define CTFX_MAX_PHRASE  64
#endif
#define CTFX_SEGQ        32
#define CTFX_CMDQ        16
#define CTFX_PULSE_N     64
#define CTFX_LOOKAHEAD   200    /* ms */

/* ---- 声（ROM に置く定数データ。V2 では声データファイルの中身になる） -------- */
typedef struct {
    char     name[8];
    uint16_t f0;            /* 基準 F0 [Hz] */
    uint8_t  f0range;       /* 抑揚の幅 ×1/64 */
    uint8_t  whisper;       /* 1 = ささやき声（声帯振動なし） */
    uint16_t fscale;        /* 声道倍率 Q12 */
    uint8_t  b1add;         /* B1 の追加 [Hz] */
    uint8_t  tilt;          /* 音源の高域減衰（1 次ローパス係数 Q8） */
    uint8_t  breath_db;     /* 母音の気息音 [dB]（60 = 母音と同じ） */
    uint8_t  jitter;        /* 周期ゆらぎ ×1/1024 */
    uint16_t gain;          /* 出力ゲイン Q8 */
    uint8_t  te;            /* 声門が閉じる位置 ×1/64 周期 */
    uint8_t  reserved;
    int16_t  pulse[CTFX_PULSE_N]; /* LF モデルの声門体積流 1 周期 Q14 */
} CtFxVoice;

/* ---- 作業領域 ------------------------------------------------------------ */
enum { FX_G_FORM = 0, FX_G_SRC, FX_G_FRIC, FX_G_COUNT };

typedef struct {
    int16_t av, ah, ab;              /* 振幅 Q12 */
    int16_t f[4], b[4];              /* カスケード F1–F4 [Hz] */
    int16_t fnp, bnp, fnz, bnz;      /* 鼻腔 極・零 [Hz] */
    int16_t fr_f[2], fr_b[2], fr_a[2]; /* 摩擦のパラレル共振器 [Hz], 振幅 Q12 */
} CtFxFrame;

typedef struct {
    uint8_t   kind, cons, vowel, aux;
    int8_t    gain;                  /* dB */
    uint8_t   fill_form;
    uint8_t   pre[FX_G_COUNT], post[FX_G_COUNT];  /* 遷移区間 [ms] */
    int32_t   start;                 /* ms */
    int16_t   dur;                   /* ms */
    CtFxFrame tg;
} CtFxSeg;

typedef struct { uint8_t kind; int32_t t0, t1; int16_t amp; } CtFxCmd;  /* amp: log2 Q10 */

typedef struct { int16_t a, b, c; int16_t y1, y2; } CtFxRes;   /* 係数 Q13 */
typedef struct { int32_t a, b, c; uint8_t on; } CtFxAnti;     /* 係数 Q10。入力の履歴は鼻腔の極のものを使う */

typedef struct {
    uint32_t magic;
    const CtFxVoice* v;
    int16_t  err_pos;
    uint16_t csc, vsc, psc;          /* 子音・母音・休止の長さ倍率 Q8 */
    int16_t  r;                      /* 抑揚の幅 Q8 */
    int16_t  fscale;

    CtMora   mora[CTFX_MAX_MORA];
    CtPhrase phrase[CTFX_MAX_PHRASE];
    CtKoe    koe;

    /* 韻律の生成 */
    uint8_t  stage, prev_vowel;
    int16_t  gi;
    int8_t   open_acc;
    int32_t  gen_t, sent_t0, total_ms;
    int16_t  ds, aa;                 /* Q8 / log2 Q10 */

    CtFxSeg  seg[CTFX_SEGQ];
    int16_t  seg_first, seg_end, seg_cur;
    CtFxCmd  cmd[CTFX_CMDQ];
    uint8_t  n_cmd;

    /* フレーム */
    int32_t  frame_k;
    CtFxFrame fa, fb;
    int16_t  micro;                  /* log2 Q10 */
    int16_t  fsm[8];                 /* フォルマントの平滑化 */
    uint16_t inc_a, inc_b;           /* 声帯音源の位相増分（1 周期 = 65536） */
    uint8_t  sub;                    /* フレーム内のサンプル位置 */
    uint8_t  done, cascade_n;

    /* 合成器 */
    uint16_t phase, inc;
    int16_t  amp;                    /* 周期ごとの振幅ゆらぎ Q12 */
    int16_t  tilt_y, asp_lp, prev_y;
    uint16_t lfsr;
    CtFxRes  casc[4], np, par[2];
    CtFxAnti nz;
} CtFxWork;

int ctfx_start(CtFxWork* w, const char* koe_utf8, const CtFxVoice* v, int16_t speed);
int ctfx_read(CtFxWork* w, int16_t* buf, int16_t n);

#endif
