# REDcoLAB 项目 Agent 总规则

> 本文件是 REDcoLAB 项目的**唯一主维护源**总规则。
> 框架版本见 `VERSION`，变更见 `CHANGELOG.md`。
> 作用范围：`REDcoLAB/` 整个项目目录。
> 上级规则：工作区根 `../AGENTS.md` 与 `../ai-toolkit/AGENTS.md`（本文件在其之下，不得冲突）。

## 1. 项目概述

- 项目名称：红红子LAB / **REDcoLAB**（大小写严格保留）。
- 项目类型：Windows 桌面绘画软件 **CLIP STUDIO PAINT (CSP) 5.0 EX** 的原生滤镜插件。
- 主要语言：**C++17**（插件本体）+ **PowerShell 5.1**（构建/安装/验证脚本）+ **Python 3**（测试与客观校验）。
- 主要框架：**CELSYS Triglav Plug-In SDK**（官方滤镜插件 SDK）。
- 产物格式：**`.dll`**（已确证为 CSP 实际加载的格式）。`.cpm` 为社区惯例，**未被确认可用**，仅作附带产出。
- 构建入口：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1`
- 安装入口：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_plugin.ps1`
- 测试入口：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_tests.ps1`
- 归档入口：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\archive.ps1`（**推送前必须用户确认**）

## 2. 能力边界（本项目的根本约束，任何改动不得违反）

### 2.0 已确证的安装位置（真机验证通过，2026-10-16）

**插件安装到 CSP 程序目录，扩展名用 `.dll`：**

```
C:\build\csp\CLIP STUDIO 1.5\CLIP STUDIO PAINT\PlugIn\PAINT\
```

即 **Celsys 自带 40 个滤镜 DLL 所在的同一目录**。

真机证据（`docs/evidence/realmachine-product-load.log`）：

```
[init] hostVersion=6 (apiResult=0) required=1
[init] module registered, version=1.0.0, moduleKind=0x4380
[filterInit] registered filter, targets=RGB/Gray/Binarization, version=1.0.0
```

- 真实 CSP 5.0.4 的**宿主版本是 6**（模拟宿主固定为 1，可据此区分真假）
- `moduleKind = kTriglavPlugInModuleKindFilter (0x4380)` **被接受**
- CSP 会**加载插件两次**：第一次只做模块初始化后立即终止，第二次才注册滤镜
- **用户数据目录（`%APPDATA%\CELSYSUserData\...`）不是加载点**，不再作为候选

**取证教训（重要）**：CSP 进程会继承启动者的 `TEMP` 环境变量。
在 DSH 会话中启动 CSP 时，`TEMP` 指向沙箱私有目录，日志会写进那里而不是真实用户 `%TEMP%`。
**判断「是否加载」必须扫描所有可能的 TEMP 位置**，否则会得出错误的否定结论（本项目曾因此误判三轮）。

### 2.1 能力边界

官方 SDK **只允许开发滤镜插件**。经真实 SDK 头文件核实（`sdk_official/TriglavPlugInRecord.h` 的 `TriglavPlugInRecordSuite`），插件可用的全部能力仅为：

| 可用 | 说明 |
|---|---|
| 模块初始化 | 上报 moduleID、moduleKind = Filter(0x4380) |
| 滤镜初始化 | 上报分类名、滤镜名、可否实时预览、目标图层类型、属性项 |
| 滤镜执行 | 取源/目标离屏位图、选区矩形、选区蒙版、图层原点、主/辅/描画色；写回像素并可上报进度 |

**明确不存在**（因此对应功能一律不得声称实现）：

- 图层枚举 / 图层树 / 图层增删改
- 原生剪切蒙版（「用下一个图层剪切到图层」）的读取或设置
- 导航器扩展、视图缩放/平移状态、画布变化通知
- 常驻面板 / 停靠面板
- 插件注册全局快捷键

### 2.2 被否决的功能（不得复活）

| 功能 | 状态 | 依据 |
|---|---|---|
| 导航器实时投影 | **不可行，已移除** | SDK 无视图/导航器接口；**任何屏幕抓取实现均被明令禁止** |
| 独立叠加剪切蒙版 | **不可行，未开发** | SDK 无图层枚举与蒙版接口 |
| 常驻统一控制面板 | **不可行** | SDK 只提供滤镜自己的模态对话框 |
| 插件注册快捷键 | **不可行** | 快捷键只能由 CSP 自身的快捷键设置分配 |

## 3. 项目结构

```text
REDcoLAB/                             独立项目（不在工作区框架仓库的跟踪范围内）
├─ AGENTS.md                          根入口（导航 + 最高优先级规则）
├─ ai-toolkit/                        ★ Agent 规则唯一主维护源
│  ├─ AGENTS.md  VERSION  CHANGELOG.md
│  ├─ skills/{index.md, plugin-dev, build-verify, release-archive}
│  └─ templates/skill-error-record.md
├─ .agents/
│  ├─ tools/{resolve-skill-path.ps1, validate-agent-framework.ps1}
│  ├─ skills/                         兼容目录（空）
│  └─ skill-error-records/            错误留档首选目录
├─ docs/
│  ├─ 阶段1-可行性报告.md
│  ├─ 阶段1-证据记录.md
│  ├─ 构建与安装说明.md
│  ├─ 用户手册.md
│  ├─ 测试报告.md
│  └─ 已知限制与未完成事项.md
├─ src/                               插件源码（唯一实现来源）
│  ├─ REDcoLAB.cpp                    插件入口与滤镜执行
│  ├─ grayscale.h                     灰度算法（纯函数，可独立测试）
│  └─ redcolab_log.h                  可选运行日志
├─ sdk_official/                      ★ CELSYS SDK 头文件：gitignore、绝不提交/分发
├─ reference/official-samples/        ★ SDK 样例源码参考：gitignore、绝不提交/分发
├─ tools/                             构建 / 安装 / 测试 / 归档脚本
├─ dist/                              构建产物（gitignore）
└─ _baseline/legacy-v0.1.6/           旧版只读存档（gitignore，见其 BASELINE-README.md）
```

## 4. 沟通规则

- 与用户沟通默认使用中文；代码标识符、命令、路径保持原文。
- 报告结论必须附真实命令与输出摘要。
- **明确区分四种状态**：已实现且通过测试 / 已实现但待真机验证 / 仅完成部分 / 当前技术路线不可行。
- 需求含糊时先问，不猜测用户意图后大改代码。

## 5. Skill 加载规则

- 先读 `skills/index.md`，它是任务路由表，不是规则全集。
- 每个任务默认选择 **1 个主 Skill**；只有跨域任务才叠加。
- Skill 必须通过解析器取真实路径：
  `powershell -NoProfile -ExecutionPolicy Bypass -File .agents\tools\resolve-skill-path.ps1 -Skill <name>`
- 解析器返回 `missing` / `conflict` 时：停止加载，写错误记录，不得猜测旧路径。

## 6. 修改与验证规则（修改即检）

| 改了什么 | 必须执行 |
|---|---|
| `src/*.cpp` / `src/*.h` | `tools\build.ps1` → `tools\run_tests.ps1`（模拟宿主 + 灰度客观校验） |
| 滤镜参数项 / 属性面板 / 注册逻辑 | 上一步 + 真机测试（重启 CSP 确认菜单与面板） |
| `tools\*.ps1` | PowerShell 语法检查 + 实际跑一次（检查退出码） |
| 版本号 / 清单 / 文档 | `tools\check_version.ps1`（如存在）+ 文档一致性检查 |
| Agent 框架文件 | `.agents\tools\validate-agent-framework.ps1` |

出现错误时继续修复，或明确记录阻塞原因，**不要静默通过**。

## 7. 生成文件规则

- `dist/*` 是生成产物，不入库；改源后重新构建。
- **不得手工编辑**生成物；版本号只在 `ai-toolkit/VERSION` 与 `src/` 里的版本常量处维护。
- `sdk_official/`、`reference/`、`_baseline/`、`dist/` 一律不入库。

## 8. 配置与编码规则（硬约束）

- C++ 源文件用 **无 BOM 的 UTF-8**；UI 文字用 `u"..."`（UTF-16LE）字面量传给 SDK。
- **PowerShell 5.1 读 UTF-8 文件会按系统 ANSI 代码页解码**（实测会把中文变成乱码，且乱码字节可能含引号，直接破坏正则解析）。因此：
  - `.ps1` 脚本本体**必须纯 ASCII**（否则脚本自己被读坏）；
  - 脚本读 `.md` / `.json` / 源码等 UTF-8 文件时，**必须显式指定编码**：`[System.IO.File]::ReadAllText($p, (New-Object System.Text.UTF8Encoding($false)))` 或 `Get-Content -Encoding UTF8`。
  - 这条已由 `.agents\tools\validate-agent-framework.ps1` 的 `skill-frontmatter` 检查看守。
- **UI 可见文本禁用中点号 `·`（U+00B7）**，用普通空格或 `-` 代替。
- PowerShell 脚本：兼容 **5.1**；不能把 `if` 当表达式赋值；含引号的多行文本用字面 here-string（`@'...'@` 结束标记顶格）。
- 脚本退出码必须检查（`$LASTEXITCODE` / `$?`），非 0 必须中止并报错。
- 编译必须**零警告目标**：`-Wall -Wextra`；确需忽略的参数用 `-Wno-unused-parameter` 并注明原因。
- 版本差异、路径差异只允许写在 `tools/` 的公共脚本里，不得散落在多个脚本中。

## 9. 安全与权限边界

- `sdk_official/` 内容**不得**出现在任何提交、压缩包、Release 附件、日志中。
- 不得把 CELSYS 头文件从任何第三方镜像下载或再分发；只能由用户从 CELSYS 官方页面获取。
- 不得执行 `git push` / `git reset --hard` / 强推 / 删除远端分支，除非用户明确要求并确认。
- `tools/install_plugin.ps1` 会写入**工作区之外**的 CSP 插件目录；执行前必须打印目标绝对路径。
- 不在任何文件、日志、错误记录中保存凭据、token、账号、邮箱。

## 10. Git 协作规则

- 本项目的远端：`https://github.com/ChimeRED/REDcoLAB`（分支 `main`）。
- 归档 = `tools/archive.ps1`：检查 → 构建 → 测试 → 归档打包 → commit → tag → push → Release。
- **推送与 Release 属破坏性操作**（会替换现有版本），必须先获得用户确认。
- 每次重大修改前，先在 `_baseline/` 下留存可恢复基线，**不得覆盖唯一的备份**。
- commit message 用中文说明「改了什么 + 为什么」。

## 11. 错误留档规则

- 首选目录：`.agents/skill-error-records/`。
- 备用目录（按序尝试，不得临时猜测落点）：1) `ai-toolkit/error-records/`；2) `.agent-framework/error-records/`；3) `error-records/`。
- 命名：`<skill-name>-<YYYY-MM-DD>-<short-task-name>.md`。
- 内容必须含：触发技能、时间、阶段、现象（命令 + 退出码 + 关键报错）、已尝试、根因判断（推测必须标注）、后续建议、解决信息。
- 模板：`templates/skill-error-record.md`。
- 同一 Skill 3 次以上失败或同类错误连续出现时，停下来修根因，不要只重复记录。

## 12. Skill 维护规则

- Skill 划分依据**稳定任务边界**；当前只保留 3 个，按真实失败再扩。
- 新增 / 删除 / 改名 Skill 时必须同步 `skills/index.md`，并升级 `VERSION` + 写 `CHANGELOG.md`。
- 每个 `SKILL.md` 必须含 7 个小节：触发条件、不适用与切换条件、核心不变量、标准工作流、按需读取、验证、错误处理。
- 改完运行 `.agents\tools\validate-agent-framework.ps1`，退出码 0 或仅剩可解释的 2（警告）。

## 13. 多 Agent 与多客户端适配

- 多 Agent 分工：**未启用**（单人维护 + 单插件项目，拆分收益低于协调成本）。
- 多客户端适配（`.claude/`、`.cursor/`、`.codex/` 等）：**未启用**。
- 启用方式与触发条件参照 `../docs/Agent框架使用说明.md`。
