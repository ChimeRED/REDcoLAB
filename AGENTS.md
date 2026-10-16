# REDcoLAB 项目 AI 入口规则

读完本文件后，必须继续读取 `ai-toolkit/AGENTS.md`，再根据任务读取 `ai-toolkit/skills/index.md`。

## 本项目是什么

- 项目目录：`REDcoLAB/` —— **独立项目**，位于工作区根 `default-workspace/` 之下。
- 项目名：中文「红红子LAB」，英文「REDcoLAB」（**大小写严格保留**）。
- 目标环境：Windows 版 **CLIP STUDIO PAINT Ver. 5.0**（本机实测 5.0.4）**EX**。
- 技术形态：CELSYS 官方 **Triglav Plug-In SDK** 的 **滤镜插件**（C++ / MinGW-w64），产物为 `.cpm` / `.dll`。

## 最小入口规则

- 与用户沟通默认使用中文；代码标识符、命令、路径保持原文。
- Agent 规则与 Skills 主维护源位于 `ai-toolkit/`，它是**唯一主源**。
- 项目总规范以 `ai-toolkit/AGENTS.md` 为准。
- 处理具体任务前先读 `ai-toolkit/skills/index.md`，禁止预先完整读取所有 Skill。
- 加载 Skill 前必须执行 `.agents/tools/resolve-skill-path.ps1 -Skill <name>`，并使用它输出的真实路径。
- 解析失败（`missing` / `conflict`）时记录到 `.agents/skill-error-records/`，不得猜测旧路径继续加载。
- `.agents/skills/` 仅作兼容或外部 Skill 目录，当前为空。

## 最高优先级项目规则

- **`sdk_official/` 与 `reference/` 内是 CELSYS 的东西**：前者著作权属 CELSYS，**绝不允许提交、打包、分发**；两者均已 gitignore。
- **禁止任何形式的屏幕抓取**（截屏 / 桌面捕获 / 窗口捕获 / 录屏 / 定时截屏轮询），不得用于任何功能。
- **不允许空壳功能**：没有真实作用的控件、按钮、开关一律不得出现在界面里。
- **不允许把推测写成已验证**：未在真实 CSP 中测过的，必须标注「待真机验证」。
- **`_baseline/` 是旧版只读存档**：只用于追溯对照，不得从中恢复被否决的功能（见 `_baseline/legacy-v0.1.6/BASELINE-README.md`）。
- 声明「已验证 / 已修复」之前必须有真实命令输出；跑不起来的检查如实标注未验证。

## 安全规则

- 不覆盖用户已有改动；不修改 `.git/` 内部文件；不修改生成文件或第三方目录。
- 破坏性操作（删除、重置、强推、发布、覆盖已发布版本）必须先获得用户确认。
- 不下载未知脚本直接执行；官方 SDK 只能从 CELSYS 官方页面获取。
- 修改后执行 `ai-toolkit/AGENTS.md` 中对应任务的最小验证闭环。
- 工具或关键步骤失败时，按 `ai-toolkit/templates/skill-error-record.md` 留档。
