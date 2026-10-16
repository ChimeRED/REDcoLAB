# REDcoLAB 变更记录

## 1.0.0 修正 3：残缺宿主契约测试（2026-10-15）

插件崩溃会连带**整个 CSP 会话**一起挂掉，所以「干净失败」是硬要求而不是加分项。
此前只验证了「**可选**回调缺失」，没有验证「**必需**回调缺失」。本轮补齐。

### 新增

- `tools/mockhost.py --hostile`：残缺宿主模式，移除插件视为**必需**的回调：
  `propertyService.createProc`、`addItemProc`、`setIntegerValueProc`、
  `filterInitializeRecord.setTargetKindsProc`、`moduleInitializeRecord.setModuleIDProc`。
- `tools/run_tests.ps1` 增加残缺宿主环节（每个产品产物各一次）。

### 测试结果

| 产物 | 残缺宿主下的行为 | 判定 |
|---|---|---|
| `REDcoLAB.cpm` | 模块初始化存活；**滤镜注册干净拒绝**（返回失败码 -1） | **PASS（无崩溃）** |
| `REDcoLAB.dll` | 同上 | **PASS（无崩溃）** |
| `REDcoLAB_probe.dll` | 不注册参数项，因此继续完成注册 | **PASS（无崩溃）** |

探针另增一条记录：`property service usable for the product build: yes/no` ——
真实 CSP 上若该服务不可用，产品版会拒绝注册，这条日志能直接说明原因。

至此测试闭环共 **10 项**，全部通过：产品四组合（cpm/dll × 正常/最小宿主）、
算法矩阵、探针两组合、残缺宿主两产物。

## 1.0.0 修正 2：真机验证探针（2026-10-15）

真机加载验证一直没做成，核心障碍是**无法区分两种日志**：
产品版只在 `REDCOLAB_LOG=1` 时写日志，而它的日志内容与我在模拟宿主上跑出来的**完全一样**。
也就是说，即使拿到日志也无法判断「CSP 加载了」还是「我自己测试写下的」。

### 新增

- `src/probe/REDcoLAB_probe.cpp`：**诊断探针插件**。与产品版**分开构建**，特性：
  - **无条件写日志**到 `%TEMP%\REDcoLAB_probe.log`，日志第二行是
    `*** CSP LOADED THE FILE ***` —— 模拟宿主无法伪造这条证据
  - `DllMain` 的 `DLL_PROCESS_ATTACH` 就写一行，**证明 DLL 被映射进进程**，
    即使宿主从未调用入口点
  - 逐个打印 `recordSuite` / `serviceSuite` 里每个函数指针是否存在（present / NULL），
    包括 `filterActivationInitializeRecord` / `filterActivationRunRecord` 的取值
  - **不修改任何像素**（它是量具，不是滤镜）
  - 故意不注册任何参数项：失败时不能被归咎于参数注册逻辑
- `tools/build.ps1 -Probe`：构建探针到 `dist\REDcoLAB_probe.dll`。
- `tools/install_plugin.ps1 -Probe`：安装探针而非产品。
- `tools/realmachine_test.ps1`：**一条命令走完真机验证准备**——
  检查前置条件 → 构建探针 → 本地先验证探针（含不修改像素断言）→ 清理旧日志 →
  安装到指定候选位置 → 打印你要做的两步与三种结果的处置方式。
- `tools/mockhost.py --expect-untouched`：断言「产物不得修改任何像素」。
  已反向验证：产品版在该模式下**正确 FAIL**，证明断言确实有效（不是空断言）。
- `tools/run_tests.ps1` 增加探针环节：正常契约 + 最小契约，均须通过。
- 重写 `docs/安装位置验证.md`：加入探针说明、日志样例、三种结果判读、候选位置排序、
  八条二进制证据、以及验证完成后的收尾清单。

### 测试结果

- 探针在模拟宿主上：生命周期全部 OK，`pixels untouched (as required)` → **PASS**；
  最小宿主契约下同样 PASS。
- 产品版在 `--expect-untouched` 下正确 FAIL（反向验证断言有效）。
- 全部既有测试保持通过（四组合 + 算法矩阵）。

### 仍未完成

**真机加载验证本身**。探针已就绪，需要一次真实的 CSP 重启执行。

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
