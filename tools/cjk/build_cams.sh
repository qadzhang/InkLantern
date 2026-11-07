#!/bin/bash
# build_cams.sh — 用本地工具链构建 6D.116 与 EOSM.202
# 用法: bash tools/cjk/build_cams.sh [--no-clean]
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
TC="$REPO/toolchain/gcc-arm-none-eabi-4_8-2014q3/bin"
export ARM_BINPATH="$TC"
export PATH="$REPO/tools/cjk/bin:$PATH"   # 本地 rst2html（docutils，不入库）

echo "== 工具链 =="
"$TC/arm-none-eabi-gcc" --version | head -1

if [ "$1" != "--no-clean" ]; then
    echo "== 清理 =="
    (cd "$REPO/modules" && make clean >/dev/null 2>&1 || true)
    for cam in EOSM.202 6D.116; do
        (cd "$REPO/platform/$cam" && make clean >/dev/null 2>&1 || true)
    done
fi

for cam in EOSM.202 6D.116; do
    echo "== 构建 $cam =="
    (cd "$REPO/platform/$cam" && make -j"$(nproc)")
    ls -la "$REPO/platform/$cam/build/magiclantern.zip"
done

echo "== 全部完成 =="
