
#include "dryos.h"
#include "bmp.h"
#include "beep.h"
#include "rbf_font.h"

extern uint32_t ml_refresh_display_needed;

/* here some macros to wrap the CHDK functions for ML code */
#define FG_COLOR(f) FONT_FG(f)
#define BG_COLOR(f) FONT_BG(f)
#define MAKE_COLOR(fg,bg) FONT(0,fg,bg)

#define FONT_CONDENSED 0x00100000 /* this bit is not (yet) used, so we use it as internal flag */

#define draw_char(x,y,c,h) do{}while(0)

static int rbf_font_load(char *file, font* f, int maxchar);
static inline int rbf_font_height(font *rbf_font);
static inline int rbf_char_width(font *rbf_font, int ch);

//-------------------------------------------------------------------
static int RBF_HDR_MAGIC1 = 0x0DF00EE0;
static int RBF_HDR_MAGIC2 = 0x00000003;

struct font font_dynamic[MAX_DYN_FONTS+1];
static char *dyn_font_name[MAX_DYN_FONTS+1];
uint32_t dyn_fonts = 0;

/* ------------------------------------------------------------------
 * CJK (Chinese/Japanese/Korean) glyph font, loaded from ML/FONTS/CJK.RBF
 *
 * The stock RBF fonts only cover single byte charsets.  All menu and
 * UI strings are UTF-8; codepoints >= 0x2000 that are not covered by
 * the active RBF font are looked up in this shared glyph table and
 * drawn on top of the current fontspec (colors from fontspec, size
 * fixed by the font file, typically 16px).
 *
 * File layout ("CJK1", little endian):
 *   u32 magic 'C','J','K','1' packed LE = 0x314B4A43
 *   u16 height          glyph height in pixels (e.g. 16)
 *   u16 wbytes          bytes per glyph row (e.g. 2 => 16px wide)
 *   u32 count           number of glyphs
 *   u32 index_off       file offset of index table (count * u16 codepoints, ascending)
 *   u32 glyphs_off      file offset of glyph bitmaps (count * height * wbytes bytes)
 *   index table and glyph data follow; pixel bit order: LSB first
 * ------------------------------------------------------------------ */
#define CJK_MAGIC       0x314B4A43
#define CJK_CODEPOINT_MIN 0x2000   /* below this, base RBF fonts are used */

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint16_t height;
    uint16_t wbytes;
    uint32_t count;
    uint32_t index_off;
    uint32_t glyphs_off;
} cjk_hdr_t;

static struct
{
    cjk_hdr_t   hdr;        /* copy of the file header */
    uint16_t*   index;      /* count codepoints, ascending */
    uint8_t*    glyphs;     /* count * height * wbytes */
} cjk_font;

/* read a chunk from an open file into cached memory, via a small temporary
 * DMA buffer (same pattern as font_read below: the MEM_TEMPORARY/DMA pool
 * is scarce on some cams - notably EOSM - so it must only be used
 * transiently and in small blocks, never held) */
static int cjk_read_chunk(FILE* fd, uint8_t* dest, int len)
{
    int bytes_read = 0;
    uint8_t* ubuf = (uint8_t*) fio_malloc(len);
    if (ubuf)
    {
        bytes_read = FIO_ReadFile(fd, ubuf, len);
        memcpy(dest, ubuf, len);
        fio_free(ubuf);
    }
    return bytes_read;
}

/* must be called before any CJK-aware draw / measure call */
static void cjk_font_load(void)
{
    char filename[] = "ML/FONTS/CJK.RBF";
    uint32_t size = 0;
    if (FIO_GetFileSize(filename, &size) != 0 || size < sizeof(cjk_hdr_t))
        return;

    FILE* fd = FIO_OpenFile(filename, O_RDONLY | O_SYNC);
    if (!fd)
        return;

    cjk_hdr_t h;
    uint16_t* index = 0;
    uint8_t* glyphs = 0;

    do
    {
        if (cjk_read_chunk(fd, (uint8_t*)&h, sizeof(h)) != (int)sizeof(h))
            break;

        if (   h.magic != CJK_MAGIC
            || h.height == 0 || h.height > 64
            || h.wbytes == 0 || h.wbytes > 8
            || h.count == 0
            || (uint64_t)h.index_off + (uint64_t)h.count * 2 > size
            || (uint64_t)h.glyphs_off + (uint64_t)h.count * h.height * h.wbytes > size )
            break;

        uint32_t sizes[2]  = { h.count * 2, h.count * h.height * h.wbytes };
        uint32_t offsets[2] = { h.index_off, h.glyphs_off };
        uint8_t* dests[2];

        index  = (uint16_t*) malloc(sizes[0]);
        glyphs = (uint8_t*)  malloc(sizes[1]);
        dests[0] = (uint8_t*) index;
        dests[1] = glyphs;
        if (!index || !glyphs)
            break;

        int ok = 1;
        for (int part = 0; part < 2 && ok; part++)
        {
            FIO_SeekSkipFile(fd, offsets[part], SEEK_SET);
            uint32_t remaining = sizes[part];
            while (remaining)
            {
                int chunk = MIN(remaining, 8192);
                if (cjk_read_chunk(fd, dests[part], chunk) != chunk)
                {
                    ok = 0;
                    break;
                }
                dests[part] += chunk;
                remaining -= chunk;
            }
        }
        if (!ok)
            break;

        /* only publish once both tables are fully loaded */
        cjk_font.hdr = h;
        cjk_font.index = index;
        cjk_font.glyphs = glyphs;
        index = 0;
        glyphs = 0;
    } while (0);

    free(index);
    free(glyphs);
    FIO_CloseFile(fd);
}

/* binary search; returns glyph index or -1 */
static int cjk_glyph_lookup(int cp)
{
    if (!cjk_font.index || cp < CJK_CODEPOINT_MIN || cp > 0xFFFF)
        return -1;

    int lo = 0;
    int hi = (int)cjk_font.hdr.count - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        uint16_t code = cjk_font.index[mid];    /* u16 aligned */
        if (code == cp)     return mid;
        else if (code < cp) lo = mid + 1;
        else                hi = mid - 1;
    }
    return -1;
}

/* CJK advance follows the base font height (square em), so Chinese text
 * is optically the same size as the surrounding Latin glyphs */
static int cjk_char_width(const font * base)
{
    return base->hdr.height;
}

/* draw one glyph, nearest-neighbour scaled from the native cell to the
 * base font's height; returns advance in px */
static int FAST cjk_draw_glyph(int idx, int x, int y, int fontspec, const font * base)
{
    const int sh = cjk_font.hdr.height;            /* native cell height */
    const int swb = cjk_font.hdr.wbytes;           /* native bytes per row */
    const int sw  = swb * 8;                       /* native cell width */
    const int th  = base->hdr.height;              /* target height */
    const int tw  = th;                            /* square em */
    const uint8_t* g = cjk_font.glyphs + (uint32_t)idx * sh * swb;

    int fg = FG_COLOR(fontspec);
    int bg = BG_COLOR(fontspec);
    uint8_t * bmp = bmp_vram();

    if (bg != NO_BG_ERASE)
    {
        bmp_fill(bg, x, y, tw, th);
    }

    for (int dy = 0; dy < th; ++dy)
    {
        if (y+dy <= BMP_H_MINUS || y+dy >= BMP_H_PLUS)
        {
            break;
        }
        const uint8_t* srow = g + (uint32_t)(dy * sh / th) * swb;
        for (int dx = 0; dx < tw; ++dx)
        {
            const int sx = dx * sw / tw;
            if (srow[sx >> 3] & (1 << (sx & 7)))
            {
                bmp_putpixel_fast(bmp, x+dx, y+dy, fg);
            }
        }
    }

    ml_refresh_display_needed = 1;
    return tw;
}

/* Decode the next UTF-8 codepoint from *sp, advancing the pointer.
 * Invalid sequences decode as '?' but always consume at least the lead
 * byte, so callers walking a string always make progress. */
static int utf8_next(const char** sp)
{
    const unsigned char* s = (const unsigned char*)*sp;
    const unsigned char* s0 = s;
    unsigned char c = *s++;
    if (c < 0x80)
    {
        *sp = (const char*)s;
        return c;
    }

    int cp;
    int extra;
    if ((c & 0xE0) == 0xC0)      { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else
    {
        *sp = (const char*)s;        /* bad lead byte: consume it */
        return '?';
    }

    for (int i = 0; i < extra; i++)
    {
        unsigned char cc = *s;
        if ((cc & 0xC0) != 0x80)
        {
            *sp = (const char*)(s0 + 1);  /* bad sequence: consume lead only */
            return '?';
        }
        cp = (cp << 6) | (cc & 0x3F);
        s++;
    }

    *sp = (const char*)s;
    return cp;
}

#ifdef CONFIG_NO_BFNT
/* kitor: Those replace entries in platform consts.h for DIGIC6+
 *        that does not use BMP fonts anymore.  */
uint8_t *BFNT_CHAR_CODES;
uint8_t *BFNT_BITMAP_OFFSET;
uint8_t *BFNT_BITMAP_DATA;

bfnt_font* BFNT_FONT;
#endif

//-------------------------------------------------------------------

static font *new_font() {
    // allocate font from cached memory
    font *f = malloc(sizeof(font));
    if (f) {
        memset(f,0,sizeof(font));      // wipe memory
        // return address in cached memory
        return f;
    }

    // memory not allocated ! should probably do something else in this case ?
    return 0;
}

uint32_t font_by_name(char *file, uint32_t fg_color, uint32_t bg_color)
{
    /* check if this font was already loaded */
    for(int pos = 0; pos < (int)dyn_fonts; pos++)
    {
        if(!strcmp(file, dyn_font_name[pos]))
        {   
            /* yeah, so return a new specifier for this id */
            return FONT_DYN(pos, fg_color, bg_color);
        }
    }
    
    /* too many fonts loaded, return default font */
    if(dyn_fonts >= MAX_DYN_FONTS)
    {
        beep();
        bmp_printf(FONT_CANON, 0, 0, "Too many fonts");
        return FONT_CANON;
    }
    
    /* was not loaded, try to load */
    char filename[128];
    snprintf(filename, sizeof(filename), "ML/FONTS/%s.RBF", file);
    
    uint32_t size;
    if((FIO_GetFileSize( filename, &size ) != 0) || (size == 0))
    {
        /* failed to load, return default font */
        beep();
        bmp_printf(FONT_CANON, 0, 0, "%s not found", file);
        return FONT_CANON;
    }
    
    void *font = new_font();
    
    /* load the font here */
    if(!rbf_font_load(filename, font, 0))
    {
        bmp_printf(FONT_CANON, 0, 0, "%s not loaded", file);
        free(font);
        return FONT_CANON;
    }

    /* now updated cached font name (not filename) */
    dyn_font_name[dyn_fonts] = malloc(strlen(filename) + 1);
    strcpy(dyn_font_name[dyn_fonts], file);
    
    /* and measure font sizes */
    font_dynamic[dyn_fonts].bitmap = font;
    font_dynamic[dyn_fonts].height = rbf_font_height((void*)font_dynamic[dyn_fonts].bitmap);
    font_dynamic[dyn_fonts].width = rbf_char_width((void*)font_dynamic[dyn_fonts].bitmap, '0');
    dyn_fonts++;

    return FONT_DYN(dyn_fonts - 1, fg_color, bg_color);
}

static void alloc_cTable(font *f) {

    // Calculate additional values for font
    f->width = 8 * f->hdr.charSize / f->hdr.height;
    f->charCount = f->hdr.charLast - f->hdr.charFirst + 1;

    // set width table to default value
    memset(f->wTable, f->width, 256);
    
    // allocate cTable memory

    // If existing data has been allocated then we are re-using the font data
    // See if it the existing cTable data is large enough to hold the new font data
    // If not free it so new memory will be allocated
    if ((f->cTable != NULL) && (f->cTableSizeMax < (f->charCount*f->hdr.charSize))) {
        free(f->cTable);  // free the memory
        f->cTable = NULL; // clear pointer so new memory is allocated
        f->cTableSizeMax = 0;
    }

    // Allocated memory if needed
    if (f->cTable == NULL) {
        // Allocate memory from cached pool
        int size = f->charCount*f->hdr.charSize;
        f->cTable = malloc(size);

        // save size
        f->cTableSize = f->charCount*f->hdr.charSize;
        if (f->cTableSizeMax == 0) f->cTableSizeMax = f->cTableSize;    // Save actual size allocated
    }
}

//-------------------------------------------------------------------
// Return address of 'character' data for specified font & char
static inline char* FAST rbf_font_char(font* f, int ch)
{
    if (f && (ch >= f->hdr.charFirst) && (ch <= f->hdr.charLast))
    {
        return &f->cTable[(ch-f->hdr.charFirst)*f->hdr.charSize];
    }

    return 0;
}
//-------------------------------------------------------------------
// Read data from SD file using uncached buffer and copy to cached
// font memory
static int font_read(FILE* fd, unsigned char *dest, int len)
{
    // Return actual bytes read
    int bytes_read = 0;

    unsigned char *ubuffer = fio_malloc(len);
    
    if (ubuffer)
    {
        // Read block and copy to dest
        bytes_read += FIO_ReadFile(fd, ubuffer, len);
        memcpy(dest, ubuffer, len);
        fio_free(ubuffer);
    }

    return bytes_read;
}
//-------------------------------------------------------------------
// Load from from file. If maxchar != 0 limit charLast (for symbols)
static int rbf_font_load(char *file, font* f, int maxchar)
{
    int i;

    // make sure the font has been allocated
    if (f == 0)
    {
        return 0;
    }

    // open file (can't use fopen here due to potential conflict FsIoNotify crash)
    FILE *fd = FIO_OpenFile(file, O_RDONLY | O_SYNC);
    if (!fd)
    {
        return 0;
    }
    
    // read header
    i = font_read(fd, (unsigned char*)&(f->hdr), sizeof(font_hdr));

    // check size read is correct and magic numbers are valid
    if ((i != sizeof(font_hdr)) || (f->hdr.magic1 != RBF_HDR_MAGIC1) || (f->hdr.magic2 != RBF_HDR_MAGIC2))
    {
        return 0;
    }
    
    if (maxchar != 0)
    {
        f->hdr.charLast = maxchar;
    }

    alloc_cTable(f);

    // read width table (using uncached buffer)
    memset(&f->wTable[0], 0, sizeof(f->wTable));
    FIO_SeekSkipFile(fd, f->hdr._wmapAddr, SEEK_SET);
    font_read(fd, (unsigned char*)&f->wTable[f->hdr.charFirst], f->charCount);

    // read cTable data (using uncached buffer)
    FIO_SeekSkipFile(fd, f->hdr._cmapAddr, SEEK_SET);
    font_read(fd, (unsigned char*)f->cTable, f->charCount*f->hdr.charSize);

    FIO_CloseFile(fd);

    /* hardcoded tab width: 4 spaces */
    f->wTable['\t'] = f->wTable[' '] * 4;

    return 1;
}

//-------------------------------------------------------------------
static inline int rbf_font_height(font *rbf_font) {
    return rbf_font->hdr.height;
}
//-------------------------------------------------------------------
/* ch is a Unicode codepoint (from utf8_next); ASCII/Latin-1 go through
 * the stock width table, CJK codepoints fall back to the shared CJK font */
static inline int rbf_char_width(font *rbf_font, int ch) {
    if (ch >= CJK_CODEPOINT_MIN && cjk_glyph_lookup(ch) >= 0)
        return cjk_char_width(rbf_font);
    if (ch < 0 || ch > 0xFF) ch = '?';
    return rbf_font->wTable[ch];
}

//-------------------------------------------------------------------
int rbf_str_width(font *rbf_font, const char *str) {
    int l=0;
    int maxl = 0;

    // Calculate how long the string is in pixels
    // and return the length of the longest line
    while (*str)
    {
        if (*str == '\n')
        {
            maxl = MAX(l, maxl);
            l = 0;
            str++;
        }
        else
        {
            int cp = utf8_next(&str);
            l += rbf_char_width(rbf_font, cp);
        }
    }
    maxl = MAX(l, maxl);

    return maxl;
}

int rbf_str_clipped_width(font *rbf_font, const char *str, int maxlen) {
    int l = 0;
    // Calculate how long the string is in pixels (possibly clipped to 'maxlen')
    while (*str) {
        int cp = utf8_next(&str);
        int w = rbf_char_width(rbf_font, cp);
        if (l+w > maxlen) break;
        l += w;
    }

    return l;
}

int rbf_strlen_clipped(font *rbf_font, const char *str, int maxlen) {
    int l = 0;
    // Calculate how long the string is in bytes (possibly clipped to 'maxlen' pixels)
    // note: only counts characters that fit, like the original byte-wise version
    char* str0 = (char*) str;
    while (*str) {
        const char* next = str;
        int cp = utf8_next(&next);
        int w = rbf_char_width(rbf_font, cp);
        if (l + w > maxlen) break;
        l += w;
        str = next;
    }

    return str - str0;
}

//-------------------------------------------------------------------
static void FAST font_draw_char(font *rbf_font, int x, int y, char *cdata, int width, int height, int pixel_width, int fontspec) {
    int xx, yy;
    uint8_t * bmp = bmp_vram();
    int fg = FG_COLOR(fontspec);
    int bg = BG_COLOR(fontspec);
    int x0 = fontspec & FONT_CONDENSED ? 1 : 0;
    
    // draw pixels for font character
    if (cdata)
    {
        if (bg != NO_BG_ERASE)
        {
            bmp_fill(bg, x, y, pixel_width, height);
        }

        for (yy=0; yy<height; ++yy)
        {
            if (y+yy <= BMP_H_MINUS || y+yy >= BMP_H_PLUS)
            {
                break;
            }
            for (xx=x0; xx<pixel_width; ++xx)
            {
                if(cdata[yy*width/8+xx/8] & (1<<(xx%8)))
                {
                    bmp_putpixel_fast(bmp, x+xx, y+yy, fg);
                }
            }
        }
    }
}

static void FAST font_draw_char_shadow(font *rbf_font, int x, int y, char *cdata, int width, int height, int pixel_width, int fontspec) {
    int xx, yy;
    uint8_t * bmp = bmp_vram();
    int fg = FG_COLOR(fontspec);
    int bg = BG_COLOR(fontspec);
    
    // draw pixels for font character
    if (cdata)
    {
        for (yy=0; yy<height; ++yy)
        {
            if (y+yy <= BMP_H_MINUS || y+yy >= BMP_H_PLUS)
            {
                break;
            }
            for (xx=0; xx<pixel_width; ++xx)
            {
                int px = (cdata[yy*width/8+xx/8] & (1<<(xx%8)));
                if (px)
                {
                    bmp_putpixel_fast(bmp, x+xx, y+yy, fg);
                    
                    /* shadow: background pixels are only drawn near a foreground pixel */
                    /* heuristic: usually there are much less fg pixels than bg, so it makes sense to look for neighbours here */
                    for (int xxx = MAX(xx-1, 0); xxx <= MIN(xx+1, pixel_width-1); xxx++)
                    {
                        for (int yyy = MAX(yy-1, 0); yyy <= MIN(yy+1, height-1); yyy++)
                        {
                            int pxx = (cdata[yyy*width/8+xxx/8] & (1<<(xxx%8)));
                            if (!pxx)
                            {
                                bmp_putpixel_fast(bmp, x+xxx, y+yyy, bg);
                            }
                        }
                    }
                }
            }
        }
    }
}

//-------------------------------------------------------------------
static int FAST rbf_draw_char(font *rbf_font, int x, int y, int ch, int fontspec) {
    // CJK codepoints: draw from the shared CJK glyph font
    if (ch >= CJK_CODEPOINT_MIN)
    {
        int idx = cjk_glyph_lookup(ch);
        if (idx >= 0)
            return cjk_draw_glyph(idx, x, y, fontspec, rbf_font);
        ch = '?';
    }

    // anything outside the single byte range of the stock fonts -> '?'
    if (ch < 0 || ch > 0xFF) ch = '?';

    // Get char data pointer
    char* cdata = rbf_font_char(rbf_font, ch);
    
    if (!rbf_font->cTable)
        bfnt_draw_char(ch, x, y, FG_COLOR(fontspec), BG_COLOR(fontspec));
    else if (fontspec & SHADOW_MASK)
        font_draw_char_shadow(rbf_font, x, y, cdata, rbf_font->width, rbf_font->hdr.height, rbf_font->wTable[ch], fontspec);
    else
        font_draw_char(rbf_font, x, y, cdata, rbf_font->width, rbf_font->hdr.height, rbf_font->wTable[ch], fontspec);

#if 0   /* fixme: breaks cursor in editor.lua */
    if (ch == '\t')
    {
        int tab_width = rbf_font->wTable[' '] * 4;
        return (x + tab_width) / tab_width * tab_width - x;
    }
#endif
    ml_refresh_display_needed = 1;
    return rbf_font->wTable[ch];
}


//-------------------------------------------------------------------
// Draw a string colored 'c1' with the character at string-position 'c' colored 'c2'.
static int rbf_draw_string_c(font *rbf_font, int x, int y, const char *str, int fontspec1, int c, int fontspec2) {
     int l=0, i=0;

     while (*str) {
          if (*str == '\n')
          {
              l = 0;
              y += rbf_font->hdr.height;
              str++;
              i++;
              continue;
          }
          int cp = utf8_next(&str);
          l+=rbf_draw_char(rbf_font, x+l, y, cp, (i==c)?fontspec1:fontspec2);
          ++i;
     }
     return l;
}

//-------------------------------------------------------------------
static int rbf_draw_string_simple(font *rbf_font, int x, int y, const char *str, int fontspec) {
    return rbf_draw_string_c(rbf_font, x, y, str, fontspec, -1, fontspec);
}

//-------------------------------------------------------------------
static int rbf_draw_clipped_string(font *rbf_font, int x, int y, const char *str, int fontspec, int maxlen)
{
    int i = 0;
    int l = 0;
    
    int justified = (fontspec & FONT_ALIGN_MASK) == FONT_ALIGN_JUSTIFIED;
    
    if (justified)
    {
        int should_fill = !(fontspec & SHADOW_MASK);
        int bg = FONT_BG(fontspec);
        int len = maxlen;
        int space = len - rbf_str_width(rbf_font, str);
        int is_mono = rbf_char_width(rbf_font, 'm') == rbf_char_width(rbf_font, 'i');
        
        /* divide the space across the chars: a space character can accept 5 times more stretch space than a regular letter */
        /* first non-letter (indent) can't be stretched (to render bullet points correctly) */
        /* monospaced fonts are condensed uniformly */
        int bins = 0;
        char* c = (char*) str;
        int indent = 1;
        while (*c && *(c+1))    /* note: last char should not be stretched */
        {
            int ccp = utf8_next((const char**)&c);
            if (ccp != ' ' && ccp != '*') indent = 0;
            bins += is_mono ? 3 : indent ? 0 : space < 0 ? 1 : ccp == ' ' ? 10 : 2;
        }
        
        /* use Bresenham line-drawing algorithm to divide space with integer-only math */
        /* http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm#Algorithm_with_Integer_Arithmetic */
        /* x: from 0 to bins */
        /* y: space accumulated (from 0 to space) */
        int dx = bins;
        int dy = ABS(space);
        
        /* if it requires too much stretching, just stretch as much as we can */
        dy = MIN(dy, dx);
        
        int D = 2*dy - dx;

        // Draw chars from string up to max pixel length
        indent = 1;
        int condensed = 0;
        while (*str)
        {
            const char* str_next = str;
            int cp = utf8_next(&str_next);
            if (l+rbf_char_width(rbf_font, cp) > maxlen) break;
            str = str_next;

            l += rbf_draw_char(rbf_font, x+l, y, cp, fontspec | (condensed ? FONT_CONDENSED : 0) );
            int l0 = l;

            if (*str)      /* more characters follow (last char is not stretched) */
            {
                /* Bresenham step */
                if (cp != ' ' && cp != '*') indent = 0;
                int repeat = is_mono ? 3 : indent ? 0 : space < 0 ? 1 : *c == ' ' ? 10 : 2;
                condensed = 0;
                for (int i = 0; i < repeat; i++)
                {
                    if (D > 0)
                    {
                        l += SGN(space);
                        D = D + (2*dy - 2*dx);
                        if (space < 0) condensed = 1;
                    }
                    else
                    {
                        D = D + 2*dy;
                    }
                }
            }

            if (should_fill && l > l0)
            {
                bmp_fill(bg, x+l0, y, l-l0, rbf_font->hdr.height);
            }

            i++;
        }
    }
    else
    {
        // Draw chars from string up to max pixel length
        while (*str)
        {
            int cp = utf8_next(&str);
            if (l+rbf_char_width(rbf_font, cp) > maxlen) break;
            l+=rbf_draw_char(rbf_font, x+l, y, cp, fontspec);
            i++;
        }
    }
    return l;
}

//-------------------------------------------------------------------
static int rbf_draw_string_single_line(font *rbf_font, int x, int y, const char *str, int fontspec, int maxlen) {
    
    /* how much space do we have to draw the string? */
    int len = maxlen;
    
    // Calulate amount of padding needed
    int padding = len - rbf_str_clipped_width(rbf_font, str, len);

    /* is that padding on the left? on the right? or both? */
    int padding_left = 0;
    int padding_right = 0;
    
    switch (fontspec & FONT_ALIGN_MASK)
    {
        case FONT_ALIGN_LEFT:
            padding_right = padding;
            break;
        
        case FONT_ALIGN_CENTER:
            x -= len/2;
            padding_left = padding/2;
            padding_right = padding - padding/2;
            break;
        
        case FONT_ALIGN_RIGHT:
            padding_left = padding;
            x -= len;
            break;
        
        case FONT_ALIGN_JUSTIFIED:
            padding_left = 0;
            padding_right = 0;
    }
    
    int bg = FONT_BG(fontspec);
    int should_fill = fontspec & FONT_ALIGN_FILL;

    // Fill left padding with background color
    if (should_fill && padding_left)
        bmp_fill(bg, x, y, padding_left, rbf_font->hdr.height);

    // Draw chars
    rbf_draw_clipped_string(rbf_font, x + padding_left, y, str, fontspec, len - padding_right - padding_left);

    // Fill right padding with background color
    if (should_fill && padding_right)
        bmp_fill(bg, x+len-padding_right, y, padding_right, rbf_font->hdr.height);

    return len;
}

int rbf_draw_string(font *rbf_font, int x, int y, const char *str, int fontspec) {

    /* how much space do we have to draw the string? */
    int len = FONT_GET_TEXT_WIDTH(fontspec);

    /* choose some reasonable defaults for text width, if it's not specified */
    if (len == 0)
    {
        if ((fontspec & FONT_ALIGN_MASK) == FONT_ALIGN_LEFT && (fontspec & FONT_ALIGN_FILL) == 0)
        {
            /* no fancy alignment */
            return rbf_draw_string_simple(rbf_font, x, y, str, fontspec);
        }
        else
        {
            /* use natural string length */
            len = rbf_str_width(rbf_font, str);
        }
    }

    char* start = (char*) str;
    char* end = start;

    /* for each line in string */
    while (*start)
    {
        /* where does this line end? */
        while (*end && *end != '\n')
            end++;
        
        /* chop the string here */
        char old = *end;
        *end = 0;
        
        /* draw this line */
        rbf_draw_string_single_line(rbf_font, x, y, start, fontspec, len);
        
        /* finished? */
        if (old == 0)
            break;
        
        /* undo chopping */
        *end = old;
        
        /* okay, let's go to next line */
        y += rbf_font->hdr.height;
        start = end = end+1;
    }
    
    return len;
}


/* for compatibility with existing code */
struct font font_small;
struct font font_med;
struct font font_med_large;
struct font font_large;
struct font font_canon;


#ifdef CONFIG_NO_BFNT
//-------------------------------------------------------------------
// Load bitmap.bfn from card
static int bfnt_load_from_card()
{
    char filename[128];
    //TODO? make it configurable?
    snprintf(filename, sizeof(filename), "ML/FONTS/bitmap.bfn");
    uint32_t size;
    if( FIO_GetFileSize( filename, &size ) != 0 )
        return 1;

    DryosDebugMsg(0, 15, "File '%s' size %d bytes", filename, size);

    BFNT_FONT = (bfnt_font*)read_entire_file(filename, (int*)&size);
    if( BFNT_FONT == NULL ){
        DryosDebugMsg(0, 15, "BFNT read failed.");
        return 2;
    }
    DryosDebugMsg(0, 15, "read ok");

    if( BFNT_FONT->magic != 0x00544e46) { // "FNT\0"
        DryosDebugMsg(0, 15, "Font magic incorrect: 0x%08x", BFNT_FONT->magic);
        free(BFNT_FONT);
        return 3;
    }

    return 0;
}
#endif

    /* must be called before menu_init, otherwise it can't measure strings */
    void _load_fonts()
    {
        /* tolerate multiple calls, but only run the first */
        static int fonts_loaded = 0;
        if (fonts_loaded)
            return;
        fonts_loaded = 1;

        /* shared CJK glyph font (optional; absent => CJK shows as '?') */
        cjk_font_load();

        int bfnt_status = -1;

    #ifdef CONFIG_NO_BFNT
    //Try to load BFNT from card on newer generations
    bfnt_status = bfnt_load_from_card();
    if ( bfnt_status == 0) {
        DryosDebugMsg(0, 15, "bfnt read OK: %08x %s",  BFNT_FONT, BFNT_FONT->name);
        DryosDebugMsg(0, 15, "hdr %08x char %08x", BFNT_FONT->charmap_offset, BFNT_FONT->charmap_size);

        //setup variables that would be constants from ROM on old generations
        BFNT_CHAR_CODES    = (uint8_t *)BFNT_FONT + BFNT_FONT->charmap_offset;
        BFNT_BITMAP_OFFSET = BFNT_CHAR_CODES + BFNT_FONT->charmap_size;
        BFNT_BITMAP_DATA   = BFNT_BITMAP_OFFSET + BFNT_FONT->charmap_size;

        DryosDebugMsg(0, 15, "%08x %08x %08x", BFNT_CHAR_CODES, BFNT_BITMAP_OFFSET, BFNT_BITMAP_DATA);
    }
    else{
        DryosDebugMsg(0, 15, "bfnt read fail: %d", bfnt_status);
    }
    #else
    //If constants are not zero - assume BFNT is available from ROM
    if(BFNT_CHAR_CODES && BFNT_BITMAP_OFFSET && BFNT_BITMAP_DATA)
        bfnt_status = 0;
    #endif

    /* fake font for Canon BFNT font backend, with the same metrics */
    font *canon_font = new_font();
    canon_font->hdr.height = 40;
    for (int i = 0; i < 256; i++)
        canon_font->wTable[i] = bfnt_char_get_width(i);

    /* Try to load some RBF fonts */
    font_by_name("term12", COLOR_BLACK, COLOR_WHITE);
    font_by_name("term20", COLOR_BLACK, COLOR_WHITE);
    #ifdef CONFIG_LOW_RESOLUTION_DISPLAY
    font_by_name("arghlf22", COLOR_BLACK, COLOR_WHITE);
    #else
    font_by_name("argnor23", COLOR_BLACK, COLOR_WHITE);
    #endif
    font_by_name("argnor28", COLOR_BLACK, COLOR_WHITE);
    font_by_name("argnor32", COLOR_BLACK, COLOR_WHITE);

    if (bfnt_status == 0) {
        /* use BFNT font as fallback */
        for (int i = dyn_fonts; i <= MAX_DYN_FONTS; i++)
        {
            font_dynamic[i].bitmap = (void*) canon_font;
            font_dynamic[i].height = 40;
            font_dynamic[i].width = rbf_char_width((void*)font_dynamic[i].bitmap, '0');
        }

        font_canon = *fontspec_font(FONT_CANON);
    }
    else {
        /* use last loaded RBF font as a fallback */
        font_by_name("argnor32", COLOR_BLACK, COLOR_WHITE);
        if(dyn_fonts)
        {
            for (int i = dyn_fonts; i <= MAX_DYN_FONTS; i++)
                font_dynamic[i] = font_dynamic[dyn_fonts - 1];
        }

        /* Replace missing FONT_CANON with FONT_LARGE */
        font_canon = *fontspec_font(FONT_LARGE);
    }

    font_small = *fontspec_font(FONT_SMALL);
    font_med = *fontspec_font(FONT_MED);
    font_med_large = *fontspec_font(FONT_MED_LARGE);
    font_large = *fontspec_font(FONT_LARGE);
}
