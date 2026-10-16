---
name: build-verify
description: 构建、安装与验证 REDcoLAB 滤镜插件时使用，包括 tools/build.ps1、tools/install_plugin.ps1、tools/run_tests.ps1、tools/mockhost.py；编译器与 CELSYS SDK 头文件就位检查；.cpm 与 .dll 双格式产出；导出符号检查；模拟宿主全链条测试；灰度客观断言（R==G==B、Alpha 不变）；真机加载取证与插件目录定位。关键词：构建、编译、g++、MinGW、SDK 头文件、.cpm、.dll、导出符号、mockhost、模拟宿主、灰度校验、插件目录、CSP 看不到菜单。不适用于：修改 src/ 下的插件逻辑（用 plugin-dev）、归档发布与推送（用 release-archive）。
---

# 构建、安装与验证（build-verify）

## 触发条件 <!-- Trigger conditions -->

- 运行或修改 `tools/build.ps1`、`tools/install_plugin.ps1`、`tools/run_tests.ps1`、`tools/mockhost.py`、`tools/verify_grayscale.py`。
- 需要确认 SDK 头文件是否就位、编译器是否可用。
- 需要构建 `.cpm` / `.dll` 双格式产物，或检查导出符号。
- 需要做灰度结果的**客观**校验（而不是肉眼看）。
- 排查「CSP 里看不到红红子LAB 滤镜菜单」「装了插件没反应」。
- 需要定位 CSP 实际的插件目录。

## 不适用与切换条件 <!-- Not applicable -->

- 修改 `src/` 实现的逻辑：切换到 `plugin-dev`，改完再回到本 Skill 验证。
- 版本号、CHANGELOG、归档、推送：切换到 `release-archive`。
- 需要绕过编译器错误而改源码：这是 `plugin-dev` 的工作，且不得降低验证强度。

## 核心不变量 <!-- Core invariants -->

1. **验证必须客观**：灰度用像素断言（`R == G == B` 且 Alpha 不变），不接受「看起来变灰了」。
2. **警告即问题**：`-Wall -Wextra` 有输出就必须处理或说明原因。
3. **SDK 头文件不得提交/打包**：`sdk_official/` 与 `reference/` 永不进入 git、zip、Release 附件。
4. **不得从第三方镜像获取 CELSYS 头文件**：只能由用户从 CELSYS 官方页面取得。
5. **真机项与模拟项分开标注**：模拟宿主 PASS ≠ 真机可用；真机未测的一律写「待真机验证」。
6. **安装脚本必须先打印目标绝对路径**，再执行写入；写入范围仅限 CSP 插件目录。
7. **模拟宿主是测试替身，不是 CSP**：不得把它的输出当作 CSP 载入成功的证据。

## 标准工作流 <!-- Standard workflow -->

1. 环境自检：编译器、`sdk_official/*.h` 是否齐全，缺哪一项就明确报告缺哪一项。
2. 构建：`tools\build.ps1` —— 产出 `dist/REDcoLAB.cpm` 与 `dist/REDcoLAB.dll`。
3. 符号检查：确认产物导出 `TriglavPluginCall`。
4. 模拟宿主：`tools\run_tests.ps1` —— 跑完整调用链并断言灰度结果。
5. 安装（需用户确认）：`tools\install_plugin.ps1` —— 打印目标路径后复制到候选插件目录。
6. 真机取证：请用户重启 CSP，检查滤镜菜单；把结论写进 `docs/测试报告.md`。
7. 记录：成功与失败都要留下命令与输出摘要。

## 按需读取 <!-- Read on demand -->

| 任务 | 文件 |
|---|---|
| 构建脚本细节 | `tools/build.ps1` |
| 模拟宿主协议 | `tools/mockhost.py` |
| 客观校验断言 | `tools/run_tests.ps1`、`tools/verify_grayscale.py` |
| 插件目录候选与依据 | `docs/已知限制与未完成事项.md` |

## 验证 <!-- Verification -->

- 运行：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_tests.ps1`
- 期望：`RESULT: PASS`，退出码 0；产物存在且导出符号正确。
- 运行：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1`
- 期望：无 warning，退出码 0。
- 真机：CSP 重启后 `滤镜` 菜单出现「红红子LAB → 一键灰度」，且执行后 Ctrl+Z 可撤销。

## 错误处理 <!-- Error handling -->

- 编译失败 / 断言失败 / 真机不生效：记录到 `.agents/skill-error-records/build-verify-<日期>-<短任务名>.md`，必须含命令、退出码、关键报错。
- 连续 3 次同类失败：停下来查根因（例如插件目录或扩展名判断错误），不要反复重试同一动作。
- 不得删除或弱化断言来让测试变绿。
