# 红红子LAB / REDcoLAB

Clip Studio Paint **原生滤镜插件**（Windows / CSP 5.0 EX）。用 CELSYS 官方 **Triglav Plug-In SDK** 写成，
编译产物是导出 `TriglavPluginCall` 的 x64 原生模块，直接由 CSP 加载。

```
滤镜 → 红红子LAB → 一键灰度
```

参数面板由 CSP 自己渲染（和「工具属性」同一套界面）：**8 种灰度算法** + **强度滑块**，拖动时画布实时预览。

---

## 当前实现的功能

| 功能 | 状态 |
|---|---|
| 一键灰度（8 种算法 + 强度混合） | **已实现**，通过自动化测试 |
| 支持选区与羽化边缘 | **已实现**，通过自动化测试 |
| 不修改 Alpha / 跳过全透明像素 | **已实现**，通过自动化测试（逐像素断言） |
| 登记可处理的图层类型（栅格 RGB / 灰度 / 二值化灰阶） | **已实现**，通过自动化测试 |
| 在 CSP 中实际加载并显示菜单 | ⚠️ **待真机验证**（见下方「当前唯一的关键未知」） |

---

## 明确不做（官方 SDK 不支持，且不会用替代品冒充）

| 功能 | 为什么不做的 |
|---|---|
| **导航器实时投影** | SDK 无视图/导航器接口。**任何屏幕抓取实现都被本项目明令禁止**（旧版曾用抓屏实现，已整块移除） |
| **独立叠加剪切蒙版** | SDK 没有图层枚举/图层树/蒙版接口，只有取「当前图层原点」一个 proc |
| **常驻停靠控制面板** | SDK 只提供滤镜自己的模态对话框，没有面板宿主接口 |
| **插件注册全局快捷键** | SDK 无此接口；快捷键只能由 CSP 的「快捷键设置」分配 |
| **全部图层一键灰度** | 滤镜一次只处理一个目标图层，SDK 无图层枚举 |

依据是真实 SDK 头文件 `TriglavPlugInRecord.h` 里的 `TriglavPlugInRecordSuite` —— 全部可用能力只有
moduleInitialize / filterInitialize / filterRun 三组。详见 [已知限制与未完成事项](docs/已知限制与未完成事项.md)。

---

## 当前唯一的关键未知

**CSP 5.0.4 到底从哪个目录、用哪种扩展名加载第三方滤镜插件 —— 仍未打通。**

经过**两次真实 CSP 启动**的实测，两个最可能的候选位置**都未被加载**：

| 位置 | CSP 启动 | 探针日志 | 判定 |
|---|---|---|---|
| `%APPDATA%\...\CELSYS\CLIPStudioPlugIn\PAINT` | 13:11:49 | 无 | **未被加载** |
| CSP 程序目录 `PlugIn\PAINT`（自带 40 个滤镜同处） | 13:15:05 | 无 | **未被加载** |

判定依据：探针在 `DllMain` 的 `DLL_PROCESS_ATTACH` 就**无条件**写日志——
**DLL 被映射进进程就必然产生日志**，不需要宿主调用入口点。没有日志 = CSP 没打开该文件。

**这意味着光靠「把文件放进目录」无法让 CSP 5.0.4 加载第三方滤镜插件。**
这同时解释了旧版 v0.1.6 的失败：它的安装位置同属「用户数据目录」一类，
「装了没用」不是代码问题，而是安装方式在该 CSP 上不成立。

**完整证据、确定性范围、与官方资料的张力、以及下一步路径**：
见 [真机取证结论](docs/真机取证结论.md) 与 [安装位置验证](docs/安装位置验证.md)。

### 待判定：三种扩展名

探针已以 `.dll` / `.cfpi` / `.cpm` 三种扩展名同时放在 CSP 程序目录，
一次**完全退出后重启**即可判定扩展名是否影响加载。这一步尚未执行。

---

## 快速开始

```powershell
# 1) 把官方 SDK 头文件放到 sdk_official/（不随仓库分发，见构建说明）
#    https://www.clipstudio.net/ja/dl/cspsdk_term/

# 2) 构建（需要 MinGW-w64 g++）
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1

# 3) 测试（不需要打开 CSP）
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_tests.ps1

# 4) 安装（先 dry run 看路径，再加 -Execute 真写入）
powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_plugin.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_plugin.ps1 -Execute

# 5) 完全退出并重启 Clip Studio Paint，看「滤镜」菜单
```

完整说明见 [构建与安装说明](docs/构建与安装说明.md)，日常使用见 [用户手册](docs/用户手册.md)。

---

## 文档索引

| 文档 | 内容 |
|---|---|
| [构建与安装说明](docs/构建与安装说明.md) | 环境、取 SDK、构建、测试、安装、真机验证步骤、排错 |
| [用户手册](docs/用户手册.md) | 面板每个控件的作用、支持范围、数据安全说明、快捷键绑定、卸载 |
| [测试报告](docs/测试报告.md) | 已通过项的原始输出 + 待真机验证清单 |
| [已知限制与未完成事项](docs/已知限制与未完成事项.md) | 不可行功能的依据、待验证项、与旧版的逐项对照 |
| [阶段1 可行性报告](docs/阶段1-可行性报告.md) | 四项需求的技术可行性判定与证据 |
| [阶段1 证据记录](docs/阶段1-证据记录.md) | 所有命令、输出、官方原文摘录（可复现） |
| [AGENTS.md](AGENTS.md) | 本项目的 Agent 治理入口（规则主源在 `ai-toolkit/`） |

---

## 目录结构

```
REDcoLAB/
├─ AGENTS.md                     Agent 入口
├─ ai-toolkit/                   规则主源（AGENTS.md / VERSION / CHANGELOG / skills）
├─ .agents/tools/                Skill 路径解析器 + 框架校验器
├─ src/
│  ├─ REDcoLAB.cpp               插件入口与滤镜执行
│  ├─ grayscale.h                灰度算法（纯函数）
│  └─ redcolab_log.h             可选运行日志
├─ tools/
│  ├─ build.ps1                  构建（零警告，产出 .cpm + .dll）
│  ├─ run_tests.ps1              测试入口
│  ├─ mockhost.py                模拟 CSP 宿主（客观像素断言）
│  └─ install_plugin.ps1         安装到候选目录（默认 dry run）
├─ docs/                         手册与报告
├─ sdk_official/                 CELSYS SDK 头文件（gitignore，绝不外发）
└─ dist/                         构建产物（gitignore）
```

---

## 许可与分发

- 本仓库**不含** CELSYS 的 SDK 头文件或样例源码（`.gitignore` 已排除，归档脚本会扫描拦截）。
  这些文件的著作权属 CELSYS，按 SDK 使用许可条款不得再分发。
- 依据 CELSYS SDK 使用许可契约书，把插件**提供给别人**需要按其指定流程提交并经其确认。
  自己使用不受此限。
- 本仓库只包含 REDcoLAB 自身编写的代码。

---

## 运行日志

默认不写日志。排查问题：

```powershell
$env:REDCOLAB_LOG = "1"   # 重启 CSP 后生效
```

日志写到 `%TEMP%\REDcoLAB.log`。

---

## 命名

- 中文正式名：**红红子LAB**
- 英文正式名：**REDcoLAB**
- 目录、源码、构建配置、日志统一使用 `REDcoLAB`（严格保留大小写）
