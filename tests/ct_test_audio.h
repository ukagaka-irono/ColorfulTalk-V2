/* テスト用の音声解析（全エンジン共通） */
#ifndef CT_TEST_AUDIO_H
#define CT_TEST_AUDIO_H

#include <stdint.h>

double ta_rms(const int16_t* x, int n);
int    ta_clipped(const int16_t* x, int n);
/* 自己相関による F0 推定。推定できなければ 0 */
double ta_f0(const int16_t* x, int n, int fs, int from, int len);
/* 振幅がしきい値を超える最初と最後 */
void   ta_span(const int16_t* x, int n, int thr, int* a, int* b);
/* 区間 [from, to) を小窓で走査した F0 の最大・最小 */
double ta_f0_max(const int16_t* x, int n, int fs, int from, int to);

#endif
