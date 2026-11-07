/*
 * test_main.c — ML CJK 渲染层宿主机像素级验收测试
 *
 * 方法论来自 retro-ws 项目的"像素级验收门"：改动中的渲染代码无法在
 * 本机跑相机固件，就在宿主机用假 VRAM + 金丝雀边界逐像素断言。
 *
 * 编译方式：run.sh 把 src/rbf_font.c 原样拷到本目录，本文件直接
 * #include 它 —— static 函数（utf8_next/cjk_font_load 等）可测，
 * 且保证测的就是真实源码，不是复制走样的版本。
 */
#include "rbf_font.c"    /* 引入被测代码 + stub dryos.h/bmp.h/beep.h */

#include <assert.h>

/* ---------------- 假 VRAM（带金丝雀护板） ---------------- */
#define VRAM_W 720
#define VRAM_H 480
#define GUARD  4096

static uint8_t vram_buf[GUARD + VRAM_W * VRAM_H + GUARD];
static uint8_t* vram_mid = vram_buf + GUARD;
static int      oob_hits = 0;      /* 越界写计数 */

static int canary_ok(void)
{
    for (int i = 0; i < GUARD; i++)
        if (vram_buf[i] != 0xA5 || vram_buf[GUARD + VRAM_W*VRAM_H + i] != 0xA5)
            return 0;
    return 1;
}

uint8_t* bmp_vram(void) { return vram_mid; }

void bmp_putpixel_fast(uint8_t * const bvram, int x, int y, uint8_t color)
{
    if (bvram != vram_mid) { oob_hits++; return; }
    if (x < 0 || x >= VRAM_W || y < 0 || y >= VRAM_H) { oob_hits++; return; }
    bvram[x + y * BMPPITCH] = color;
}

void bmp_putpixel(int x, int y, uint8_t color) { bmp_putpixel_fast(vram_mid, x, y, color); }

int bmp_printf(uint32_t fontspec, int x, int y, const char* fmt, ...)
{ (void)fontspec; (void)x; (void)y; (void)fmt; return 0; }

void bmp_fill(uint8_t color, int x, int y, int w, int h)
{
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            bmp_putpixel_fast(vram_mid, xx, yy, color);
}

/* ---------------- 其余链接存根 ---------------- */
uint32_t ml_refresh_display_needed = 0;
uint8_t *BFNT_CHAR_CODES = NULL;
uint8_t *BFNT_BITMAP_OFFSET = NULL;
uint8_t *BFNT_BITMAP_DATA = NULL;
static int bfnt_calls = 0;
void bfnt_draw_char(int ch, int x, int y, int fg, int bg) { (void)ch;(void)x;(void)y;(void)fg;(void)bg; bfnt_calls++; }
void bfnt_puts(const char *s, int x, int y, int fg, int bg) { (void)s;(void)x;(void)y;(void)fg;(void)bg; }
int  bfnt_char_get_width(int ch) { (void)ch; return 8; }

/* FIO 存根：走真实 stdio，端到端覆盖 cjk_font_load 的分块加载路径 */
ML_FILE FIO_OpenFile(const char* p, int f) { (void)f; return (ML_FILE)fopen(p, "rb"); }
int FIO_ReadFile(ML_FILE fd, void* b, int l) { return (int)fread(b, 1, l, (FILE*)fd); }
int FIO_CloseFile(ML_FILE fd) { return fclose((FILE*)fd) == 0 ? 0 : 1; }
int FIO_SeekSkipFile(ML_FILE fd, int o, int w) { return (int)fseek((FILE*)fd, o, w); }
int FIO_GetFileSize(const char* p, uint32_t* s)
{
    FILE* f = fopen(p, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    *s = (uint32_t)ftell(f);
    fclose(f);
    return 0;
}
void* fio_malloc(int l) { return malloc(l); }
void fio_free(void* p) { free(p); }

void* read_entire_file(const char* path, int* size)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* buf = malloc(n);
    if (fread(buf, 1, n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    fclose(f);
    if (size) *size = (int)n;
    return buf;
}

void DryosDebugMsg(int a, int b, const char* fmt, ...) { (void)a;(void)b;(void)fmt; }

/* ---------------- 测试基建 ---------------- */
static int fails = 0, total = 0;
#define CHECK(cond, name) do { \
    total++; \
    if (cond) { printf("PASS  %s\n", name); } \
    else { fails++; printf("FAIL  %s   (%s:%d)\n", name, __FILE__, __LINE__); } \
} while (0)

static void clear_vram(void)
{
    memset(vram_mid, 0, VRAM_W * VRAM_H);
}

static int count_ink(int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (vram_mid[x + y * BMPPITCH] == 9)   /* fg=9 */
                n++;
    return n;
}

/* 造一个假的基础字体（rbf_font.h 的 font，即 RBF 后端结构）：
 * 高度 12、ASCII 宽 8、cTable=NULL（ASCII 绘制走 bfnt 存根，宽度仍走 wTable） */
static font g_base;
static font* make_fake_base_font(void)
{
    memset(&g_base, 0, sizeof(g_base));
    g_base.hdr.height = 12;
    g_base.hdr.charFirst = 0x20;
    g_base.hdr.charLast = 0xFF;
    g_base.width = 8;
    for (int i = 0; i < 256; i++) g_base.wTable[i] = 8;
    font_dynamic[0].bitmap = (unsigned*) &g_base;   /* 与 bmp_puts 的类型双关一致 */
    return &g_base;
}

/* ---------------- 用例 ---------------- */

static void test_utf8_next(void)
{
    const char* s;
    s = "A";       CHECK(utf8_next(&s) == 0x41 && *s == 0, "utf8: ASCII");
    s = "\xE4\xB8\xAD"; CHECK(utf8_next(&s) == 0x4E2D && *s == 0, "utf8: 中 U+4E2D");
    s = "\xE2\x82\xAC"; CHECK(utf8_next(&s) == 0x20AC && *s == 0, "utf8: € U+20AC");
    s = "\xC2\xA9";     CHECK(utf8_next(&s) == 0xA9 && *s == 0, "utf8: 2字节 © ");
    s = "\xF0\x9F\x98\x80"; CHECK(utf8_next(&s) == 0x1F600, "utf8: 4字节 emoji");
    s = "\x80";    CHECK(utf8_next(&s) == '?', "utf8: 非法前导字节 -> '?'");
    s = "\xFF";    CHECK(utf8_next(&s) == '?', "utf8: 0xFF -> '?'");
    s = "\xE4\xB8";  CHECK(utf8_next(&s) == '?', "utf8: 截断序列安全终止");
    /* 连续解码 */
    s = "中a文";
    CHECK(utf8_next(&s) == 0x4E2D, "utf8: 连续 1/3");
    CHECK(utf8_next(&s) == 'a',    "utf8: 连续 2/3");
    CHECK(utf8_next(&s) == 0x6587, "utf8: 连续 3/3");
}

static void test_cjk_load_and_lookup(void)
{
    cjk_font_load();
    CHECK(cjk_font.index != NULL, "cjk: cjk.rbf 分块加载成功");
    CHECK(cjk_font.hdr.height == 24, "cjk: 原生 height=24");
    CHECK(cjk_font.hdr.wbytes == 3, "cjk: wbytes=3");
    CHECK(cjk_font.hdr.count >= 20, "cjk: count>=20（测试字集10字+精简标点集）");
    CHECK(cjk_glyph_lookup(0x4E2D) >= 0, "cjk: lookup 中 命中");  /* 需测试字体包含 */
    CHECK(cjk_glyph_lookup(0x3000) == -1, "cjk: 不存在的码点 miss");
    CHECK(cjk_glyph_lookup('A') == -1, "cjk: ASCII 不进 CJK 表");
    CHECK(cjk_glyph_lookup(0x10000) == -1, "cjk: >0xFFFF 拒绝");
}

static void test_widths(void)
{
    font* f = make_fake_base_font();

    /* 若 '中' 在字库：宽度 = 16 */
    int w_zhong = 12;   /* CJK 步进 = 基础字体高度(12) */
    int r = rbf_str_width(f, "A");
    CHECK(r == 8, "width: 单个 ASCII = 8");
    r = rbf_str_width(f, "中");
    CHECK(r == w_zhong, "width: 单个中文 = 基础字体高 12");
    r = rbf_str_width(f, "中A中");
    CHECK(r == 12 + 8 + 12, "width: 中A中 = 32");
    r = rbf_str_width(f, "中\nAAAA中");
    CHECK(r == MAX(12, 4 * 8 + 12), "width: 多行取最长行");  /* AAAA中 = 44 */

    r = rbf_str_clipped_width(f, "中中中", 24);
    CHECK(r == 24, "width: 裁剪到 24px -> 2 个中文");
    r = rbf_strlen_clipped(f, "中中中", 24);
    CHECK(r == 6, "strlen_clipped: 24px -> 6 字节");

    r = rbf_str_clipped_width(f, "中中A中", 36);
    CHECK(r == 32, "width: 裁剪边界 (12+12+8=32 恰好放下)");
}

static void test_draw_pixels(void)
{
    clear_vram();
    font* f = make_fake_base_font();
    uint32_t spec = FONT(0 << 16, 9 /*fg*/, 2 /*bg*/);

    /* 画一个中文：应出现在 (10,10)-(26,26) 区域内
       注意：rbf_draw_string 会临时写输入串（按行切断），必须传可写缓冲 */
    char b1[] = "中";
    int l = rbf_draw_string(f, 10, 10, b1, spec);
    CHECK(l >= 12, "draw: 返回 advance >= 12（基础字体高）");
    int ink = count_ink(10, 10, 22, 22);
    CHECK(ink > 10, "draw: 中文有墨迹（12px 格）");
    CHECK(count_ink(22, 0, 720, 480) == 0, "draw: 墨迹不出 12px 宽格");
    CHECK(count_ink(0, 22, 720, 480) == 0, "draw: 墨迹不出 12px 高格");
    CHECK(canary_ok() && oob_hits == 0, "draw: 金丝雀完整、无越界写");

    /* 混合 ASCII：ASCII 走 bfnt（无墨迹），但 advance 正确推进 */
    clear_vram();
    char b2[] = "A中B";
    l = rbf_draw_string(f, 0, 4, b2, spec);
    CHECK(count_ink(8, 4, 20, 20) > 10, "draw: 混排时中文落在第二个格");
    CHECK(count_ink(0, 4, 8, 20) == 0 && count_ink(20, 4, 36, 20) == 0,
          "draw: ASCII 槽位无墨迹（bfnt 存根）");

    /* 字库中不存在的码点 -> '?'，不崩溃 */
    clear_vram();
    char b3[] = "\xE2\x80\x8B";
    l = rbf_draw_string(f, 0, 4, b3, spec);   /* U+200B 零宽空格（无字形） */
    CHECK(l == 8, "draw: 未知码点回退 '?' 的 advance=8");

    /* 多行：第二行 y 偏移 = 基础字体高度 12 */
    clear_vram();
    char b4[] = "中\n中";
    rbf_draw_string(f, 0, 4, b4, spec);
    CHECK(count_ink(0, 4, 16, 17) > 20, "draw: 第 1 行");    /* 行高 12，只查本行独有部分 */
    CHECK(count_ink(0, 16, 16, 32) > 20, "draw: 第 2 行 y+=12");

    CHECK(canary_ok() && oob_hits == 0, "draw: 全部绘制后金丝雀仍完整");
}

static void test_clipped_and_justified(void)
{
    clear_vram();
    font* f = make_fake_base_font();
    /* 注意 FONT_TEXT_WIDTH 是 6bit 量化编码：40 会解码成 48（恰好 3 个中文）。
       用 32（解码仍为 32）才能验证第 3 个字被裁掉。 */
    uint32_t spec = FONT(0 << 16, 9, 2) | FONT_ALIGN_LEFT | FONT_TEXT_WIDTH(32) | FONT_ALIGN_FILL;

    char b5[] = "中中中中";
    rbf_draw_string(f, 0, 4, b5, spec);
    CHECK(count_ink(0, 4, 16, 20) > 20, "clip: 第 1 个中文绘出");
    CHECK(count_ink(32, 4, 720, 480) == 0, "clip: 32px 外无墨迹");

    /* JUSTIFIED 分支（Bresenham 拉伸路径，含 CJK 解码循环） */
    clear_vram();
    spec = FONT(0 << 16, 9, 2) | FONT_ALIGN_JUSTIFIED | FONT_TEXT_WIDTH(200) | FONT_ALIGN_FILL;
    char b6[] = "中文测试菜单项";
    rbf_draw_string(f, 0, 4, b6, spec);
    CHECK(count_ink(0, 4, 210, 24) > 50, "justified: CJK 拉伸绘制有墨迹");
    CHECK(canary_ok() && oob_hits == 0, "justified: 无越界");

    /* CENTER 对齐 */
    clear_vram();
    spec = FONT(0 << 16, 9, 2) | FONT_ALIGN_CENTER | FONT_TEXT_WIDTH(320);
    char b7[] = "中";
    rbf_draw_string(f, 160, 4, b7, spec);
    /* 居中：字符串宽 16，窗口 320，锚点 x=160 => 绘制起点 160-160+152 = 152 */
    CHECK(count_ink(152, 4, 160, 20) > 5, "center: 居中绘制（左半）");
    CHECK(count_ink(160, 4, 168, 20) > 5, "center: 居中绘制（右半）");
}

static void test_scaling(void)
{
    clear_vram();
    font* f = make_fake_base_font();      /* h=12 */
    uint32_t spec = FONT(0 << 16, 9, 2);
    char b[] = "中";

    int adv = rbf_draw_string(f, 100, 100, b, spec);
    CHECK(adv == 12, "scale: 12px 字体下 advance=12（下采样）");
    CHECK(count_ink(100, 100, 112, 112) > 10, "scale: 12px 墨迹正常");

    g_base.hdr.height = 32;               /* 换 32px 基础字体 */
    adv = rbf_draw_string(&g_base, 100, 200, b, spec);
    CHECK(adv == 32, "scale: 32px 字体下 advance=32（上采样）");
    CHECK(count_ink(100, 200, 132, 232) > 60, "scale: 32px 墨迹面积放大");
    CHECK(count_ink(132, 200, 720, 480) == 0, "scale: 32px 不越 32 格");

    g_base.hdr.height = 12;               /* 还原 */
    CHECK(canary_ok() && oob_hits == 0, "scale: 金丝雀完整");
}

int main(void)
{
    memset(vram_buf, 0xA5, sizeof(vram_buf));

    test_utf8_next();

    /* 先生成包含测试字的字体再跑加载用例：run.sh 会用测试字符集生成 _test.rbf */
    const char* rbf = getenv("CJK_TEST_RBF");
    if (!rbf) rbf = "cjk_test.rbf";

    /* 直接把测试字库喂给加载器（read_entire_file 读宿主路径） */
    {
        int size = 0;
        void* p = read_entire_file(rbf, &size);
        if (!p) { fprintf(stderr, "找不到测试字体 %s\n", rbf); return 2; }
        free(p);
    }
    /* cjk_font_load 读取固定路径 "ML/FONTS/CJK.RBF" —— 用 chdir 仿真 */
    {
        /* read_entire_file 是直通实现，建一个 ML/FONTS 目录结构 */
        system("mkdir -p ML/FONTS 2>/dev/null");
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cp '%s' ML/FONTS/CJK.RBF", rbf);
        if (system(cmd) != 0) { fprintf(stderr, "复制测试字体失败\n"); return 2; }
    }
    cjk_font_load();

    test_cjk_load_and_lookup();
    test_scaling();
    test_widths();
    test_draw_pixels();
    test_clipped_and_justified();

    printf("\n==== %d/%d PASS, %d FAIL ====\n", total - fails, total, fails);
    return fails ? 1 : 0;
}
