/** \file
 * Magic Lantern menu help
 */
/*
 * Copyright (C) 2011 Alex Dumitrache <broscutamaker@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "dryos.h"
#include "version.h"
#include "bmp.h"
#include "gui.h"
#include "config.h"
#include "property.h"
#include "lens.h"
#include "font.h"
#include "menu.h"
#include "menuhelp.h"

extern int menu_help_active;
static int current_page = 1;
static int help_pages = 100; // dummy value, will be updated on the fly

void 
draw_beta_warning()
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);

    bmp_printf(FONT_CANON, 242, 53, "Magic Lantern");

    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 150, "这是供测试用的开发版快照。");

    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, "请在 www.magiclantern.fm 报告所有错误。");

    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 250, "请谨慎用于正式拍摄工作。      ");

    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 300, "祝使用愉快！");

    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 350, "（按任意机身按键进入 ML 设置）");

    big_bmp_printf(FONT_MED,  10,  410,
        "Magic Lantern 版本：%s\n"
        "Git 提交：%s\n"
        "构建于 %s，构建者 %s。",
        build_version,
        build_id,
        build_date,
        build_user);
}

void 
draw_404_page()
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);

    bmp_printf(FONT_CANON, 10, 20, "404 未记录功能");

    bmp_printf(FONT_MED, 10, 100, "此功能可能尚未编写文档。");
    bmp_printf(FONT_MED, 10, 120, "毕竟我们是程序员，不是技术文档作者。");

    bmp_printf(FONT_MED, 10, 180, "不过……您可以直接试用，看看它有何作用。");

    bmp_printf(FONT_MED, 10, 240, "然后写一小段文字描述它，");
    bmp_printf(FONT_MED, 10, 260, "我们会将其收录进用户指南。");

    bmp_printf(FONT_MED, 10, 320, "谢谢！");

}

void 
draw_help_not_installed_page()
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);

    bmp_printf(FONT_CANON, 10, 20, "未找到帮助文件");

    bmp_printf(FONT_MED, 10, 150, "找不到 Magic Lantern 帮助文件。              ");

    bmp_printf(FONT_MED, 10, 200, "请确保所有 ML 文件已安装到存储卡。        ");

    bmp_printf(FONT_MED, 10, 250, "安装说明请见 http://wiki.magiclantern.fm/install。 ");
}

void menu_help_show_page(int page)
{
    menu_help_active = 1;
    
#ifndef CONFIG_RELEASE_BUILD
    if (page == 1) { draw_beta_warning(); return; } // display this instead of the main About page
#endif

    if (page == 0) { draw_404_page(); return; } // help page not found
    if (page == -1) { draw_help_not_installed_page(); return; } // help page not found
    
    char path[100];
    struct bmp_file_t * doc = (void*) -1;

#ifdef CONFIG_HELP_CACHE
    char rpath[30];
    snprintf(rpath, sizeof(rpath), "ML/doc/page-%03d.vrm", page);
    if (load_vram(rpath)==-1)
#endif
    {
        snprintf(path, sizeof(path), "ML/doc/page-%03d.bmh", page);
        doc = bmp_load(path, 1);
        if (!doc)
        {
            snprintf(path, sizeof(path), "ML/doc/page-%03d.bmp", page);
            doc = bmp_load(path, 1);
        }

        if (doc)
        {
            bmp_draw_scaled_ex(doc, 0, 0, 720, 480, 0);
            #ifdef CONFIG_HELP_CACHE
            extern int _bmp_draw_should_stop;
            if (!_bmp_draw_should_stop) save_vram(rpath);
            #endif
            free(doc);
        }
        else
        {
            clrscr();
            bmp_printf(FONT_MED, 0, 0, "无法加载帮助页 %s。", path);
        }
    }
}

void menu_help_redraw()
{
    BMP_LOCK( menu_help_show_page(current_page); );
}

void menu_help_next_page()
{
    current_page = MOD(current_page, help_pages) + 1;
    menu_help_active = 1;
}

void menu_help_prev_page()
{
    current_page = MOD(current_page - 2, help_pages) + 1;
    menu_help_active = 1;
}

void menu_help_go_to_page(int page)
{
    current_page = page;
    menu_help_active = 1;
}

void str_make_lowercase(char* s)
{
    while (*s) { *s = tolower(*s); s++; }
}

void menu_help_go_to_label(void* label, int delta)
{
    int page = 0; // if help page won't be found, will show 404
    if (is_menu_selected("Help"))
        page = 1; // don't show the 404 page in Help menu :P
    
    int size = 0;

    if (label == NULL)
        DryosDebugMsg(0, 15, "label was NULL");
    else
        DryosDebugMsg(0, 15, "label: %s", label);

    char* buf = (void*)read_entire_file("ML/doc/menuidx.dat", &size);
    if (!buf || !size)
        page = -1; // show "help not found" warning
    
    // trim spaces
    char label_adj[100];
    label_adj[0] = '\0';
    if (label != NULL)
    {   // this snprintf() implementation is not safe with NULL pointers
        snprintf(label_adj, sizeof(label_adj), "%s", label);
    }
    int i = strlen(label_adj);
    while (i > 0 && label_adj[i - 1] == ' ')
    {
        label_adj[i - 1] = '\0';
        i--;
    }
    str_make_lowercase(label_adj);

    int prev = -1;
    for (i = 0; i < size; i++)
    {
        if (buf[i] == '\n')
        {
            buf[i] = 0;
            char* line_buf = &buf[prev+1];

            char* name = line_buf+4;
            str_make_lowercase(name);
            int pagenum = atoi(line_buf);

            if(streq(name, label_adj))
                page = pagenum;
            help_pages = MAX(help_pages, pagenum);

            prev = i;
        }
    }

    fio_free(buf);
    
    current_page = page;
    menu_help_active = 1;
}
