#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_cjk_font.py — 生成 Magic Lantern 中文点阵字体 data/fonts/cjk.rbf

WHAT : 扫描 ML 源码（src/ modules/ platform/）中 >= 0x2000 的 Unicode 码点，
       用 Noto Sans CJK SC 光栅化为 24x24 1-bit 点阵（相机端按字体高度缩放绘制），打包成 CJK.RBF 私有格式，
       供 src/rbf_font.c 的 UTF-8 渲染层在相机上加载。
WHY  : RBF 基础字体只覆盖单字节字符集；中文字形必须由独立字库提供。
       字符集从源码字符串自动派生 —— 翻译改了，重跑本脚本即可，永不脱节。
HOW  : 文件格式（小端）:
         u32 magic 0x314B4A43 ("CJK1")
         u16 height (16)   u16 wbytes (2)
         u32 count        u32 index_off (20)   u32 glyphs_off
         u16 index[count]  码点升序
         u8  glyphs[count * height * wbytes]   行主序、字节内 LSB 在前
用法 :
       python3 tools/cjk/gen_cjk_font.py            # 正常：扫源码生成
       python3 tools/cjk/gen_cjk_font.py --preview  # 同时输出预览 PNG
依赖 : python3-pil + fonts-noto-cjk
"""
import argparse
import glob
import os
import re
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_PATH = os.path.join(REPO, "data", "fonts", "cjk.rbf")
PREVIEW_PATH = os.path.join(REPO, "tools", "cjk", "preview.png")

CELL_W = 24          # 字形格宽（像素）
CELL_H = 24          # 字形格高（像素）
WBYTES = CELL_W // 8
THRESHOLD = 96       # 抗锯齿灰度 -> 1bit 的阈值
FONT_PX = 24         # 光栅化请求字号（绘制时按当前字体高度缩放）

# 始终包含的最小中文标点集（>= 0x2000）；翻译实际用到的字符由字面量扫描补齐
ALWAYS_INCLUDE = "，。、：；！？（）—…·"


def find_cjk_font():
    """通过 fontconfig 找 Noto Sans CJK SC Medium 的文件与 face index

    经验（retro-ws 项目实证，16px 下对比测得）：Regular 字重 1bit 光栅化后笔画只有
    1px、全角标点过细；Medium 字重笔画 ~2px，相机小屏可读性显著更好。
    """
    import subprocess
    for name in ("Noto Sans CJK SC Medium", "Noto Sans CJK SC"):
        try:
            out = subprocess.run(
                ["fc-match", "-f", "%{file}|%{index}", name],
                capture_output=True, text=True, timeout=10).stdout.strip()
            path, idx = out.split("|")
            if os.path.exists(path):
                return path, int(idx)
        except Exception:
            pass
    # 退路：直接试常见路径
    for cand in ("/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc",
                 "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"):
        if os.path.exists(cand):
            return cand, 0
    sys.exit("找不到 CJK 字体（需要 fonts-noto-cjk）")



def scan_codepoints(roots):
    """只从 C 字符串字面量中收集 >= 0x2000 的码点（相机是菜单界面不是阅读器，
    注释/文档里的汉字不进字库——这是 CHDK 中文版的成熟做法）"""
    lit_re = re.compile(r'"((?:[^"\\\n]|\\.)*"?)')
    found = set()
    for root in roots:
        for path in glob.glob(os.path.join(root, "**", "*.c"), recursive=True) + \
                    glob.glob(os.path.join(root, "**", "*.h"), recursive=True):
            try:
                with open(path, encoding="utf-8", errors="ignore") as f:
                    text = f.read()
            except OSError:
                continue
            for m in lit_re.finditer(text):
                for ch in m.group(1):
                    cp = ord(ch)
                    if cp >= 0x2000 and not (0xD800 <= cp <= 0xDFFF):
                        found.add(cp)
    return found


def render_glyph(font, ch, origin):
    """以统一原点把字符画到大画布上，返回 CELL_W×CELL_H 的 1-bit 行字节列表"""
    canvas = Image.new("L", (CELL_W * 3, CELL_H * 3), 0)
    draw = ImageDraw.Draw(canvas)
    draw.text(origin, ch, font=font, fill=255)

    # 以参考字形（汉字满格）垂直对齐：取整字 bbox，垂直居中裁进 16 格
    bbox = canvas.getbbox()
    if bbox is None:            # 空白字形（空格等）
        return [0] * (CELL_H * WBYTES)

    x0, y0, x1, y1 = bbox
    cx = (x0 + x1) // 2
    cy = (y0 + y1) // 2

    # 裁剪方形窗口：水平以字形中心、垂直以字形中心（CJK 等高，等价于共享基线）
    left = cx - CELL_W // 2
    top = cy - CELL_H // 2
    rows = []
    px = canvas.load()
    for yy in range(CELL_H):
        rowbytes = []
        for b in range(WBYTES):
            v = 0
            for bit in range(8):
                xx = left + b * 8 + bit
                if 0 <= xx < canvas.width and 0 <= top + yy < canvas.height:
                    if px[xx, top + yy] >= THRESHOLD:
                        v |= 1 << bit       # LSB first（与 rbf_font.c 一致）
            rowbytes.append(v)
        rows.extend(rowbytes)
    return rows


def build_font(codepoints):
    path, face_idx = find_cjk_font()
    font = ImageFont.truetype(path, FONT_PX, index=face_idx)

    # 用参考汉字校准绘制原点：让 '汉' 在 3x 画布中大致落在中央
    origin = (CELL_W, CELL_H)

    cps = sorted(codepoints)
    glyphs = []
    for cp in cps:
        ch = chr(cp)
        rows = render_glyph(font, ch, origin)
        glyphs.append(bytes(rows))

    index = struct.pack("<%dH" % len(cps), *cps)
    # 头 20 字节，与 src/rbf_font.c 的 cjk_hdr_t（packed）严格一致
    hdr = struct.pack("<IHHIII",
                      0x314B4A43, CELL_H, WBYTES, len(cps), 20, 20 + len(index))
    blob = hdr + index + b"".join(glyphs)
    return blob, cps


def make_preview(cps, rbf_path):
    """回读生成的 1-bit rbf 逐点阵绘制预览 —— 所见即相机所得"""
    with open(rbf_path, "rb") as f:
        blob = f.read()
    magic, height, wbytes, count, index_off, glyphs_off = \
        struct.unpack("<IHHIII", blob[:20])
    assert magic == 0x314B4A43
    index = struct.unpack("<%dH" % count, blob[index_off:index_off + count * 2])
    cp2glyph = {cp: blob[glyphs_off + i * height * wbytes:
                          glyphs_off + (i + 1) * height * wbytes]
                for i, cp in enumerate(index)}

    samples = "中文菜单：光圈快门ISO，曝光补偿（自动）。音量峰值对焦！"
    img = Image.new("L", (720, 120), 255)
    d = ImageDraw.Draw(img)
    d.text((8, 4), "ML CJK 24px (round-trip from cjk.rbf, Noto Medium):",
           font=ImageFont.load_default(), fill=0)
    x = 8
    for ch in samples:
        cp = ord(ch)
        g = cp2glyph.get(cp)
        if g is None:
            if cp < 0x2000:
                d.text((x, 32), ch, font=ImageFont.load_default(), fill=0)
                x += 8
            else:
                d.rectangle([x, 32, x + 15, 47], outline=0)
                x += 16
            continue
        for yy in range(height):
            for xx in range(wbytes * 8):
                if g[yy * wbytes + xx // 8] & (1 << (xx % 8)):
                    img.putpixel((x + xx, 32 + yy), 0)
        x += wbytes * 8
        if x > 700:
            break
    img.save(PREVIEW_PATH)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", action="store_true", help="输出预览 PNG")
    ap.add_argument("--charset-out", default=None, help="导出字符集清单（调试）")
    ap.add_argument("--roots", nargs="*",
                    default=[os.path.join(REPO, "src"),
                             os.path.join(REPO, "modules"),
                             os.path.join(REPO, "platform")])
    args = ap.parse_args()

    cps = scan_codepoints(args.roots)
    cps |= {ord(c) for c in ALWAYS_INCLUDE}
    # 剔除明显不是文字的区间（私用区等）
    cps = {cp for cp in cps if cp <= 0xFFFD}

    blob, sorted_cps = build_font(cps)
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "wb") as f:
        f.write(blob)

    print("CJK 字体已生成: %s" % OUT_PATH)
    print("  字符数: %d  文件大小: %d 字节" % (len(sorted_cps), len(blob)))

    if args.charset_out:
        with open(args.charset_out, "w", encoding="utf-8") as f:
            f.write("".join(chr(c) for c in sorted_cps))
    if args.preview:
        make_preview(set(sorted_cps), OUT_PATH)
        print("  预览图: %s" % PREVIEW_PATH)


if __name__ == "__main__":
    main()
