# 墨灯 InkLantern

**Magic Lantern 简体中文固件 · 现行官方源（magiclantern_simplified / 2025）汉化版**

为佳能 **EOS 6D（1.1.6）** 与 **EOS M（2.0.2）** 打造的 Magic Lantern 中文固件：
英文内核保启动兼容，界面全面中文，开箱即用的成品 zip 交付。

## 特性

- **自研 UTF-8 渲染层**（`src/rbf_font.c`）：原 RBF 字体系统按单字节绘制，
  本项目新增 UTF-8 解码与共享 CJK 字库，全部绘制路径（宽度/裁剪/对齐/
  两端拉伸）按码点重写——现有代码零改动即可显示中文
- **菜单级子集点阵字库**（`data/fonts/cjk.rbf`）：Noto Sans CJK SC Medium
  24px 光栅化，字符集从源码字符串自动派生（921 字 / 68KB），按当前字体高度
  最近邻缩放，中英文光学等大；“相机是操作界面，不是阅读器”
- **英文内部名 + 中文显示层**架构：页签内部名保持英文（实机验证中文内部名
  导致 EOS M 无法启动），屏幕页签由 `menu_tab_label()` 绘制时映射为中文
- **DMA 安全加载**：字库分块读入，DMA 池仅作瞬时跳板，常驻内存全部走
  常规 cacheable 池（佳能卡 I/O 与菜单操作依赖该池）
- **翻译覆盖**：核心全部菜单/帮助/提示 + 20 个默认模块，约 3000 条字符串；
  页签、条目、脚本查找键的字符串耦合全部成对同步
- **宿主机像素级验收**（49/49）：假 VRAM + 金丝雀 + ASAN，含缩放专项，
  相机固件改动在本机即可回归
- 附带 **LiveView 30 分钟限制解除**（上游已含的 lua_fix 特性，
  设置页签 → 实时取景省电 → 30分钟定时器 → 禁用）

## 交付物

| 文件 | 机型 |
|---|---|
| `dist/magiclantern-EOSM.202-中文版.zip` | EOS M（固件 2.0.2） |
| `dist/magiclantern-6D.116-中文版.zip` | EOS 6D（固件 1.1.6） |

## 构建

```bash
bash tools/cjk/build_cams.sh     # 全量构建双机型（依赖准备见 dist/README.md）
```

## 文档

| 文档 | 内容 |
|---|---|
| [AGENTS.md](AGENTS.md) | 代理/贡献者操作规范（硬性规则、验证门） |
| [汉化说明.md](汉化说明.md) | 改造设计规范（渲染层/字库格式/翻译配对机制） |
| [logs/worklog.md](logs/worklog.md) | 全程过程记录（含 EOSM 实机排障六轮二分） |
| [dist/README.md](dist/README.md) | 安装说明与工具链重建 |

## 致谢与许可

- 上游：[Magic Lantern](https://www.magiclantern.fm/) /
  [reticulatedpines/magiclantern_simplified](https://github.com/reticulatedpines/magiclantern_simplified)
- 先行者实证参考：[ryulin456/magiclantern-zh](https://github.com/ryulin456/magiclantern-zh)（显示层翻译架构、
  DMA 池纪律）、CHDK 中文版（菜单级子集字库理念）
- 本项目继承 Magic Lantern 的 **GPLv2** 许可（见 [COPYING](COPYING)）；
  字形基于 SIL OFL 许可的 Noto Sans CJK SC 光栅化生成

> ⚠️ 固件已在 EOS M 实机多轮验证（启动/菜单/字号），仍请遵循 ML 官方安全
> 流程使用；异常时删除卡上 `autoexec.bin` 与 `ML` 目录即恢复原厂。
