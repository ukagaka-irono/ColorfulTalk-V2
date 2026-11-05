/* ColorfulTalk 固定小数点コア 内部定義  SPDX-License-Identifier: MIT */
#ifndef FX_INTERNAL_H
#define FX_INTERNAL_H

#include "ct_fx.h"

#define FX_MAGIC 0x43544658UL

/* セグメントの種類（V3 と同じ） */
enum { S_SIL = 0, S_VOWEL, S_DEVVOWEL, S_CLOSURE, S_VBAR, S_BURST, S_ASP, S_FRIC, S_NASAL, S_FLAP, S_GLIDE };
enum { NP_M = 0, NP_N, NP_NG, NP_UVULAR };
enum { F0_PHRASE = 0, F0_ACCENT };

/* 生成した表（fx_tables.c） */
extern const int16_t  FX_COS[257];
extern const int16_t  FX_EXPBW[257];
extern const int16_t  FX_DB[73];
extern const uint16_t FX_EXP2[257];
extern const int16_t  FX_GP[256];
extern const int16_t  FX_GA[101];
extern const int16_t  FX_SIN[64];
extern const uint16_t FX_CSC[26];
extern const uint16_t FX_VSC[26];

/* fx_math.c */
int16_t fx_db(int16_t db);                       /* dB → Q12 */
int16_t fx_cos_hz(int16_t hz);                   /* cos(2πf/fs) Q14 */
int16_t fx_exp_bw(int16_t bw);                   /* exp(-π bw/fs) Q14 */
uint16_t fx_speed_scale(const uint16_t* t, int16_t speed);
int16_t fx_gp(int32_t t_ms);                     /* Q12 */
int16_t fx_ga(int32_t t_ms);                     /* Q12 */
uint16_t fx_hz_from_log2(uint16_t base_hz, int16_t log2q10);   /* base × 2^(x/1024) */
uint16_t fx_isqrt32(uint32_t x);
int16_t fx_smooth256(int32_t num, int32_t den);  /* smoothstep(num/den) Q8 */

/* fx_phoneme.c */
void fx_seg_target(const CtFxSeg* s, const CtFxVoice* v, int16_t fscale, CtFxFrame* out);
/* fx_prosody.c */
void fx_gen_fill(CtFxWork* w, int32_t t_play);
/* fx_frame.c */
void fx_seg_link(CtFxWork* w, int16_t ai);
void fx_make_frame(CtFxWork* w, int32_t k, CtFxFrame* out, uint16_t* inc);
/* fx_synth.c */
void fx_synth_reset(CtFxWork* w);
int16_t fx_synth_run(CtFxWork* w, int16_t* buf, int16_t n);

#define FX_SEG(w, i) (&(w)->seg[(uint16_t)(i) % CTFX_SEGQ])

#endif
