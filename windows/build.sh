#!/usr/bin/env bash
# Tendedero for Windows — cross-build from Linux (mingw-w64) or native MSYS2.
set -e
cd "$(dirname "$0")"
CXX=${CROSS_PREFIX:-x86_64-w64-mingw32-}g++
WINDRES=${CROSS_PREFIX:-x86_64-w64-mingw32-}windres
mkdir -p build
$WINDRES res/app.rc -O coff -o build/app.res
$CXX -std=c++17 -O2 -DUNICODE -D_UNICODE src/*.cpp build/app.res \
  -o build/Tendedero.exe \
  -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -luuid -lcomdlg32 \
  -lshlwapi -ladvapi32 -lkernel32 -municode -mwindows -static
${CROSS_PREFIX:-x86_64-w64-mingw32-}strip -s build/Tendedero.exe || true
echo "Built build/Tendedero.exe ($(stat -c%s build/Tendedero.exe) bytes)"
