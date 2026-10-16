# REDcoLAB 变更记录

## 1.0.0 修正 1：宿主调用守卫补全 + 客观测试强化（2026-10-15）

### 修复

- **补全初始化阶段的宿主调用守卫**（真实缺陷，会导致 CSP 崩溃）。
  上一轮只守卫了滤镜执行阶段，`filterInitialize` 里仍有 8 处未判空的宿主函数指针调用：
  `addItemProc`、`setIntegerValueProc`、`setIntegerDefaultValueProc`、`setIntegerMinValueProc`、
  `setIntegerMaxValueProc`、`releaseProc`，以及全部 `TriglavPlugInFilterInitializeSet*` 宏。
  现已新增 `PropertyServiceUsable` 前置校验与一整层 `CallProperty*` / `CallSetFilter*` 守卫函数。
  见错误记录 `ERR-20261015-003`。
- `getHostVersionProc` 的返回值现在被检查：API 调用失败与版本过低都会明确拒绝加载，并写入日志。

### 新增

- `tools/algorithm_test.py`：**算法/强度矩阵测试**。用**独立重写的教科书公式**作为参照，
  逐像素比对插件输出，避免与 `src/grayscale.h` 同源而共享同一处错误。
  - 8 种算法 × 满强度，逐像素比对
  - 强度 0..100 步进 10，共 11 档，逐通道比对
  - 8 算法 × 3 强度 = 24 组 Alpha 不变性与透明像素安全性检查
- `tools/mockhost.py` 新增 `--minimal` 模式：剥离可选宿主回调，验证插件健壮性。
  已固化为 `tools/run_tests.ps1` 的常驻环节（每个产物跑 normal + minimal 两种契约）。
- `tools/install_plugin.ps1` 新增 `-Only <n>`：只安装到指定候选位置，便于把加载位置收敛到唯一答案。
- `docs/安装位置验证.md`：真机加载位置的验证方案、结果判读表与备选位置排序。

### 测试结果

- `build.ps1`：零警告通过。
- `run_tests.ps1`：`REDcoLAB.cpm` / `REDcoLAB.dll` × normal / minimal **四个组合全部 PASS**；
  算法矩阵测试 PASS。退出码 0。

## 1.0.0 框架建立（2026-10-15）

按《通用 Agent 治理框架搭建规范》v1.0 为 CSP 滤镜插件项目建立 Agent 治理框架。

### 新增

- 根入口 `AGENTS.md`：导航 + 最高优先级项目规则（禁止屏幕抓取、禁止空壳、禁止把推测当已验证、SDK 头文件不外发）。
- `ai-toolkit/AGENTS.md`：项目总规则 13 节，含**能力边界**与**被否决功能**清单。
- `ai-toolkit/VERSION`（1.0.0）与本变更记录。
- `ai-toolkit/skills/`：3 个初始 Skill
  - `plugin-dev`：插件本体开发（入口、selector、属性项、targetKinds、灰度、Alpha、选区）。
  - `build-verify`：构建、安装、模拟宿主测试、灰度客观断言、真机加载取证。
  - `release-archive`：版本、CHANGELOG、归档打包、commit/tag/push/Release、基线留存。
- `ai-toolkit/skills/index.md`：任务路由表 + 组合场景 + 手工路由测试集。
- `ai-toolkit/templates/skill-error-record.md`：错误留档模板。
- `.agents/tools/resolve-skill-path.ps1`：Skill 路径解析器（主源优先、冲突不静默、JSON 输出）。
- `.agents/tools/validate-agent-framework.ps1`：框架校验器（12 项检查，退出码 0/1/2）。
- `.agents/skill-error-records/`：错误留档首选目录。
- `_baseline/legacy-v0.1.6/`：旧版只读冻结存档（git bundle + 工作树快照 + 未提交改动 diff + 基线副本）。

### 相对旧版（v0.1.6）的设计取舍

| 旧版做法 | 新版决定 | 原因 |
|---|---|---|
| 导航器用屏幕抓取（10-60fps 抓 CSP 画布） | **整块移除** | 用户明令禁止任何屏幕抓取；且不能冒充导航器扩展 |
| 全局热键 + 常驻设置面板（Python 伴侣程序） | **整块移除** | SDK 无常驻面板接口；用户要求不得把普通窗口冒充原生面板 |
| `native/fetch_sdk.ps1` 从第三方 GitHub 镜像拉 CELSYS 头文件 | **移除**，改为只从 CELSYS 官方页面获取 | 第三方镜像再分发 CELSYS 头文件的合规性存疑；官方契约书对本 SDK 有明确限制 |
| 产物仅 `.cpm`，安装到 `CLIPStudioModule\PlugIn\PAINT\win` | **同时产出 `.cpm` 与 `.dll`**，安装脚本覆盖全部候选目录 | 实证发现：CSP 主程序内 `.cpm` 字面量出现 0 次，而 Celsys 自带滤镜全部是 `PlugIn\PAINT\*.dll`；旧版从未在真实 CSP 中验证过加载 |
| 未验证即宣称「已装进 CSP」 | 严格区分「模拟宿主 PASS」与「真机验证」 | 旧版 README 与推送消息声明了未验证的结论 |
