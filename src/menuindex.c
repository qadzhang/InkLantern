#include "dryos.h"
#include "version.h"
#include "bmp.h"
#include "menu.h"
#include "menuhelp.h"

extern void menu_easy_advanced_display(void* priv, int x0, int y0, int selected);

static void menu_nav_help_open(void* priv, int delta)
{
    menu_help_go_to_label("Magic Lantern menu", 0);
}

static MENU_UPDATE_FUNC(user_guide_display)
{
    MENU_SET_VALUE("");
}

#ifndef CONFIG_DIGIC_678X
// D678X cams don't seem to have ICON_MAINDIAL,
// as they don't have the same built-in fonts.
static MENU_UPDATE_FUNC(set_scrollwheel_display)
{
    if (info->can_custom_draw)
    {
        int x = bmp_string_width(MENU_FONT, entry->name) + 40;
        bfnt_draw_char(ICON_MAINDIAL, x, info->y - 5, COLOR_WHITE, NO_BG_ERASE);
    }
}
#endif

/* config.c */
extern int _set_at_startup;

static struct menu_entry help_menus[] = {
    {
        .select = menu_nav_help_open,
        .name = "按 " INFO_BTN_NAME,
        .choices = CHOICES("上下文帮助"),
    },
    {
        .select = menu_nav_help_open,
        #if defined(CONFIG_500D)
        .name = "按 " SYM_LV " / 回放",
        .choices = CHOICES("打开子菜单（Q）"),
        #elif defined(CONFIG_50D)
        .name = "按 FUNC / 回放",
        .choices = CHOICES("打开子菜单（Q）"),
        #elif defined(CONFIG_5D2)
        .name = "照片风格 / 回放",
        .choices = CHOICES("打开子菜单（Q）"),
        #elif defined(CONFIG_5DC) || defined(CONFIG_40D)
        .name = "按 JUMP / 回放",
        .choices = CHOICES("打开子菜单（Q）"),
        #elif defined(CONFIG_EOSM)
        .name = "点按或按回放",
        .choices = CHOICES("打开子菜单（Q）"),
        #elif defined(CONFIG_100D)
        .name = "按 Av / 回放",
        .choices = CHOICES("打开子菜单（Q）"),
        #else
        .name = "按 Q / 回放",
        .choices = CHOICES("打开子菜单"),
        #endif

        .children =  (struct menu_entry[]) {
            {
                .name = "……就像这样 :)",
            },
            MENU_EOL,
        },
    },
    #if defined(CONFIG_5D2) || defined(CONFIG_50D)
    {
        .name = "长按摇杆",
        .select = menu_nav_help_open,
        .choices = CHOICES("打开子菜单"),

        .children =  (struct menu_entry[]) {
            {
                .name = "……就像这样 :)",
            },
            MENU_EOL,
        },
    },
    #endif
    {
        .select  = menu_nav_help_open,
        #ifdef CONFIG_DIGIC_678X
        .name    = "SET / 主拨盘",
        // scroll wheel / main dial icon doesn't exist on D678X, no built in fonts
        #else
        .name    = "SET / ",
        .update  = set_scrollwheel_display,
        #endif
        .choices = CHOICES("编辑数值"),
    },
    {
        .select = menu_nav_help_open,
        #ifdef CONFIG_500D
        .name = "放大",
        #else
        .name = SYM_LV" 或放大",
        #endif
        .choices = CHOICES("实时取景中编辑"),
    },
    #ifdef FEATURE_JUNKIE_MENU
    {
        .select = menu_nav_help_open,
        .name = "按 MENU",
        .choices = CHOICES("极客模式"),
    },
    #endif
    #if defined(FEATURE_OVERLAYS_IN_PLAYBACK_MODE) && defined(BTN_ZEBRAS_FOR_PLAYBACK_NAME) && defined(BTN_ZEBRAS_FOR_PLAYBACK)
    /* if BTN_ZEBRAS_FOR_PLAYBACK_NAME is undefined, you must define it (or undefine FEATURE_OVERLAYS_IN_PLAYBACK_MODE) */
    {
        .select = menu_nav_help_open,
        .name = "按 "BTN_ZEBRAS_FOR_PLAYBACK_NAME,
        .choices = CHOICES("叠加（仅回放）"),
    },
    #endif
    #ifdef FEATURE_ARROW_SHORTCUTS
    {
        .select = menu_nav_help_open,
        .name = "按 "ARROW_MODE_TOGGLE_KEY,
        .choices = CHOICES("快捷键（仅实时取景）"),
    },
    #elif defined(ARROW_MODE_TOGGLE_KEY)
    #error Please remove unused definition of ARROW_MODE_TOGGLE_KEY.
    #endif
    {
        .select = menu_nav_help_open,
        .name = "开机时按 SET",
        .priv = &_set_at_startup,
        .max  = 1,
        .icon_type = IT_ACTION,
        .choices = CHOICES("跳过加载 ML", "加载 ML 必需"),
        .help = "修改此设置：设置 → 配置选项",
    },
    {
        .name = "快捷键",
        .select = menu_help_go_to_label,
    },
#if 0 // disable old, broken help system
    {
        .name = "Complete user guide",
        .select = menu_open_submenu,
        .children =  (struct menu_entry[]) {
            #include "menuindexentries.h"
            MENU_EOL
        },
    },
#endif
#if 0 // disable menu that always says there's no help files
      // FIXME: turn this into a generic about ML page
    {
        .name = "About Magic Lantern",
        .select = menu_help_go_to_label,
    },
#endif
};

static void
help_menu_init( void* unused )
{
    menu_add("Help", help_menus, COUNT(help_menus));
}

INIT_FUNC( "help_menu", help_menu_init );
