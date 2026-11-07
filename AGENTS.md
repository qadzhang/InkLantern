# AGENTS.md — 本仓库 AI 代理操作规范

本仓库是 Magic Lantern 官方源（`reticulatedpines/magiclantern_simplified`，dev 分支）
的**中文本地化工作副本**，交付目标：**6D.116** 与 **EOSM.202** 两个机型的汉化固件。
任何 AI 代理在本仓库工作前必须通读本文档。

## 1. 目录导航

| 路径 | 归属 | 说明 |
|---|---|---|
| `src/ modules/ platform/ data/ …` | 上游 | 官方代码（含本项目的汉化改动与构建适配） |
| `src/rbf_font.c` | 本项目核心改动 | UTF-8 + CJK 渲染层，改动前先读 `汉化说明.md §2` |
| `data/fonts/cjk.rbf` | 本项目生成物 | 中文字库，由生成器产出，**不要手改** |
| `tools/cjk/` | 本项目工具 | 字体生成器 / 宿主测试 / 构建脚本 / 翻译规则 |
| `toolchain/` | 本地依赖（**不入 git**） | gcc-arm-none-eabi 4.8.4 + docutils，重建方法见 dist/README.md；rst2html 包装在 `tools/cjk/bin/` |
| `dist/` | 交付物 | 双机型 zip、预览图、交付 README |
| `logs/worklog.md` | 日志 | **唯一**的过程记录存放处；任何文档不得掺日志 |
| `汉化说明.md` | 文档 | 改造设计与规范（格式、术语、配对规则） |

## 2. 构建与测试命令

```bash
# 全量构建双机型（含清理；模块改动必须连带清理，上游已知缺陷）
bash tools/cjk/build_cams.sh

# 增量构建单机型
cd platform/EOSM.202 && ARM_BINPATH=$PWD/../../toolchain/gcc-arm-none-eabi-4_8-2014q3/bin \
  PATH=$PWD/../../tools/cjk/bin:$PATH make -j8

# 改了任何用户可见字符串之后，必须按顺序：
python3 tools/cjk/gen_cjk_font.py            # 1) 重生成字库（字符集自动取自源码）
bash tools/cjk/host_test/run.sh              # 2) 渲染层回归（必须 49/49 PASS）
bash tools/cjk/build_cams.sh                 # 3) 重出双机型 zip
```

工具链版本是 **gcc 4.8.4 2014q3**，与上游 CI 的新 GCC 假设不同——见 §5 禁区。

## 3. 硬性规则：字符串配对三定律

ML 大量用**字符串做跨文件耦合**。改任何字符串前先 `grep -rn` 全仓库查引用，三类配对
**必须两侧（或全部引用点）同步修改**，漏一处就静默丢菜单/丢功能（不报编译错误）：

1. **页签内部名必须保持英文**（Audio/Expo/…/Games）——实机验证：内部改成中文
   会导致 EOSM 开机红灯常亮、无法进入 ML 菜单（机制待查，与字体无关）。
   中文页签显示由 `src/menu.c` 的 `menu_tab_label()` 在**绘制时**映射实现
   （同 ryulin456 发布版的架构：内部英文键 + 显示层翻译）；新增页签时在此表补条目。
   引用点形态：`menu_add / menu_remove / menu_find_by_name / is_menu_active /
   is_menu_selected / select_menu / select_menu_by_name / streq(menu->name,"…") /
   startswith(info->name,"…")`。页签内的菜单条目、帮助、提示全部中文，不受影响。
2. **MENU_PLACEHOLDER ↔ 模块 `.name`**：占位符按 `streq` 与真实条目配对替换
   （`menu.c` 的 `menu_update_placeholder`）。已汉化的配对例如：
   后期去闪烁 / 静音拍摄 / 自动向右曝光 / 双ISO / 照片风格 / 文件管理器 / 剪裁标记 /
   矢量示波器 / 剩余内存 / 间隔拍摄。
3. **脚本查找键**：`menu_get/set_value_from_script / menu_set_str_value_from_script /
   from_script / stress_test_toggle_menu_item` 的页签名、条目名、**选项值字符串**
   三层都要与真实菜单一致；`scripts/api_test.lua` 引用同一批名字，改 C 侧必同步 lua。

## 4. 翻译规范（摘要，全文见 `汉化说明.md §3` 与 `tools/cjk/TRANSLATE_RULES.md`）

- 只译用户可见字符串：`.name/.help/.help2/CHOICES/MENU_SET_*/bmp_printf/NotifyBox` 等字面量。
- **禁区**（不译）：`DryosDebugMsg/console_printf/printf` 调试日志；文件名/路径/配置键
  （`CONFIG_INT("idle.dis.30min",…)` 的键名）；lvinfo 内部 `.name`（不渲染）；定长小缓冲
  字段（如 module.c 7 字节短状态码）；技术缩写 ISO/EV/RAW/MLV/Tv/Av/dB 等。
- 格式守恒：`%` 占位符数量与位置不变；`\n` 转义保留；内嵌引号用全角“”；中文标点全角。
- 修改后跑 `tools/cjk/` 的占位符计数自检（对照 git HEAD）。

## 5. 构建系统禁区（gcc 4.8 兼容）

上游按新 GCC 编写，以下写法在 4.8 下会失败，**不要引入**；已有的豁免集中在
`Makefile.globals`（`-Wno-missing-field-initializers/-Wno-missing-braces/
-Wno-error=pragmas/-Wno-error=maybe-uninitialized`，且已移除 `-Wnull-dereference`）：

- GCC 6+ 专属警告选项 / pragma（`-Wnonnull-compare` 等）；
- `ar x --output`（binutils ≥2.31 选项）——用 `cd $(BUILD_DIR) && $(AR) x $(abspath $?) $(notdir …)`；
- case 标签后直接跟声明（C23 前非法，需空语句隔离）；
- 原型参数表内首次声明 struct（需文件级前置声明）。

## 5.5 内存池铁律（相机稳定性）

- **严禁长期持有 `fio_malloc`/`read_entire_file` 返回的内存**——它们来自
  MEM_TEMPORARY/DMA 池，佳能卡 I/O 与菜单操作依赖它（EOSM 最小最敏感）。
  需要常驻的数据：分块读入 `malloc`（cacheable 池）。参照
  `src/rbf_font.c` 的 `cjk_read_chunk`/`cjk_font_load` 与
  ryulin456 项目的 `read_entire_file_persistent`。
- EOSM 追加内存余量：`platform/EOSM.202/modules.hidden` 中的模块
  （arkanoid/bench/selftest/edmac）默认不驻留，卡上保留可手动加载。

## 6. 交付前验证清单

1. `bash tools/cjk/host_test/run.sh` → 49/49 PASS；
2. `python3 tools/cjk/gen_cjk_font.py` 报告的字符数覆盖所有新增中文字符（无缺字）；
3. 双机型 zip 产出，且 `autoexec.bin` 内探针命中页签名与关键条目名
   （参照 `dist/` 验证脚本的探针清单）；
4. `ML/fonts/cjk.rbf` 在 zip 内且为最新尺寸；
5. git diff 复核：改动只落在字符串字面量与本文件所列适配点，代码骨架零变化。

## 7. 其他约束

- 不 push、不向上游提交；所有本地改动留在工作区（或征得用户同意后本地 commit）。
- `zebra-5dc.c`、`modules.included` 之外的模块（mlv_rec/bolt_rec/io_crypt/adtg_log/
  script 等）不属于本交付目标，非必要不动。
- 磁盘紧张时 `toolchain/gcc-arm.tar.bz2` 已删可不清；`toolchain/` 目录本身必须保留。
- 过程记录一律追加到 `logs/worklog.md`，文档（含本文件）不写日志。
