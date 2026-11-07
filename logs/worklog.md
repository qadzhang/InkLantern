# 工作日志（log 记录专用文件）

> 本文件是本项目唯一的过程记录存放处。AGENTS.md、汉化说明.md、dist/README.md
> 等文档不写任何日志性内容，历史与过程信息一律追加到本文件。

## 2026-11-07 汉化版首版交付（6D.116 + EOSM.202）

### 阶段记录

1. **调研**：确认现行官方源为 GitHub `reticulatedpines/magiclantern_simplified`（dev 分支，
   2025 Midsummer 构建基底）；6D 用 `platform/6D.116`，EOS M 用 `platform/EOSM.202`；
   每机型 `modules.included` 决定进 zip 的模块清单（两机型相同 20 个，EOSM 少 dot_tune）。
2. **工具链**：Launchpad 官方 `gcc-arm-none-eabi-4_8-2014q3-20140805-linux.tar.bz2`
   （61,804,575 B，MD5 acc8c8ff45f8801e2155934214309a87），解压至 `toolchain/`；
   docutils 以 pip `--target` 装进 `toolchain/pylibs/`，`toolchain/bin/rst2html` 为包装脚本。
   下载过程曲折：launchpadlibrarian 不支持 Range，多轮重试，一度被自设的 560s 超时
   在 61.29MB/61.80MB 处截断，放宽至 900s 后成功。
3. **中文渲染层**（`src/rbf_font.c`）：UTF-8 解码器 + 共享 CJK 字库 + 全绘制路径改造。
   宿主机像素级验收（`tools/cjk/host_test/`）抓出并修复 2 个真实缺陷：
   - justified 分支 `*(str+1)` 越界读（重排 str 推进顺序引入）；
   - `rbf_strlen_clipped` 语义漂移（旧版只计"放得下"的字节）。
   另发现并接受 2 项库存行为：绘制函数会临时写输入串；`y<=BMP_H_MINUS` 会跳过 y=0 行。
4. **字库**：Noto Sans CJK SC Medium 16px 光栅化（Regular 在 1-bit 下笔画 1px 过细，
   经 Regular/Medium/Bold 三组实测对比后定 Medium；Bold 未匹配到实际文件）。
   首版生成器头部长度与 C 端结构不一致（24 vs 20 字节），被"字体回读预览"环节当场抓获。
   最终字符集从翻译后源码自动派生：937 字 / 31,878 B。
5. **翻译**：12 个并行子代理 + 人工收敛，覆盖 src/ 全部用户可见文件与 20 个默认模块，
   约 3000 条字符串。风险点全部按配对规则处理（见汉化说明.md §3）。
   未覆盖文件后期补翻：patch.c / gps.c / console.c / af_patterns.c / crop-mode-hack.c。
6. **构建适配（gcc 4.8 vs 2025 主线的新 GCC 假设）**，逐项如下：
   - `-Wnull-dereference`：GCC 6+ 选项 → 从 Makefile.globals 移除；
   - 指定初始化器/聚合初始化告警 → 加 `-Wno-missing-field-initializers`、`-Wno-missing-braces`；
   - `#pragma GCC diagnostic "-Wnonnull-compare"`（stdio.c）→ 加 `-Wno-error=pragmas`；
   - `lua_lens.c` maybe-uninitialized 误报 → 加 `-Wno-error=maybe-uninitialized`；
   - `ar x --output`（新 binutils 选项，platform/Makefile、installer/Makefile、modules/lua/Makefile
     共 5 处）→ 改写为 `cd $(BUILD_DIR) && $(AR) x $(abspath $?) $(notdir ...)`；
   - `modules/raw_video/raw_vidx/mlv_3.c`：case 标签后直接声明（C23 前非法）→ 补空语句 ×8；
   - `src/ml_socket.h`：`struct timeval` 原型作用域问题 → 补前置声明。
7. **EOSM 30 分钟定时器需求**：确认 2025 主线已吸收 lua_fix 特性
   （`CONFIG_INT("idle.dis.30min",...)` + `powersave_prolong()`，两机型 internals.h 均开
   CONFIG_PROP_REQUEST_CHANGE），在 autoexec.bin 二进制层 5/5 探针命中，无需合并旧包。
   官方实验构建 zip 因 Cloudflare 人机验证在沙箱内不可下载（拿到挑战页，5,502 B HTML）。
8. **交付**：`dist/` 下双机型 zip + 字体渲染预览 + README；二进制探针验证通过
   （页签名、模块中文名、cjk.rbf 尺寸、模块数 19/20）。

### 遗留与边界

- 未在真机刷写验证（环境无相机）；
- 模块 README.rst 帮助长文、DryosDebugMsg/console 调试日志保留英文（按设计）；
- `scripts/api_test.lua` 已同步中文页签/条目名（251 处），其中个别"前缀匹配"正向
  断言（如 `"Brack"`）在新名下不再成立，属测试脚本预期内降级；
- lvinfo 内部 `.name`（lens.c/histogram.c 等不渲染的标识符）保留英文；
- module.c 7 字节短状态码（OFF/OK/Err…）因缓冲区限制保留英文，语义由长状态中文表达。

## 2026-11-07 EOSM 实机故障修复（v2 构建）

### 用户反馈

EOSM 实机更新首版后：开机读卡指示灯长亮很久才灭，随后无法进入 ML 菜单。

### 诊断

1. 体积理论被数据否决：英文基线（git worktree + 兼容补丁，同工具链）autoexec
   = 463,296 B，中文版 = 464,640 B（仅 +1.3KB；中文 UTF-8 常比英文短语更短）；
   模块 .mo 尺寸 ±0.1KB 持平。
2. 真因定位：`cjk_font_load` 首版用 stock `read_entire_file`，其缓冲来自
   `fio_malloc` = MEM_TEMPORARY|DMA 池且被永久持有（31.9KB）。佳能卡 I/O 与
   菜单操作依赖该池，EOSM 池最小——被占后开机模块加载（全程卡 I/O）迟滞、
   菜单无法打开。
3. 外部佐证（ryulin456/magiclantern-zh，2026-10，发布过含 EOSM.202 在内的
   20 机型中文包）：其 `read_entire_file_persistent()` 注释原文
   "must not remain allocated from shoot_malloc after the read, since Canon
   menu operations require that pool to be free"；CHDK 中文版为菜单严格子集
   字库（9.7–17.4KB）；ML 论坛 2016 中文 RBF 尝试帖为同源病例。

### 修复（v2）

1. `src/rbf_font.c`：加载器重写——fio_malloc 仅作 ≤8KB 瞬时跳板（即用即还），
   索引/位图常驻常规 cacheable malloc；头部/边界校验不变。
2. 字库再瘦身：字符集只从 C 字符串字面量提取（注释/文档汉字不入库，
   CHDK 实践），ALWAYS_INCLUDE 精简为 11 个标点：937 → 921 字 / 31,334 B。
3. `platform/EOSM.202/modules.hidden`：arkanoid/bench/selftest/edmac 默认
   不驻留（.hid 机制，卡上保留、模块菜单可手动加载），为 EOSM 留内存余量。
4. 宿主测试：FIO 存根改为真实 stdio（端到端覆盖分块加载路径）；修复一次
   存根替换未生效的测试事故（python replace 未加断言，教训已记）。
   43/43 PASS。
5. 重建双机型并二进制验证通过（EOSM autoexec 464,960B、4 个 .hid；
   6D 488,064B、全模块）。英文基线 worktree 已清理。

### 待实机复验

修复直接对标 ryulin456 已发布的 EOSM 同思路实现，但本构建仍需用户实机
确认：读卡灯应恢复正常节奏（数秒）、可进入中文 ML 菜单。

## 2026-11-07 EOSM 实机排障与 v3 交付（根因：中文页签名）

### 用户实机二分结果（每轮严格删 ML 目录+autoexec 后安装）

- B0 英文基线：正常 → 2025 dev 底座在本机无问题
- B1 英文+CJK 渲染层：正常 → 渲染层/字库加载无罪
- B2 中文无字库：失败（红灯常亮、菜单不可入）→ 翻译侧问题，与字体无关
- C2 英文核心+中文模块：正常 → 模块翻译无罪
- C1 中文核心(英文页签)+英文模块：正常 → 核心条目/帮助翻译无罪
- D1 中文页签+英文模块：失败 → **元凶=页签名汉化**
- D5 全中文+英文页签：功能正常可进菜单 → 交付配置

（注：诊断包系列因 worktree 的 zip 目录缓存未刷新 fonts，C1/D1/D5 均不含
cjk.rbf，"D5 不显示汉字"即此 packaging 疏漏所致，非代码问题。教训：
诊断构建也要 clean 或强刷 zip 目录。）

### 根因边界

- 页签名汉化（85 处调用语境令牌中的某种机制）在 EOSM 上引发启动级失败；
  与字体、字库加载、条目翻译、模块翻译均无关。
- 机制未明（menu_find_by_name 缺失即建 + ASSERT 路径静态分析均安全），
  静态未定位到崩溃点；按实证边界处理：页签名保留英文。

### v3 交付

- 主树落地与 D5 相同配置：67 个源文件 + api_test.lua 71 处页签令牌回退英文，
  menu.c startswith 的 skip 字节值还原（Movie=5/Focus=5/Expo=4）。
- 双机型重建验证：autoexec EOSM 465,024B / 6D 488,128B，cjk.rbf 31,334B 入包，
  中文条目探针 6/6，模块 19/20。
- 文档同步：AGENTS.md/汉化说明.md 改为"页签必须英文"铁律并记录实证依据。

## 2026-11-07 v4：显示层中文页签（对标 ryulin456 发布版架构）

- 解剖 ryulin456 EOSM 发布包（用户实机确认其中文正常）：autoexec.bin 内
  Audio/Expo/Overlay/Movie/Shoot/Focus/Debug/Prefs/Modules/Help 全部为英文
  内部名 → 中文在显示层实现。与本项目 v3 的"英文内部名"方向一致。
- v4 实现：`src/menu.c` 新增 `menu_tab_label()`——14 项英文→中文静态映射
  （13 页签 + Intervalometer/间隔拍摄），仅在 `menus_display()` 绘制选中页签
  标题处替换（FONT_CANON 路径同样经 CJK 渲染层出中文）。内部名、查找、
  挂载、脚本键全部不受影响。
- 双机型构建验证：EN 内部页签名 10/10、中文显示词 11/11、中文条目 4/4、
  cjk.rbf 31334B。产物已更新 dist/。

## 2026-11-07 v5：字号匹配（用户实机反馈英文明显大于中文）

- 现象：菜单拉丁字体 23px（argnor23）等语境下，固定 16px 的中文明显偏小
  （FONT_LARGE 32px / FONT_CANON 40px 语境差距更大）。
- 方案（ryulin456 同款 bfnt_*_scaled 思路）：字库升级为 24px 原生
  （921 字 × 24×3B = 68,174B，仍远小于其 EOSM 实证过的 133KB 常驻）；
  绘制时按当前基础字体高度最近邻缩放，CJK 步进 = 基础字体高度（方形字面），
  中英文光学尺寸一致。12px 语境为规整 ÷2 下采样。
- 渲染层：cjk_char_width(base)/cjk_draw_glyph(...,base) 改为高度感知；
  宿主测试更新几何断言并新增缩放专项（12px 下采样 / 32px 上采样 / 越界零容忍），
  49/49 PASS。
- 双机型重建验证：cjk.rbf 68,174B（24px×3B×921 字）入包，中文探针 5/5。
  dist/ 已更新。

## 2026-11-07 收尾：仓库清理、git 提交与文档全量同步

- 清理：三个构建目录（~58MB）、二分 worktree、过期 16px 预览图删除；
  rst2html 包装从 toolchain/bin 迁至 tools/cjk/bin（toolchain 整体 gitignore）。
- .gitignore 追加 toolchain/、tools/cjk/preview.png、!dist/*.zip（精确放行成品）。
- 提交 5c9ced6：97 文件（含双机型成品 zip、渲染层、工具、文档）。
- 文档全量同步（本次审计发现的过时项）：汉化说明（24px/921 字/49 项断言/
  缩放行为/已知边界重写）、AGENTS（PATH 新位置、49/49、toolchain 不入库）、
  TRANSLATE_RULES（页签规则反转为"禁止翻译"，附 menu_tab_label 映射表）、
  gen_cjk_font 文案、dist/README 页签路径。
