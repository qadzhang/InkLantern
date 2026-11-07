# Magic Lantern 中文版 — 交付说明

## 产物

| 文件 | 适用机型 | 固件要求 |
|---|---|---|
| `magiclantern-EOSM.202-中文版.zip` | 佳能 EOS M | 佳能固件 2.0.2 |
| `magiclantern-6D.116-中文版.zip` | 佳能 EOS 6D | 佳能固件 1.1.6 |
| `字体渲染预览.png` | — | 固件内实际点阵字库的渲染效果（所见即相机所得） |

zip 内含 `autoexec.bin`、`ML/fonts/cjk.rbf`（中文字库）与全部中文模块，
解压覆盖到 SD 卡根目录即可（覆盖升级前建议备份 `ML/SETTINGS` 保留配置）。

## 常用设置位置（汉化菜单）

| 需求 | 路径 |
|---|---|
| 解除 LiveView 30 分钟自动关闭 | 佳能菜单：自动关闭电源 → 禁用；ML：设置页签（内部名 Prefs）→ 实时取景省电 → 30分钟定时器 → 禁用（LED 每 10 秒闪烁为正常提醒） |
| 中文字体 | 自动加载（`ML/fonts/cjk.rbf`），无需设置 |
| 页签一览 | 屏幕显示中文页签（音频/曝光/视频/…，内部英文键保启动安全） |

## 文档索引

| 文档 | 内容 |
|---|---|
| `/AGENTS.md` | AI 代理操作规范（构建命令、硬性规则、验证清单） |
| `/汉化说明.md` | 改造设计与规范（渲染层/字库格式/翻译规范/构建适配/测试体系） |
| `/logs/worklog.md` | 过程记录（本目录及各文档不含日志内容） |
| `/tools/cjk/TRANSLATE_RULES.md` | 翻译任务规则与术语表 |

## 重新构建

```bash
bash tools/cjk/build_cams.sh        # 全量构建双机型
```

构建依赖**不入 git**，首次构建前准备（一次性）：

```bash
# 1) ARM 工具链 gcc 4.8.4 2014q3 解压到 toolchain/（Launchpad 官方包，
#    gcc-arm-none-eabi-4_8-2014q3-20140805-linux.tar.bz2，61,804,575 字节，
#    MD5 acc8c8ff45f8801e2155934214309a87）
mkdir -p toolchain && tar xjf gcc-arm-none-eabi-4_8-*.tar.bz2 -C toolchain
# 2) docutils 装进本地目录（rst2html 包装脚本在 tools/cjk/bin/）
pip3 install --target toolchain/pylibs docutils
```

## 注意

- 未经真机刷写验证，首次上机请遵循 ML 官方安全流程（可拔电池恢复）；
- 模块 Help 长文（README）与调试日志保留英文；ISO/EV/RAW 等缩写按惯例保留。
