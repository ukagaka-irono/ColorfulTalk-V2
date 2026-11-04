# エラーコード（全製品共通）

ヘッダ `colorful_talk_common.h` で定義します。値は製品をまたいで同じで、今後も変えません（追加は末尾のみ）。

| 値 | 定数 | 意味 | 主に返す関数 |
|---|---|---|---|
| 0 | `CT_OK` | 成功 | すべて |
| 1 | `CT_ERR_NULL` | 引数が NULL | すべて |
| 2 | `CT_ERR_UTF8` | 不正な UTF-8 | start / text_to_koe |
| 3 | `CT_ERR_CHAR` | 記号列に使えない文字 | start |
| 4 | `CT_ERR_ACCENT` | アクセント記号の位置が不正 | start |
| 5 | `CT_ERR_LONG` | 長音記号の位置が不正 | start |
| 6 | `CT_ERR_DEVOICE` | 無声化記号の位置が不正 | start |
| 7 | `CT_ERR_EMPTY` | 発音する文字がない | start |
| 8 | `CT_ERR_PARAM` | パラメータが範囲外 | start / preset |
| 9 | `CT_ERR_TOOLONG` | 記号列が長すぎる | start |
| 10 | `CT_ERR_MEMORY` | メモリ不足（PC 向けの便利関数のみ） | synthe_wav など |
| 11 | `CT_ERR_VOICE` | 声データが不正（V2） | ct2_start / ct2_voice_check |
| 12 | `CT_ERR_IO` | ファイルを読めない（ColorfulYomi） | cy_open |
| 13 | `CT_ERR_DIC` | 辞書の形式が違う・壊れている（ColorfulYomi） | cy_open |
| 14 | `CT_ERR_BUFFER` | 出力先のバッファが足りない（ColorfulYomi） | cy_text_to_koe |

記号列のエラー（2〜7, 9）では、原因の文字位置（0 始まり、UTF-8 のバイトではなく文字単位）を取得できます。
V3 では `ct3_error_pos()` です。
