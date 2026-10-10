#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 LVGL 8.3 用的界面汉字字体 ui_font.c。

只收录界面出现的汉字（约 20 个），1bpp、16px、SPARSE_TINY cmap，
fallback 指向 lv_font_montserrat_16，所以 ASCII、数字、℃ 的 ° 走回退，
不必收进本字体。字符集变化时改 GLYPHS 再跑一次即可。

用法: python tools/gen_ui_font.py
依赖: Pillow、C:/Windows/Fonts/simhei.ttf
渲染约定与 LVGL lv_draw_sw_letter 对齐:
  gpos.y = pos.y + (line_height - base_line) - box_h - ofs_y
  取 line_height=16, base_line=2 → 基线在行顶下方 14px。
  以基线在图像第 14 行渲染, ofs_y = 13 - bbox_bottom。
"""
import os
from PIL import Image, ImageDraw, ImageFont

# 界面用到的汉字（与各 ui_*.c 的 label 文本一致）。改文案时同步这里。
# 前段=固定界面字；后段=方案 4.4 天气词表的动态字（晴/多云/阴/雾…），
# 供 C3 回传的"多云 26°C"这类文本显示。数字、°、ASCII 走 montserrat 回退。
GLYPHS = "时间计步天气语音上下打开返回离线在表温空闲" \
         "晴大部多云阴雾毛雨雪雷暴不可用" \
         "重庆北京上海广州成都切换体感湿度北南东西风查询中录音上传识别失败超时没听清按住说话松开" \
         "校动阈步间隔毫确认检测跌倒按键" \
         "今日准对零量值态" \
         "秒倒计开始停闹钟提醒点关算" \
         "游戏贪吃蛇乒乓球俄罗斯方块结束再来一局长按选择进入得分转向挡板转落移"

FONT_PATH = "C:/Windows/Fonts/simhei.ttf"
PX = 16
BASE_LINE = 2
LINE_HEIGHT = 16
BASELINE_ROW = LINE_HEIGHT - BASE_LINE   # 14
OUT = os.path.join(os.path.dirname(__file__), "..", "APP", "ui", "ui_font.c")


def render_glyph(font, ch):
    """渲染一个字，返回 (bitmap_bytes, box_w, box_h, ofs_x, ofs_y, adv_w)。"""
    img = Image.new("L", (PX + 8, PX + 8), 0)
    d = ImageDraw.Draw(img)
    # 左基线锚点，x=1 留一点左边距；基线放在第 BASELINE_ROW 行
    d.text((1, BASELINE_ROW), ch, fill=255, font=font, anchor="ls")

    px = img.load()
    w, h = img.size
    # 计算墨迹 bbox（阈值 128）
    left, top, right, bottom = w, h, -1, -1
    for y in range(h):
        for x in range(w):
            if px[x, y] >= 128:
                if x < left: left = x
                if x > right: right = x
                if y < top: top = y
                if y > bottom: bottom = y
    if right < 0:  # 空白（不该发生）
        return b"", 0, 0, 0, 0, PX * 16

    box_w = right - left + 1
    box_h = bottom - top + 1
    ofs_x = left
    ofs_y = BASELINE_ROW - 1 - bottom   # = 13 - bbox_bottom

    # 1bpp 连续打包，MSB 在前，行步长 = box_w bit
    bits = []
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            bits.append(1 if px[x, y] >= 128 else 0)
    data = bytearray()
    acc = 0
    nbit = 0
    for b in bits:
        acc = (acc << 1) | b
        nbit += 1
        if nbit == 8:
            data.append(acc)
            acc = 0
            nbit = 0
    if nbit:
        acc <<= (8 - nbit)
        data.append(acc)
    return bytes(data), box_w, box_h, ofs_x, ofs_y, PX * 16


def main():
    font = ImageFont.truetype(FONT_PATH, PX)
    chars = sorted(set(GLYPHS), key=lambda c: ord(c))

    glyphs = []       # (codepoint, data, box_w, box_h, ofs_x, ofs_y, adv_w)
    for ch in chars:
        glyphs.append((ord(ch),) + render_glyph(font, ch))

    # 拼 bitmap 与 glyph_dsc
    bitmap = bytearray()
    dsc = [(0, 0, 0, 0, 0, 0)]  # id0 reserved
    idx = 0
    for (_cp, data, bw, bh, ox, oy, aw) in glyphs:
        dsc.append((idx, aw, bw, bh, ox, oy))
        bitmap += data
        idx += len(data)

    lines = []
    A = lines.append
    A("/*******************************************************************************")
    A(" * Size: 16 px, Bpp: 1, simhei, UI glyphs only")
    A(" * 由 tools/gen_ui_font.py 生成，勿手改。fallback=lv_font_montserrat_16")
    A(" ******************************************************************************/")
    A("")
    A('#include "lvgl.h"')
    A("")
    A("#ifndef UI_FONT_16")
    A("#define UI_FONT_16 1")
    A("#endif")
    A("")
    A("#if UI_FONT_16")
    A("")
    A("/*-----------------")
    A(" *    BITMAPS")
    A(" *----------------*/")
    A("")
    A("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {")
    # 每字一段，便于对照
    off = 0
    for (cp, data, bw, bh, ox, oy, aw) in glyphs:
        A("    /* U+%04X \"%s\" */" % (cp, chr(cp)))
        row = "    "
        for i, b in enumerate(data):
            row += "0x%x, " % b
            if (i + 1) % 12 == 0:
                A(row.rstrip())
                row = "    "
        if row.strip():
            A(row.rstrip())
        A("")
        off += len(data)
    A("};")
    A("")
    A("/*---------------------")
    A(" *  GLYPH DESCRIPTION")
    A(" *--------------------*/")
    A("")
    A("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {")
    A("    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,")
    for (bi, aw, bw, bh, ox, oy) in dsc[1:]:
        A("    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d}," %
          (bi, aw, bw, bh, ox, oy))
    A("};")
    A("")
    A("/*---------------------")
    A(" *  CHARACTER MAPPING")
    A(" *--------------------*/")
    A("")
    # SPARSE_TINY 的 unicode_list 是 uint16，LVGL 用 int16 做二分比较。
    # 整段码点跨度超过 32767 时差值被当成负数，比较反了，字就找不到。
    # 所以按码点切段，每段跨度控制在 32767 以内。
    cps = [g[0] for g in glyphs]
    assert cps == sorted(cps), "unicode_list 必须升序，否则二分查找会漏字"
    spans = []
    begin = 0
    for i in range(1, len(cps) + 1):
        if i == len(cps) or cps[i] - cps[begin] > 8000:
            spans.append((begin, i))
            begin = i
    for si, (a, b) in enumerate(spans):
        base = cps[a]
        A("static const uint16_t unicode_list_%d[] = {" % si)
        row = "    "
        for k, cp in enumerate(cps[a:b]):
            row += "0x%x, " % (cp - base)
            if (k + 1) % 8 == 0:
                A(row.rstrip())
                row = "    "
        if row.strip():
            A(row.rstrip())
        A("};")
        A("")
    A("static const lv_font_fmt_txt_cmap_t cmaps[] =")
    A("{")
    for si, (a, b) in enumerate(spans):
        A("    {")
        A("        .range_start = %d, .range_length = %d, .glyph_id_start = %d," % (cps[a], cps[b-1] - cps[a] + 1, a + 1))
        A("        .unicode_list = unicode_list_%d, .glyph_id_ofs_list = NULL, .list_length = %d, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY" % (si, b - a))
        A("    },")
    A("};")
    A("")
    A("/*--------------------")
    A(" *  ALL CUSTOM DATA")
    A(" *--------------------*/")
    A("")
    A("#if LVGL_VERSION_MAJOR == 8")
    A("static  lv_font_fmt_txt_glyph_cache_t cache;")
    A("#endif")
    A("")
    A("#if LVGL_VERSION_MAJOR >= 8")
    A("static const lv_font_fmt_txt_dsc_t font_dsc = {")
    A("#else")
    A("static lv_font_fmt_txt_dsc_t font_dsc = {")
    A("#endif")
    A("    .glyph_bitmap = glyph_bitmap,")
    A("    .glyph_dsc = glyph_dsc,")
    A("    .cmaps = cmaps,")
    A("    .kern_dsc = NULL,")
    A("    .kern_scale = 0,")
    A("    .cmap_num = %d," % len(spans))
    A("    .bpp = 1,")
    A("    .kern_classes = 0,")
    A("    .bitmap_format = 0,")
    A("#if LVGL_VERSION_MAJOR == 8")
    A("    .cache = &cache")
    A("#endif")
    A("};")
    A("")
    A("/*-----------------")
    A(" *  PUBLIC FONT")
    A(" *----------------*/")
    A("")
    A("#if LVGL_VERSION_MAJOR >= 8")
    A("const lv_font_t ui_font_16 = {")
    A("#else")
    A("lv_font_t ui_font_16 = {")
    A("#endif")
    A("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,")
    A("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,")
    A("    .line_height = %d," % LINE_HEIGHT)
    A("    .base_line = %d," % BASE_LINE)
    A("#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)")
    A("    .subpx = LV_FONT_SUBPX_NONE,")
    A("#endif")
    A("#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8")
    A("    .underline_position = -1,")
    A("    .underline_thickness = 1,")
    A("#endif")
    A("    .dsc = &font_dsc,")
    A("#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9")
    A("    .fallback = &lv_font_montserrat_16,")
    A("#endif")
    A("    .user_data = NULL,")
    A("};")
    A("")
    A("#endif /*#if UI_FONT_16*/")
    A("")

    with open(os.path.normpath(OUT), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("wrote %s : %d glyphs, %d bitmap bytes" % (os.path.normpath(OUT), len(glyphs), len(bitmap)))


if __name__ == "__main__":
    main()
