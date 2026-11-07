/* Benchmarks */

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <menu.h>
#include <screenshot.h>
#include <console.h>
#include <version.h>
#include <property.h>
#include <zebra.h>
#include <edmac-memcpy.h>
#include <powersave.h>
#include <shoot.h>

#include "cache.c"

extern void peaking_benchmark();
extern void menu_benchmark();

/* fixme: how to use multiple files without exporting a bunch of symbols from the module? */
#include "card_bench.c"
#include "mem_bench.c"
#include "mem_perf.c"

static struct menu_entry bench_menu[] =
{
    {
        .name        = "性能测试",
        .select        = menu_open_submenu,
        .help = "测一测你的相机有多快。存储卡、CPU、图形...",
        .submenu_width = 650,
        .children =  (struct menu_entry[]) {
            {
                .name        = "存储卡测试",
                .select        = menu_open_submenu,
                .help = "CF 或 SD 存储卡性能测试",
                .children =  (struct menu_entry[]) {
                    {
                        .name = "快速读写测试(1分钟)",
                        .select = run_in_separate_task,
                        .priv = card_benchmark_task_quick,
                        .help = "用 16MB 缓冲区测试存储卡读写速度。会使用 1GB 临时文件。",
                        .help2 = "要测 RAW 视频，请在视频模式或回放(PLAY)模式下运行。"
                    },
                    {
                        .name = "CF+SD 写入测试(1分钟)",
                        .select = run_in_separate_task,
                        .priv = twocard_benchmark_task,
                        .help = "同时测试 CF 和 SD 两张卡的写入速度。",
                        .shidden = 1,   /* only appears if you have two cards inserted */
                    },
                    {
                        .name = "缓冲读写测试(5分钟)",
                        .select = run_in_separate_task,
                        .priv = card_benchmark_task_full,
                        .help = "测试各种缓冲区大小。做 RAW 视频基准测试时无需此项，",
                        .help2 = "但若想优化视频缓冲算法，可以试试。"
                    },
                    {
                        .name = "缓冲写入测试(不限时)",
                        .select = run_in_separate_task,
                        .priv = card_bufsize_benchmark_task,
                        .help = "用于寻找最佳写入缓冲区大小的实验。",
                        .help2 = "结果保存在 BENCH.LOG 中。"
                    },
                    MENU_EOL,
                },
            },
            {
                .name        = "内存测试",
                .select        = menu_open_submenu,
                .help = "内存或缓存性能测试",
                .children =  (struct menu_entry[]) {
                    {
                        .name = "Memcpy 测试(20秒)",
                        .select = run_in_separate_task,
                        .priv = mem_benchmark_simple_task,
                        .help = "测试不同子系统的 memcpy 速度。",
                        .help2 = "(memcpy、dma_memcpy、DMA 单元、EDMAC 单元)"
                    },
                    {
                        .name = "内存测试(1分钟)",
                        .select = run_in_separate_task,
                        .priv = mem_benchmark_task,
                        .help = "用不同方法测试内存读写速度。",
                        .help2 = "(可缓存、不可缓存、EDMAC、不同数据类型...)"
                    },
                    {
                        .name = "缓存测试(RAM)",
                        .select = run_in_separate_task,
                        .priv = mem_perf_test_cached,
                        .help = "通过基准测试检测 RAM 缓存大小(留意速度骤降点)。",
                        .help2 = "提示：加载 'plot' 模块可看到漂亮的曲线图。",
                    },
                    {
                        .name = "缓存测试(RAM，绕过缓存)",
                        .select = run_in_separate_task,
                        .priv = mem_perf_test_uncached,
                        .help = "验证速度骤降是否确实由缓存容量不足引起。",
                        .help2 = "提示：加载 'plot' 模块可看到漂亮的曲线图。",
                    },
                    {
                        .name = "缓存测试(ROM)",
                        .select = run_in_separate_task,
                        .priv = mem_perf_test_rom,
                        .help = "通过基准测试检测 ROM 缓存大小(留意速度骤降点)。",
                        .help2 = "提示：加载 'plot' 模块可看到漂亮的曲线图。",
                    },
                    MENU_EOL,
                },
            },
            {
                .name        = "其他测试",
                .select        = menu_open_submenu,
                .help = "峰值对焦与菜单后端的性能测试(目前仅此两项)",
                .children =  (struct menu_entry[]) {
                    {
                        .name = "峰值对焦测试(30秒)",
                        .select = run_in_separate_task,
                        .priv = peaking_benchmark,
                        .help = "测试峰值对焦在回放模式下的运行速度(1000 次迭代)。",
                        .help2 = "存储卡上需要有一张有效图像。"
                    },
                    {
                        .name = "菜单测试(10秒)",
                        .select = run_in_separate_task,
                        .priv = menu_benchmark,
                        .help = "测试菜单后端的速度。"
                    },
                    MENU_EOL,
                },
            },
            MENU_EOL,
        }
    },
};

/* fixme: only iterates the card benchmarks submenu */
static struct menu_entry * bench_menu_entry(const char* entry_name)
{
    /* menu entries are not yet linked, so iterate as in array, not as in linked list */
    for(struct menu_entry * entry = bench_menu[0].children[0].children ; !MENU_IS_EOL(entry) ; entry++ )
    {
        if (entry != NULL && entry->name != NULL && entry_name != NULL)
        {
            if (streq(entry->name, entry_name))
            {
                return entry;
            }
        }
    }
    return 0;
}

/* fixme: move to core */
static void bench_menu_show(const char* entry_name)
{
    struct menu_entry * entry = bench_menu_entry(entry_name);
    if (entry)
    {
        entry->shidden = 0;
    }
    else
    {
        console_show();
        printf("Could not find '%s'\n", entry_name);
    }
}


static void twocard_init()
{
    twocard_mq = (void*)msg_queue_create("twocard", 100);
    bench_menu_show("CF+SD 写入测试(1分钟)");
}

static unsigned int bench_init()
{
    int cf_present = is_dir("A:/");
    int sd_present = is_dir("B:/");
    
    if (cf_present && sd_present)
    {
        twocard_init();
    }
    
    menu_add("Debug", bench_menu, COUNT(bench_menu));
    
    return 0;
}

static unsigned int bench_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(bench_init)
    MODULE_DEINIT(bench_deinit)
MODULE_INFO_END()
