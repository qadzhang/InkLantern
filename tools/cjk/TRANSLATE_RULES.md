# Magic Lantern 汉化翻译规则（所有翻译代理必读）

## 目标
把指定 C 源文件里**用户在相机屏幕上能看到**的英文界面字符串翻译成简体中文（UTF-8）。
只改字符串字面量的内容，绝不改动任何代码结构、标识符、注释、宏、数字。

## 必须翻译的模式（仅限其中的英文字符串字面量）
1. 菜单项名字与帮助：`.name = "..."`、`.help = "..."`、`.help2 = "..."`、
   `MENU_ITEM("名字", "帮助", ...)` 老式宏里的名字和帮助参数
2. 选项列表：`CHOICES("选项1", "选项2", ...)`、`.choices = (char*[]) {...}`
3. 屏幕提示：`MENU_SET_WARNING(..., "...")`、`MENU_SET_RINFO("...")`、
   `MENU_SET_LABEL(..., "...")`、`MENU_SET_VALUE("...")`、`MENU_SET_HELP(...)`、`MENU_SET_ICON`
4. 屏幕文字：`bmp_printf(..., "...")`、`NotifyBox(..., "...")`、`notify_box(...)`、
   `bmp_draw_string`/`DrawBrowserString` 等里直接显示的字面量
5. 模块名描述：`MODULE_...` 宏中出现的**自然语言描述**（不要动模块文件名/标识符）
6. `info_str`/`get_label` 等回调返回的显示文本

## 绝对不翻译
1. `DryosDebugMsg(...)`、`console_printf(...)`、`qprintf`、`printf` —— 开发调试日志
2. 文件名/路径（"ML/LOGS/..."、"autoexec.bin"）、扩展名、配置键名
3. 任何被代码引用比较的字符串：翻译前先想一下这个字符串是否被
   `strcmp`/`streq`/`startswith`/`menu_find_by_name`/`is_menu_active` 引用。
   **.name 字段如果在本文件之外被引用（用 grep 全仓库查），跳过并在报告中列出。**
   **页签名（menu_add 的第一参数）一律保留英文，禁止翻译**（见下节）。
4. 格式串里的 `%d %s %x %%` 等占位符必须原样保留，位置不得移动
5. `\n` `\t` `\\` `\"` 转义序列保留
6. 纯技术缩写按术语表保留：RAW、ISO、EV、MLV、HDMI、USB、SD、CF、LV(可译"实时取景")、
   WB(白平衡)、ETTR、DNG、EXIF、HDR、FPS、KB/MB、dB、kHz、dBu、PTP、GCC… 
7. 数字/单位/版本号（"1.0.9"、"dB -40"）不动
8. C 注释（/* ... */ 和 // ...）一律不动

## 页签名：一律保留英文（禁止翻译）

实机验证：页签**内部名**改中文会导致 EOSM 开机红灯常亮、无法进入菜单
（机制未明）。屏幕上的中文页签名由 `src/menu.c::menu_tab_label()` 在绘制时
映射实现，翻译任务**不要**改动任何 menu_add/menu_find_by_name/is_menu_active
等调用里的页签名。需要新增页签中文显示时，在 menu_tab_label 表中加一行：
Audio→音频 Expo→曝光 Overlay→叠加 Movie→视频 Shoot→拍摄 Focus→对焦
Debug→调试 Prefs→设置 Modules→模块 Help→帮助 Display→显示 Scripts→脚本
Games→游戏 Intervalometer→间隔拍摄

## 常用术语表（保持全局一致）
Exposure 曝光 | Aperture 光圈 | Shutter 快门 | ISO 感光度 | Metering 测光 |
White Balance 白平衡 | Focus 对焦 | Focal length 焦距 | Zoom 变焦 |
LiveView 实时取景 | Playback 回放 | Record 录制 | Movie 视频/录制 |
Overlay 叠加 | Histogram 直方图 | Waveform 波形图 | Zebra 斑马纹 |
Cropmark 剪裁标记 | Focus peaking 峰值对焦 | Magic zoom 魔术变焦 |
Intervalometer 间隔拍摄 | Bulb B门 | Long exposure 长曝光 | Motion detect 运动检测 |
Silent picture 静音拍摄 | Dual ISO 双ISO | ETTR 向右曝光 | Bracket 包围 |
Flash 闪光灯 | Trigger 触发 | Battery 电池 | Temperature 温度 |
 shutter count 快门数 | File manager 文件管理 | Delete 删除 | Rename 重命名 |
Help 帮助 | Enable 启用 | Disable 禁用 | ON 开 | OFF 关 | Auto 自动 |
Global draw 全局绘制 | FPS override 帧率覆盖 | Bitrate 码率 | Audio 音频 |
Gain 增益 | Wind filter 风声滤波 | Input 输入 | Output 输出 | Monitor 监视 |
Digital dolly 数码滑轨 | Rack focus 焦点转移 | Follow focus 跟焦 | Trap focus 陷阱对焦 |
Self timer 自拍 | Mirror lockup 反光板预升 | HDR bracketing HDR包围 |
Module 模块 | Load 加载 | Unload 卸载 | Config 配置 | Settings 设置 |

## 风格要求
1. 菜单 `.name` 力求 ≤ 8 个汉字；帮助文本完整但紧凑
2. 中文标点用全角（，。：；！？（）、""）；括号内英文缩写用半角
3. 保留原文的 `\n` 行结构（帮助文本多行时保持行数大致对应）
4. 语序自然，像相机说明书，不要机器翻译腔
5. 提示/警告句式："无法在实时取景下使用" / "请先设置…" / "此功能需要…"
6. 缩略语第一次可带英文注释，如"向右曝光(ETTR)"

## 自检（改完每个文件后）
- `python3 -c "open('文件名',encoding='utf-8').read()"` 确认无编码损坏
- grep 确认没有把 % 占位符弄丢：对比翻译前后 `%` 出现次数（含 %% 计 2）
- 确认引号成对、没有把 `"` 翻进字符串里（用 “” 代替内嵌引号）
- 报告：翻译的文件、跳过的字符串及原因
