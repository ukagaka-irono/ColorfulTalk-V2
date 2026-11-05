cmake_minimum_required(VERSION 3.20)
project(ColorfulTalkV2 VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8 /W3)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  add_compile_options(-Wall -Wextra)
endif()

# V2 は RAM 5KB 以内に収めるため、1 回に渡せる記号列を 160 モーラ・40 アクセント句にする
set(CT2_LIMITS CTFX_MAX_MORA=160 CTFX_MAX_PHRASE=40)

# ---- 共通（カナ記号列のパーサ）と固定小数点コア（V1 と共通） --------------------
add_library(ct_common STATIC common/src/ct_koe.c common/src/ct_wav.c)
target_include_directories(ct_common PUBLIC common/include)

set(FX_SOURCES fx/src/fx_api.c fx/src/fx_math.c fx/src/fx_tables.c fx/src/fx_phoneme.c
               fx/src/fx_prosody.c fx/src/fx_frame.c fx/src/fx_synth.c)
add_library(ct_fx STATIC ${FX_SOURCES})
target_include_directories(ct_fx PUBLIC fx/include PRIVATE fx/src)
target_compile_definitions(ct_fx PUBLIC ${CT2_LIMITS})
target_link_libraries(ct_fx PUBLIC ct_common)

# ---- V2 ------------------------------------------------------------------
set(CT2_SOURCES src/ct2_api.c src/ct2_voice.c)

add_library(colorful_talk_v2 STATIC ${CT2_SOURCES})
target_include_directories(colorful_talk_v2 PUBLIC include PRIVATE src)
target_link_libraries(colorful_talk_v2 PUBLIC ct_fx)
set_target_properties(colorful_talk_v2 PROPERTIES OUTPUT_NAME colorful_talk_v2_static)

add_library(colorful_talk_v2_dll SHARED ${CT2_SOURCES} ${FX_SOURCES} common/src/ct_koe.c common/src/ct_wav.c)
target_include_directories(colorful_talk_v2_dll PUBLIC include PRIVATE src fx/include fx/src common/include)
target_compile_definitions(colorful_talk_v2_dll PRIVATE CT_BUILD_DLL ${CT2_LIMITS})
set_target_properties(colorful_talk_v2_dll PROPERTIES OUTPUT_NAME colorful_talk_v2 C_VISIBILITY_PRESET hidden)

# 同梱の 14 種の声（C の配列。試聴ツールとテスト用）
add_library(ct2_builtin_voices STATIC src/ct2_builtin_voices.c)

# ---- ツール ----------------------------------------------------------------
add_executable(ctalk tools/ctalk.c)
target_link_libraries(ctalk PRIVATE colorful_talk_v2 ct2_builtin_voices)
if(WIN32)
  target_link_libraries(ctalk PRIVATE winmm)
endif()

add_executable(mkvoice tools/mkvoice.c)
if(NOT WIN32)
  target_link_libraries(mkvoice PRIVATE m)
endif()

# 14 種の声データを作り直す: cmake --build build --target voices
add_custom_target(voices
  COMMAND mkvoice --list ${CMAKE_SOURCE_DIR}/voices/voices.csv --outdir ${CMAKE_SOURCE_DIR}/voices
          --c ${CMAKE_SOURCE_DIR}/src/ct2_builtin_voices.c
  DEPENDS mkvoice
  COMMENT "Regenerating voices/*.ctv and src/ct2_builtin_voices.c")

# 合成コアの表を作り直す（開発時のみ）: cmake --build build --target fxgen_run
add_executable(fxgen EXCLUDE_FROM_ALL tools/fxgen.c)
if(NOT WIN32)
  target_link_libraries(fxgen PRIVATE m)
endif()
add_custom_target(fxgen_run COMMAND fxgen ${CMAKE_SOURCE_DIR}/fx/src/fx_tables.c DEPENDS fxgen)

# ---- テスト ----------------------------------------------------------------
enable_testing()
add_executable(test_koe tests/test_koe.c)
target_link_libraries(test_koe PRIVATE ct_common)
add_test(NAME koe COMMAND test_koe)

add_executable(test_v2 tests/test_v2.c tests/ct_test_audio.c)
target_include_directories(test_v2 PRIVATE tests src)
target_link_libraries(test_v2 PRIVATE colorful_talk_v2 ct2_builtin_voices)
add_test(NAME v2 COMMAND test_v2 ${CMAKE_SOURCE_DIR}/voices)
