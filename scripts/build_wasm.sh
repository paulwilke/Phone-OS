#!/bin/sh
# Builds the simulator for the browser (docs/simulator.md, D-58) into out/wasm/: phone.js (the loader), phone.wasm
# and phone.data (the card sim/data and the catalogs lang/). scripts/make_site.py puts them into the page.
# Needs Emscripten (brew install emscripten; in CI: the emsdk of .github/workflows/pages.yml). Nothing is committed.
set -eu
cd "$(dirname "$0")/.."
mkdir -p out/wasm
# No threads (a browser page needs no special headers for that), no socket (the parents' page is not part of this).
# Warnings are shown, not errors: clang warns about a private field of sim/SocketTcp.h that the native build does not.
em++ -std=c++17 -O2 -Wall -Wextra -Isrc -Itesting -Isim \
  $(find src -name '*.cpp' | sort) sim/wasm_main.cpp \
  -o out/wasm/phone.js \
  --preload-file sim/data@/data --preload-file lang@/lang --exclude-file 'lang/web/*' \
  -sMODULARIZE=1 -sEXPORT_NAME=PhoneSim -sENVIRONMENT=web \
  -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=0 -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8
