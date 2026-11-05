# 試聴用の WAV を samples/v2 に作る（先に Release ビルドしておくこと）
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ctalk = Join-Path $root 'build\Release\ctalk.exe'
$out = Join-Path $root 'samples\v2'
New-Item -ItemType Directory -Force $out | Out-Null

$text = "わたしわ、からふるとーくです。こえお/きりかえて/はなせま'す。"
foreach ($v in 'f1', 'f2', 'f3', 'f4', 'f5', 'm1', 'm2', 'm3', 'm4', 'c1', 'c2', 'e1', 'w1', 'r1') {
    & $ctalk -v $v -o (Join-Path $out "voice_$v.wav") $text
}
Write-Host "samples written to $out"
