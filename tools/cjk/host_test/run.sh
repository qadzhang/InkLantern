#!/bin/bash
# run.sh — ML CJK 渲染层宿主机像素级验收
# 流程：生成测试字体 -> 拷贝真实 src/rbf_font.c 到隔离目录（stub 头遮蔽）
#       -> host gcc 编译 -> 运行断言
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
WORK="$(mktemp -d /tmp/ml_cjk_test.XXXXXX)"
export WORK
trap 'rm -rf "$WORK"' EXIT

# 1) 生成含测试字符的字体
cd "$HERE/.."
python3 - <<'EOF'
import sys, os
sys.path.insert(0, os.path.abspath('.'))
import gen_cjk_font as g
cps = {ord(c) for c in "中文测试菜单项声光图表"} | {ord(c) for c in g.ALWAYS_INCLUDE}
blob, sorted_cps = g.build_font(cps)
with open(os.path.join(os.environ['WORK'], 'cjk_test.rbf'), 'wb') as f:
    f.write(blob)
print("测试字体: %d 字" % len(sorted_cps))
EOF

# 2) 隔离目录：stub 头 + 真实被测源码
cd "$WORK"
cp "$REPO/src/rbf_font.c" .
cp "$REPO/src/rbf_font.h" .
cp "$HERE/stubs/dryos.h" .
cp "$HERE/stubs/bmp.h" .
cp "$HERE/stubs/beep.h" .
cp "$HERE/test_main.c" .

# 3) 编译运行（-O2 模拟相机端优化；-fsanitize 捕捉越界 UB）
#    注意：只编译 test_main.c —— 它 #include 了 rbf_font.c
gcc -O2 -g -fsanitize=address,undefined -Wall -Wextra -Wno-unused-parameter \
    -o cjk_test test_main.c 2>&1 | head -40

# cjk 字体缓冲按设计常驻（相机生命周期），关闭退出时泄漏检测
CJK_TEST_RBF="$WORK/cjk_test.rbf" ASAN_OPTIONS=detect_leaks=0 ./cjk_test
