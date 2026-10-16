---
name: release-archive
description: 归档、发布与版本管理 REDcoLAB 时使用，包括 tools/archive.ps1、版本号升级、CHANGELOG 维护、归档打包、commit / tag / push、GitHub Release 发布、_baseline/ 基线留存。关键词：归档、发布、Release、tag、版本号、CHANGELOG、push、自动上传、替换旧版本、基线。不适用于：日常改代码（用 plugin-dev）、构建与验证（用 build-verify）、只改文档。
---

# 归档与发布（release-archive）

## 触发条件 <!-- Trigger conditions -->

- 用户要求「归档」「发布」「上传 GitHub」「替换旧版本」「发 Release」。
- 需要升版本号、写 CHANGELOG、打 tag。
- 需要在 `_baseline/` 留存可恢复基线。
- 需要生成归档压缩包。

## 不适用与切换条件 <!-- Not applicable -->

- 还没通过 `build-verify` 的构建与测试：先回到 `build-verify`，不得跳过。
- 只改代码不发布：用 `plugin-dev` / `build-verify`。
- 用户只说「备份一下」但没提发布：只做 `_baseline/` 留存，不做推送。

## 核心不变量 <!-- Core invariants -->

1. **推送与 Release 必须先获用户确认**：会替换远端现有版本，属破坏性操作。
2. **归档包绝不含 SDK 头文件**：打包前必须扫描包内是否出现 `TriglavPlugIn*.h`，出现即中止。
3. **不得强推**（`--force` / `--force-with-lease`），不得删除远端分支或 tag。
4. **不得覆盖唯一的基线备份**：`_baseline/` 下每次留存新目录，不删除旧的。
5. **版本号单一来源**：以 `ai-toolkit/VERSION` 为准，同步 `src/` 内版本常量与 `CHANGELOG.md`。
6. **不写入凭据**：仓库、日志、文档中不得出现 token、密码、账号、邮箱。
7. **推送前必须 `git status` 干净**，且确认暂存区里没有 `sdk_official/`、`reference/`、`_baseline/`、`dist/`。

## 标准工作流 <!-- Standard workflow -->

1. 前置检查：`build-verify` 已 PASS；`ai-toolkit/VERSION` 与源码版本常量一致。
2. 留存基线：把当前已发布状态复制到 `_baseline/<版本>-<日期>/`，写 `BASELINE-README.md`（来源、HEAD、还原方式）。
3. 升版本：更新 `VERSION`、`src/` 版本常量、`CHANGELOG.md`（中文说明改了什么与为什么）。
4. 构建与测试：重新跑 `build-verify` 的闭环，确认产物是最新源码编译的。
5. 打包：生成 `dist/REDcoLAB-<版本>.zip`，含 `REDcoLAB.cpm`、`REDcoLAB.dll`、`README.md`、`CHANGELOG.md`；**然后扫描包内容**。
6. 提交：`git add` 明确列出要提交的路径（不要 `git add -A`）；commit message 用中文。
7. 打 tag：`v<版本>`。
8. **询问用户确认**后再 `push` 与创建 Release。
9. 记录归档结果（commit、tag、Release URL、包内文件清单）到 `docs/测试报告.md` 或 `CHANGELOG.md`。

## 按需读取 <!-- Read on demand -->

| 任务 | 文件 |
|---|---|
| 归档脚本细节 | `tools/archive.ps1` |
| 版本单一来源 | `ai-toolkit/VERSION` |
| 变更记录格式 | `ai-toolkit/CHANGELOG.md` |
| 基线留存格式 | `_baseline/legacy-v0.1.6/BASELINE-README.md` |

## 验证 <!-- Verification -->

- 运行：`tools\archive.ps1 -DryRun`（如支持）——应打印将执行的动作，不实际推送。
- 检查：归档包内 `Select-String -Pattern 'TriglavPlugIn'` 无命中。
- 检查：`git status --short` 为空；`git tag -l` 含新 tag；`git log -1` 是本次提交。
- 检查：远端 tag 与本地一致（仅在用户确认推送后）。

## 错误处理 <!-- Error handling -->

- 推送失败：本机首次连 github.com 常超时，可重试（脚本既有行为），但**不得**改成强推。
- 归档包混入 SDK 头文件：立即中止，删除该包，修 `.gitignore` 与打包脚本后重跑。
- 失败记录到 `.agents/skill-error-records/release-archive-<日期>-<短任务名>.md`。
