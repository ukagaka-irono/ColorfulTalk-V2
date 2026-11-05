/*
 * mkvoice: ColorfulTalk V2 の声データ（.ctv）を作る。
 *
 *   1 つ作る:
 *     mkvoice -o myvoice.ctv --name myvoice --f0 220 --formant 1.15 --rd 1.3 [--range 1.0]
 *             [--breath 1.0] [--jitter 1.0] [--whisper] [--level 1.0]
 *   一覧からまとめて作る（同梱の 14 種はこれで作る）:
 *     mkvoice --list voices/voices.csv --outdir voices [--c src/ct2_builtin_voices.c]
 *
 * パラメータ
 *   --f0       声の高さの基準 [Hz]（60〜500）
 *   --range    抑揚の幅（1 = 標準、0〜3）
 *   --formant  声道の長さの倍率。大きいほど小柄な声（0.7〜1.5）
 *   --rd       声帯音源の Rd。小さいほど張った声、大きいほど息っぽい（0.3〜2.7）
 *   --breath   気息音の量（1 = 標準、0〜3）
 *   --jitter   声のゆらぎ（1 = 標準、0〜3）
 *   --whisper  ささやき声（声帯を振動させない）
 *   --level    音量の補正（1 = 計算どおり）
 *
 * PC 上で動かす開発用ツール（浮動小数点を使う）。
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define PULSE_N 64
#define HEADER 64
#define BASIC 20
#define TOTAL (HEADER + BASIC + 2 * PULSE_N)

static long rnd(double x) { return (long)floor(x + 0.5); }

/* ---- LF モデル（ColorfulTalk V3 と同じ式） ------------------------------- */
static double lf_area(double alpha, double tp, double te, double ta, double eps)
{
    double wg = PI / tp, D = 1.0 - te;
    double s = sin(wg * te), ex = exp(alpha * te);
    double E0 = -1.0 / (ex * s);
    double open = E0 * (ex * (alpha * s - wg * cos(wg * te)) + wg) / (alpha * alpha + wg * wg);
    double ret = -(1.0 / (eps * ta)) * ((1.0 - exp(-eps * D)) / eps - D * exp(-eps * D));
    return open + ret;
}

static void lf_pulse(double rd, double* out, int n, double* te_out)
{
    double Rd = rd < 0.3 ? 0.3 : rd > 2.7 ? 2.7 : rd;
    double Rap = (-1.0 + 4.8 * Rd) / 100.0, Rkp = (22.4 + 11.8 * Rd) / 100.0;
    double Rgp = 0.25 * Rkp / (0.11 * Rd / (0.5 + 1.2 * Rkp) - Rap);
    double tp = 0.5 / Rgp, te = tp * (1.0 + Rkp), ta = Rap, D, eps, alpha = 0, lo, hi, flo, wg, E0, acc, peak;
    int k, it;
    if (te > 0.95) te = 0.95;
    if (ta < 1e-3) ta = 1e-3;
    D = 1.0 - te;
    eps = 1.0 / ta;
    for (it = 0; it < 60; it++) eps = (1.0 - exp(-eps * D)) / ta;
    lo = -20.0;
    flo = lf_area(lo, tp, te, ta, eps);
    for (hi = -19.0; hi <= 200.0; hi += 1.0) {
        double fh = lf_area(hi, tp, te, ta, eps);
        if ((fh > 0) != (flo > 0)) {
            for (it = 0; it < 60; it++) {
                double mid = 0.5 * (lo + hi), fm = lf_area(mid, tp, te, ta, eps);
                if ((fm > 0) == (flo > 0)) { lo = mid; flo = fm; } else hi = mid;
            }
            alpha = 0.5 * (lo + hi);
            break;
        }
        lo = hi;
        flo = fh;
    }
    wg = PI / tp;
    E0 = -1.0 / (exp(alpha * te) * sin(wg * te));
    acc = 0;
    peak = 1e-12;
    for (k = 0; k < n; k++) {
        double t = (k + 0.5) / n, e;
        if (t <= te) e = E0 * exp(alpha * t) * sin(wg * t);
        else e = -(1.0 / (eps * ta)) * (exp(-eps * (t - te)) - exp(-eps * D));
        acc += e / n;
        out[k] = acc;
    }
    for (k = 0; k < n; k++) {
        out[k] -= acc * (k + 1) / n;
        if (out[k] > peak) peak = out[k];
    }
    for (k = 0; k < n; k++) out[k] = out[k] < 0 ? 0 : out[k] / peak;
    *te_out = te;
}

static double slope_energy(double rd)
{
    double p[256], te, e = 0;
    int k;
    lf_pulse(rd, p, 256, &te);
    for (k = 1; k < 256; k++) e += (p[k] - p[k - 1]) * (p[k] - p[k - 1]);
    return e;
}

/* ---- 声データ ------------------------------------------------------------ */
typedef struct {
    char   name[16];
    char   desc[64];
    double f0, range, formant, rd, breath, jitter, level;
    int    whisper;
} VoiceDef;

static void put16(unsigned char* p, long v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char* p, unsigned long v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

static unsigned long crc32(const unsigned char* p, size_t n)
{
    unsigned long crc = 0xFFFFFFFFUL;
    size_t i;
    int k;
    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
    return ~crc & 0xFFFFFFFFUL;
}

static int check_def(const VoiceDef* d)
{
    if (d->f0 < 60 || d->f0 > 500) return fprintf(stderr, "mkvoice: %s: f0 must be 60-500\n", d->name), 0;
    if (d->range < 0 || d->range > 3) return fprintf(stderr, "mkvoice: %s: range must be 0-3\n", d->name), 0;
    if (d->formant < 0.7 || d->formant > 1.5) return fprintf(stderr, "mkvoice: %s: formant must be 0.7-1.5\n", d->name), 0;
    if (d->rd < 0.3 || d->rd > 2.7) return fprintf(stderr, "mkvoice: %s: rd must be 0.3-2.7\n", d->name), 0;
    if (d->breath < 0 || d->breath > 3 || d->jitter < 0 || d->jitter > 3)
        return fprintf(stderr, "mkvoice: %s: breath/jitter must be 0-3\n", d->name), 0;
    if (d->level <= 0 || d->level > 4) return fprintf(stderr, "mkvoice: %s: level must be 0-4\n", d->name), 0;
    if (!d->name[0]) return fprintf(stderr, "mkvoice: name is required\n"), 0;
    return 1;
}

static void build(const VoiceDef* d, unsigned char* out)
{
    double p[PULSE_N], te, tilt_db, tilt_a, gain, breath_db;
    unsigned char* b = out + HEADER;
    int k;

    lf_pulse(d->rd, p, PULSE_N, &te);
    tilt_db = d->rd > 0.6 ? 6.0 * (d->rd - 0.6) : 0.0;
    tilt_a = tilt_db >= 25 ? 0.85 : tilt_db / 25.0 * 0.85;
    gain = pow(210.0 / d->f0, 1.1) * pow(slope_energy(1.3) / slope_energy(d->rd), 0.8) * pow(d->rd / 1.3, 0.4);
    if (d->formant > 1.15) gain *= pow(1.15 / d->formant, 1.5);
    if (d->whisper) gain = 1.0;
    gain *= d->level;
    breath_db = d->breath > 0 ? 40.0 + 20.0 * log10(d->breath) : 0.0;
    if (breath_db < 0) breath_db = 0;

    memset(out, 0, TOTAL);
    memcpy(out, "CTV2", 4);
    put16(out + 4, 1);
    put16(out + 6, HEADER);
    put32(out + 8, TOTAL);
    strncpy((char*)out + 16, d->name, 15);
    put16(out + 32, HEADER);
    put16(out + 34, HEADER + BASIC);

    put16(b + 0, rnd(d->f0));
    b[2] = (unsigned char)rnd(64 * d->range);
    b[3] = (unsigned char)(d->whisper ? 1 : 0);
    put16(b + 4, rnd(4096 * d->formant));
    b[6] = (unsigned char)rnd(d->formant > 1.0 ? 130.0 * (d->formant - 1.0) : 0.0);
    b[7] = (unsigned char)rnd(256 * tilt_a);
    b[8] = (unsigned char)rnd(breath_db);
    b[9] = (unsigned char)rnd(10.24 * d->jitter);
    put16(b + 10, rnd(256 * gain));
    b[12] = (unsigned char)rnd(64 * te);
    b[13] = PULSE_N;
    for (k = 0; k < PULSE_N; k++) put16(out + HEADER + BASIC + 2 * k, rnd(16384 * p[k]));
    put32(out + 12, crc32(out, TOTAL));
}

static int write_file(const char* path, const unsigned char* data, size_t n)
{
    FILE* f = fopen(path, "wb");
    if (!f || fwrite(data, 1, n, f) != n) {
        fprintf(stderr, "mkvoice: cannot write %s\n", path);
        if (f) fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

static void write_c(FILE* f, const VoiceDef* d, const unsigned char* data)
{
    int k;
    fprintf(f, "/* %s: %s */\nconst unsigned char CT2_VOICE_%s[%d] = {", d->name, d->desc, d->name, TOTAL);
    for (k = 0; k < TOTAL; k++) fprintf(f, "%s0x%02X%s", k % 16 ? " " : "\n    ", data[k], k < TOTAL - 1 ? "," : "");
    fprintf(f, "\n};\n\n");
}

static void usage(void)
{
    fputs("usage: mkvoice -o FILE.ctv --name NAME --f0 HZ --formant X --rd X [--range X] [--breath X]\n"
          "               [--jitter X] [--whisper] [--level X]\n"
          "       mkvoice --list voices.csv --outdir DIR [--c FILE.c]\n", stderr);
}

/* 一覧: 名前,説明,f0,range,formant,rd,breath,jitter,whisper,level（# で始まる行と見出しは読み飛ばす） */
static int run_list(const char* list, const char* outdir, const char* cfile)
{
    FILE* f = fopen(list, "rb");
    FILE* c = NULL;
    char line[512];
    char names[32][16];
    int n = 0;
    if (!f) { fprintf(stderr, "mkvoice: cannot read %s\n", list); return 1; }
    if (cfile) {
        c = fopen(cfile, "wb");
        if (!c) { fprintf(stderr, "mkvoice: cannot write %s\n", cfile); fclose(f); return 1; }
        fprintf(c, "/* 生成ファイル（tools/mkvoice.c --list）。手で編集しないこと。  SPDX-License-Identifier: MIT */\n\n");
    }
    while (fgets(line, sizeof line, f)) {
        VoiceDef d;
        unsigned char data[TOTAL];
        char path[600];
        memset(&d, 0, sizeof d);
        if (line[0] == '#' || !strncmp(line, "name,", 5) || line[0] == '\r' || line[0] == '\n') continue;
        if (sscanf(line, "%15[^,],%63[^,],%lf,%lf,%lf,%lf,%lf,%lf,%d,%lf", d.name, d.desc, &d.f0, &d.range,
                   &d.formant, &d.rd, &d.breath, &d.jitter, &d.whisper, &d.level) != 10) {
            fprintf(stderr, "mkvoice: bad line: %s", line);
            fclose(f);
            if (c) fclose(c);
            return 1;
        }
        if (!check_def(&d)) { fclose(f); if (c) fclose(c); return 1; }
        build(&d, data);
        sprintf(path, "%s/%s.ctv", outdir, d.name);
        if (!write_file(path, data, TOTAL)) { fclose(f); if (c) fclose(c); return 1; }
        if (c) write_c(c, &d, data);
        if (n < 32) strcpy(names[n], d.name);
        n++;
    }
    fclose(f);
    if (c) {
        int k;
        fprintf(c, "/* 一覧（試聴ツールとテスト用） */\nconst unsigned char* const CT2_BUILTIN_VOICES[%d] = {", n);
        for (k = 0; k < n && k < 32; k++) fprintf(c, "%sCT2_VOICE_%s", k ? ", " : " ", names[k]);
        fprintf(c, " };\nconst char* const CT2_BUILTIN_NAMES[%d] = {", n);
        for (k = 0; k < n && k < 32; k++) fprintf(c, "%s\"%s\"", k ? ", " : " ", names[k]);
        fprintf(c, " };\nconst int CT2_BUILTIN_COUNT = %d;\n", n);
        fclose(c);
    }
    fprintf(stderr, "mkvoice: wrote %d voices\n", n);
    return 0;
}

int main(int argc, char** argv)
{
    VoiceDef d;
    unsigned char data[TOTAL];
    const char *out = NULL, *list = NULL, *outdir = ".", *cfile = NULL;
    int i;

    memset(&d, 0, sizeof d);
    d.range = 1.0; d.breath = 1.0; d.jitter = 1.0; d.level = 1.0; d.formant = 1.0; d.rd = 1.3;
    strcpy(d.desc, "custom voice");
    for (i = 1; i < argc; i++) {
        const char* a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--whisper")) { d.whisper = 1; continue; }
        if (!v) { usage(); return 2; }
        i++;
        if (!strcmp(a, "-o")) out = v;
        else if (!strcmp(a, "--list")) list = v;
        else if (!strcmp(a, "--outdir")) outdir = v;
        else if (!strcmp(a, "--c")) cfile = v;
        else if (!strcmp(a, "--name")) { strncpy(d.name, v, 15); }
        else if (!strcmp(a, "--f0")) d.f0 = atof(v);
        else if (!strcmp(a, "--range")) d.range = atof(v);
        else if (!strcmp(a, "--formant")) d.formant = atof(v);
        else if (!strcmp(a, "--rd")) d.rd = atof(v);
        else if (!strcmp(a, "--breath")) d.breath = atof(v);
        else if (!strcmp(a, "--jitter")) d.jitter = atof(v);
        else if (!strcmp(a, "--level")) d.level = atof(v);
        else { usage(); return 2; }
    }
    if (list) return run_list(list, outdir, cfile);
    if (!out) { usage(); return 2; }
    if (!check_def(&d)) return 1;
    build(&d, data);
    return write_file(out, data, TOTAL) ? 0 : 1;
}
