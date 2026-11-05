/* テスト用の音声解析 */
#include <math.h>
#include <stdlib.h>
#include "ct_test_audio.h"

double ta_rms(const int16_t* x, int n)
{
    double s = 0;
    int i;
    for (i = 0; i < n; i++) s += (double)x[i] * x[i];
    return n ? sqrt(s / n) : 0;
}

int ta_clipped(const int16_t* x, int n)
{
    int i, c = 0;
    for (i = 0; i < n; i++) if (x[i] >= 32000 || x[i] <= -32000) c++;
    return c;
}

double ta_f0(const int16_t* x, int n, int fs, int from, int len)
{
    int lag, best = 0, i, lo = fs / 500, hi = fs / 60;
    double bestv = 0, e0 = 0;
    if (from < 0 || from + len + hi > n) return 0;
    for (i = 0; i < len; i++) e0 += (double)x[from + i] * x[from + i];
    for (lag = lo; lag <= hi; lag++) {
        double s = 0, e1 = 0;
        for (i = 0; i < len; i++) {
            s += (double)x[from + i] * x[from + i + lag];
            e1 += (double)x[from + i + lag] * x[from + i + lag];
        }
        s /= sqrt(e0 * e1) + 1e-9;
        if (s > bestv) { bestv = s; best = lag; }
    }
    return best && bestv > 0.5 ? (double)fs / best : 0;
}

void ta_span(const int16_t* x, int n, int thr, int* a, int* b)
{
    int i;
    *a = 0; *b = n;
    for (i = 0; i < n; i++) if (abs(x[i]) > thr) { *a = i; break; }
    for (i = n - 1; i >= 0; i--) if (abs(x[i]) > thr) { *b = i; break; }
}

double ta_f0_max(const int16_t* x, int n, int fs, int from, int to)
{
    int w = fs / 33, p;
    double best = 0;
    for (p = from; p + w <= to; p += w / 3) {
        double f = ta_f0(x, n, fs, p, w);
        if (f > best) best = f;
    }
    return best;
}
