/* 宿主测试 stub：替代 bmp.h。
 * 字体位域宏（FONT / SHADOW / ALIGN 等）从真实 src/bmp.h 原样复制以保真，
 * 绘制原语换成测试实现（带越界检查的假 VRAM）。 */
#ifndef STUB_BMP_H
#define STUB_BMP_H

#include "dryos.h"

/* 与真实 src/font.h 一致的最小定义 */
struct font
{
    unsigned height;
    unsigned width;
    unsigned* bitmap;
};

#define BMPPITCH      720
#define BMP_W_MINUS   0
#define BMP_W_PLUS    720
#define BMP_H_MINUS   0
#define BMP_H_PLUS    480

#define COLOR_BLACK 0
#define COLOR_WHITE 255
#define COLOR_CYAN  200
#define COLOR_GRAY(g) (g)

uint8_t* bmp_vram(void);
void bmp_fill(uint8_t color, int x, int y, int w, int h);
void bmp_putpixel(int x, int y, uint8_t color);
void bmp_putpixel_fast(uint8_t * const bvram, int x, int y, uint8_t color);
int  bmp_printf(uint32_t fontspec, int x, int y, const char* fmt, ...);

extern uint32_t ml_refresh_display_needed;

/* ---- 以下宏与真实 src/bmp.h 完全一致 ---- */
#define FONT_MASK              0x00070000
#define SHADOW_MASK            0x00080000
#define SHADOW_FONT(fnt) ((fnt) | SHADOW_MASK)
#define FONT_ALIGN_MASK        0x03000000
#define FONT_ALIGN_LEFT        0x00000000
#define FONT_ALIGN_CENTER      0x01000000
#define FONT_ALIGN_RIGHT       0x02000000
#define FONT_ALIGN_JUSTIFIED   0x03000000
#define FONT_TEXT_WIDTH_MASK   0xFC000000
#define FONT_TEXT_WIDTH(width)  ((((width+8) >> 4) << 26) & FONT_TEXT_WIDTH_MASK)
#define FONT_ALIGN_FILL        0x00800000
#define FONT_EXPAND_MASK       0x00700000
#define FONT_EXPAND(pix)       (((pix) << 20) & FONT_EXPAND_MASK)
#define FONT(font,fg,bg)        ( 0 \
        | ((font) & (0xFFFF0000)) \
        | ((bg) & 0xFF) << 8 \
        | ((fg) & 0xFF) << 0 \
)
#define FONT_DYN(font_id,fg,bg) FONT((font_id)<<16,fg,bg)
#define FONT_MONO_12  FONT_DYN(0, COLOR_WHITE, COLOR_BLACK)
#define FONT_MONO_20  FONT_DYN(1, COLOR_WHITE, COLOR_BLACK)
#define FONT_SANS_23  FONT_DYN(2, COLOR_WHITE, COLOR_BLACK)
#define FONT_SANS_28  FONT_DYN(3, COLOR_WHITE, COLOR_BLACK)
#define FONT_SANS_32  FONT_DYN(4, COLOR_WHITE, COLOR_BLACK)
#define FONT_CANON    FONT_DYN(7, COLOR_WHITE, COLOR_BLACK)
#define FONT_SMALL      FONT_MONO_12
#define FONT_MED        FONT_SANS_23
#define FONT_MED_LARGE  FONT_SANS_28
#define FONT_LARGE      FONT_SANS_32

/* 真实构建中来自平台 consts.h；测试中定义为 NULL */
extern uint8_t *BFNT_CHAR_CODES;
extern uint8_t *BFNT_BITMAP_OFFSET;
extern uint8_t *BFNT_BITMAP_DATA;
#define FONT_ID(font) (((font) >> 16) & 0x7)
#define FONT_BG(font) (((font) & 0xFF00) >> 8)
#define FONT_FG(font) (((font) & 0x00FF) >> 0)
#define FONT_GET_TEXT_WIDTH(font)    (((font) >> 22) & 0x3F0)
#define NO_BG_ERASE 0xFF

#define MAX_DYN_FONTS 7
extern struct font font_dynamic[MAX_DYN_FONTS+1];
extern uint32_t font_by_name(char *file, uint32_t fg_color, uint32_t bg_color);

static inline struct font *
fontspec_font(uint32_t fontspec)
{
    return &(font_dynamic[FONT_ID(fontspec) % COUNT(font_dynamic)]);
}

static inline uint32_t fontspec_fg(uint32_t fontspec) { return (fontspec >> 0) & 0xFF; }
static inline uint32_t fontspec_bg(uint32_t fontspec) { return (fontspec >> 8) & 0xFF; }
static inline uint32_t fontspec_height(uint32_t fontspec) { return fontspec_font(fontspec)->height; }
static inline uint32_t fontspec_width(uint32_t fontspec) { return fontspec_font(fontspec)->width; }

/* BFNT 后端存根（测试不覆盖 Canon ROM 字体路径） */
void bfnt_draw_char(int ch, int x, int y, int fg, int bg);
void bfnt_puts(const char *str, int x, int y, int fg, int bg);
int  bfnt_char_get_width(int ch);

#endif
