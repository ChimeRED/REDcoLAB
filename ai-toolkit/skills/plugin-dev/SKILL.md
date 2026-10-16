---
name: plugin-dev
description: 修改 REDcoLAB CSP 滤镜插件本体时使用，包括 src/ 下的 TriglavPluginCall 入口、四个 selector 分支、滤镜属性项与参数面板、目标图层类型 targetKinds、灰度算法、选区与 Alpha 处理、预览开关、运行日志。关键词：滤镜插件、Triglav、模块初始化、滤镜初始化、滤镜执行、属性项、targetKinds、灰度、alpha、预览。不适用于：只改构建/安装/测试脚本（用 build-verify）、只做归档发布（用 release-archive）、只改文档。
---

# 插件本体开发（plugin-dev）

## 触发条件 <!-- Trigger conditions -->

- 修改 `src/REDcoLAB.cpp`、`src/grayscale.h`、`src/redcolab_log.h` 中的任意内容。
- 新增/修改/删除滤镜参数项（枚举、整数、布尔），或调整参数面板的默认值、取值范围、记忆行为。
- 修改目标图层类型（`targetKinds`）或预览开关（`canPreview`）。
- 修改灰度算法、选区蒙版处理、Alpha 处理、强度混合、反相逻辑。
- 排查滤镜执行阶段的崩溃、无效果、像素写错。

## 不适用与切换条件 <!-- Not applicable -->

- 只改 `tools/*.ps1`、`tools/mockhost.py`、`tools/*.py`：切换到 `build-verify`。
- 只做版本号、CHANGELOG、归档、推送：切换到 `release-archive`。
- 只改 `docs/`、`README.md`：无需 Skill。
- 任务涉及「导航器」「图层枚举」「剪切蒙版」「常驻面板」「插件注册快捷键」：**立即停止**——这些在 SDK 中不存在，见 `ai-toolkit/AGENTS.md` §2。

## 核心不变量 <!-- Core invariants -->

1. **只实现 SDK 真实提供的能力**。任何改动前先核对 `sdk_official/TriglavPlugInRecord.h` 的 `TriglavPlugInRecordSuite`；不得凭空调用不存在的 proc。
2. **导出符号只有一个**：`extern "C" __declspec(dllexport) void TriglavPluginCall(...)`。不得新增其它导出。
3. **不改变 Alpha**：除非用户显式要求且参数项明确标注，灰度处理只能改 RGB 通道。
4. **不得越界写像素**：写回时严格用 `blockRect` 作为循环边界（宿主可能改写传入的 rect，必须另存副本）。
5. **透明像素跳过**（与官方样例一致）：`alpha == 0` 的像素不处理。
6. **UI 文字用 UTF-16 字面量**（`u"..."`），禁用中点号 `·`（U+00B7）。
7. **不新增空壳**：不加没有真实作用的参数项或开关。
8. **版本常量与 `ai-toolkit/VERSION` 保持一致**。

## 标准工作流 <!-- Standard workflow -->

1. 读 `ai-toolkit/AGENTS.md` §2 与 `sdk_official/TriglavPlugInRecord.h`，确认要用的 proc 真实存在及其签名。
2. 读 `src/` 现有实现，确认改动点与影响面。
3. 实施最小变更；不重构无关代码。
4. 回归：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1`（必须零警告）。
5. 验证：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_tests.ps1`（模拟宿主 + 灰度断言）。
6. 如改动影响注册或参数面板，标注「需真机重启 CSP 验证」，并在 `docs/测试报告.md` 记录待验证项。

## 按需读取 <!-- Read on demand -->

| 任务 | 文件 |
|---|---|
| 能力边界与 proc 清单 | `sdk_official/TriglavPlugInRecord.h` |
| 常量值与枚举（selector、channelOrder、property 类型） | `sdk_official/TriglavPlugInDefine.h` |
| 服务函数签名 | `sdk_official/TriglavPlugInService.h` |
| 官方滤镜样例（由浅入深） | `reference/official-samples/`（gitignore） |

## 验证 <!-- Verification -->

- 运行：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_tests.ps1`
- 期望：模拟宿主全链条 OK；`grayscale check: OK (all R==G==B)`；`RESULT: PASS`；退出码 0。
- 期望：`tools\build.ps1` 输出无 warning。
- 真机项：滤镜菜单出现「红红子LAB → 一键灰度」、参数面板可调、Ctrl+Z 可撤销。

## 错误处理 <!-- Error handling -->

- 构建失败或测试失败：记录到 `.agents/skill-error-records/plugin-dev-<日期>-<短任务名>.md`。
- 不得为了「跑通」而放宽断言、删除检查或注释掉验证步骤。
- 不得在未通过模拟宿主验证的情况下声称插件可用。
