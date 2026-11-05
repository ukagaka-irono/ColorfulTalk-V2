/*
 * AVR（int が 16bit の CPU）で V2 を動かし、出力をシミュレータへ渡すためのファームウェア。
 * サンプルは GPIOR1（下位）・GPIOR2（上位）に書いてから GPIOR0 に 1 を書く。終わりは 2、エラーは 3。
 * 記号列は text.h、声データは voice.h（ビルド時に作る）で与える。
 * SPDX-License-Identifier: MIT
 */
#include <avr/io.h>
#include <stdint.h>
#include "colorful_talk_v2.h"
#include "text.h"
#include "voice.h"

static uint8_t work[4352] __attribute__((aligned(4)));

int main(void)
{
    int16_t buf[32];
    int n, i, err;

    if (ct2_work_size() > sizeof work) { GPIOR1 = 0xFF; GPIOR0 = 3; for (;;) {} }
    err = ct2_start(work, TEXT, VOICE, 100);
    if (err) { GPIOR1 = (uint8_t)err; GPIOR0 = 3; for (;;) {} }
    while ((n = ct2_read(work, buf, 32)) > 0) {
        for (i = 0; i < n; i++) {
            GPIOR1 = (uint8_t)buf[i];
            GPIOR2 = (uint8_t)((uint16_t)buf[i] >> 8);
            GPIOR0 = 1;
        }
    }
    GPIOR0 = 2;
    for (;;) {}
}
