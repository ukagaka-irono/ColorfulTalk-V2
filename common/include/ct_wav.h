/* PC 向け: PCM の蓄積と WAV（RIFF, 16bit, モノラル）の作成  SPDX-License-Identifier: MIT */
#ifndef CT_WAV_H
#define CT_WAV_H

#include <stdint.h>

typedef struct { int16_t* data; int n, cap; } CtPcm;

int            ct_pcm_append(CtPcm* p, const int16_t* x, int n);   /* 0 = OK */
unsigned char* ct_wav_build(const int16_t* pcm, int n, int fs, int* size); /* malloc */

#endif
