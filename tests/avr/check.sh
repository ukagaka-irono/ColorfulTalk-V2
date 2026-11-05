#!/usr/bin/env bash
# V2 を AVR（ATmega1284P, int = 16bit）向けにビルドし、avr8js で動かして PC 版と出力を比べる。
#   AVR_GCC=<avr-gcc の bin> AVR8JS=<avr8js を npm install したフォルダ> tests/avr/check.sh "記号列" ...   （声は VOICE=f1 など、voices/ の名前）
# 先に PC 版を Release でビルドしておくこと（build/Release/ctalk.exe か build/ctalk を使う）。
# SPDX-License-Identifier: MIT
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-$ROOT/build/avr}"
: "${AVR_GCC:?set AVR_GCC to the avr-gcc bin directory}"
: "${AVR8JS:?set AVR8JS to a folder where avr8js is installed}"
CTALK="$ROOT/build/Release/ctalk.exe"; [ -x "$CTALK" ] || CTALK="$ROOT/build/ctalk"
mkdir -p "$OUT"
VOICE="${VOICE:-f1}"
# 声データを C の配列にして ROM に置く
{ echo "static const unsigned char VOICE[] = {"; od -An -v -tx1 "$ROOT/voices/$VOICE.ctv" | awk '{ for (i = 1; i <= NF; i++) printf "0x%s,", $i; print "" }'; echo "};"; } > "$OUT/voice.h"
cp "$ROOT/tests/avr/run.mjs" "$AVR8JS/run.mjs"

SRC="$ROOT/common/src/ct_koe.c $ROOT/fx/src/fx_api.c $ROOT/fx/src/fx_math.c $ROOT/fx/src/fx_tables.c
     $ROOT/fx/src/fx_phoneme.c $ROOT/fx/src/fx_prosody.c $ROOT/fx/src/fx_frame.c $ROOT/fx/src/fx_synth.c
     $ROOT/src/ct2_api.c $ROOT/src/ct2_voice.c"
FLAGS="-mmcu=atmega1284p -Os -std=c99 -Wall -Wextra -DCT_NO_WAV -DCTFX_MAX_MORA=160 -DCTFX_MAX_PHRASE=40
       -I$ROOT/include -I$ROOT/src -I$ROOT/fx/include -I$ROOT/fx/src -I$ROOT/common/include -I$OUT"

# エンジンだけのサイズ（ファームウェアの main を除く）
"$AVR_GCC/avr-gcc" $FLAGS -c $SRC
mv ./*.o "$OUT/"
"$AVR_GCC/avr-size" -t "$OUT"/*.o | tail -1 | awk '{printf "engine: text %d bytes, data %d bytes, bss %d bytes\n", $1, $2, $3}'

status=0
i=0
for TEXT in "$@"; do
  i=$((i + 1))
  printf '#define TEXT "%s"\n' "$TEXT" > "$OUT/text.h"
  "$AVR_GCC/avr-gcc" $FLAGS -o "$OUT/v2.elf" "$ROOT/tests/avr/avr_main.c" $SRC
  "$AVR_GCC/avr-objcopy" -O ihex -R .eeprom "$OUT/v2.elf" "$OUT/v2.hex"
  STATS=$(cd "$AVR8JS" && node run.mjs "$OUT/v2.hex" "$OUT/avr_$i.raw")
  "$CTALK" -v "$VOICE" -o "$OUT/pc_$i.wav" "$TEXT"
  tail -c +45 "$OUT/pc_$i.wav" > "$OUT/pc_$i.raw"
  if cmp -s "$OUT/pc_$i.raw" "$OUT/avr_$i.raw"; then same=identical; else same=DIFFERENT; status=1; fi
  echo "[$i] $same  $STATS  $TEXT"
done
"$AVR_GCC/avr-size" "$OUT/v2.elf" | tail -1 | awk '{printf "firmware: text %d, data %d, bss %d bytes\n", $1, $2, $3}'
exit $status
