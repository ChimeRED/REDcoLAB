# REDcoLAB 变更记录

## 1.0.0 修正 6：通道排列覆盖 —— 修掉一个未显式处理的分支（2026-10-16）

### 发现的缺陷

真实 CSP 可能以多种**通道排列**把图层交给滤镜。此前只有 `RGBAlpha` 路径被真正执行过，
其余分支从未被测——而**这类分支写错不会崩溃，只会静默写坏用户的图层**，比崩溃更危险。

新增 `tools/mockhost.py --channel-order` 后逐一测试，发现：

| 通道排列 | 修正前 | 问题 |
|---|---|---|
| `0x05` `BinarizationAlpha`（二值化单通道图层） | 落进默认分支 | **未显式识别为「无需转换」**，只是因未注册 `getRGBChannelIndexProc` 才侥幸没写像素 |

### 修复

`src/REDcoLAB.cpp` 的通道排列分派改为四个显式分支：

- `GrayAlpha` → 已是灰度，直接返回
- `BinarizationGrayAlpha` **与** `BinarizationAlpha` → 二值图层，直接返回
- `Alpha`（新增显式分支）→ 无颜色通道，直接返回
- 其余（CMYK / plane 等）→ 记日志后返回，不改动画布

### 新增测试

`tools/run_tests.ps1` 增加**通道排列覆盖**环节，逐一验证 6 种排列：

```
0x01 Alpha-only                     PASS   （必须不改动）
0x02 GrayAlpha                      PASS   （必须不改动）
0x03 RGBAlpha (normal conversion)   PASS   （必须真的转成灰度）
0x04 CMYKAlpha                      PASS   （必须不改动）
0x05 BinarizationAlpha              PASS   （必须不改动）
0x06 BinarizationGrayAlpha          PASS   （必须不改动）
```

`0x03` 单独断言**确实发生转换**，防止把「该转换的」误判成「无需处理」。

### 测试闭环现状

共 **16 项**，全部通过：产品四组合（cpm/dll × 正常/最小宿主）、算法矩阵、
探针两组合、**6 种通道排列**、残缺宿主两产物。

## 1.0.0 修正 5：真机加载成功 —— 撤回此前的错误否定结论（2026-10-16）

### 结果

**插件已被 CSP 5.0.4 成功加载并完成滤镜注册。**

```
[init] hostVersion=6 (apiResult=0) required=1
[init] module registered, version=1.0.0, moduleKind=0x4380
[filterInit] registered filter, targets=RGB/Gray/Binarization, version=1.0.0
```

- 安装位置：**CSP 程序目录 `...\CLIP STUDIO PAINT\PlugIn\PAINT\`**，扩展名 `.dll`
- 真实宿主版本：**6**；`moduleKind = Filter(0x4380)` **被接受**
- CSP 会**加载插件两次**（第一次仅模块初始化后终止，第二次才注册滤镜）
- 证据留存：`docs/evidence/realmachine-product-load.log`

### ⚠️ 撤回错误结论

前一轮我曾判定「两个候选位置均未被加载」「手动放文件在 5.0.4 上不成立」——
**这些结论是错的，现已撤回。**

根因：**CSP 进程继承了启动者的 `TEMP` 环境变量**。在 DSH 会话中启动 CSP 时 `TEMP` 指向
沙箱私有目录，探针日志被写进那里，而我一直检查真实用户 `%TEMP%`，因此看不到日志，
误判为「未被加载」。更早发现的那份 38019 字节日志里其实混有真机记录，
我按段统计时判断标准不够严而整体当成了模拟宿主输出。

**方法论教训已写入 `ai-toolkit/AGENTS.md`**：不得假定 `%TEMP%` 是宿主实际使用的目录；
判定加载必须扫描所有可能的 TEMP 位置；优先采用宿主无法伪造的判据（如 `hostVersion=6`）。

### 变更

- `ai-toolkit/AGENTS.md`：新增 §2.0「已确证的安装位置」与取证教训；产物格式明确为 `.dll`
- `tools/install_plugin.ps1`：候选顺序调整，**程序目录升为第 1 位并标注已确证**，
  其余位置标注「未确认」并说明早前测试无效
- `docs/安装位置验证.md`：改写为正向最终结论
- `docs/真机取证结论.md`：加入更正章节，明示撤回
- `README.md`：安装位置更新为已确证结论
- 新增 `src/probe/REDcoLAB_kindprobe.cpp` + `tools/build.ps1 -KindProbe`：
  运行时按环境变量 `REDCOLAB_KIND` 选择声明的模块种类，用于确证 `0x4380` 被接受

### 仍需人工确认

菜单文案显示、参数面板与实时预览、执行后画面变灰、Ctrl+Z 撤销、
以及矢量/文字/文件夹图层不出现在可执行范围内。这些需要打开 CSP 界面操作。

## 1.0.0 修正 4：真机取证结论定稿（2026-10-16）

两次真实 CSP 启动的取证结果：**两个最可能的候选位置均未被加载**。

### 已确证

| 位置 | CSP 启动证据 | 探针日志 | 判定 |
|---|---|---|---|
| `%APPDATA%\CELSYSUserData\CELSYS\CLIPStudioPlugIn\PAINT` | 13:11:49 启动 | 真实 `%TEMP%` 中无 | **未被加载** |
| CSP 程序目录 `PlugIn\PAINT`（仅 `.dll`） | 13:15:05–13:15:50 运行 | 真实 `%TEMP%` 中无 | **未被加载** |

判定依据：探针在 `DllMain` 的 `DLL_PROCESS_ATTACH` 就无条件写日志，DLL 被映射即必然产生日志，
无需宿主调用入口点。无日志 = CSP 未打开该文件。

### 明确的不确定项

- `.cfpi` / `.cpm` 扩展名是否有效：**未测试**（变体放入时间晚于最后一次 CSP 启动）
- 第 2 次是否为「完全退出后重启」：**无法从文件时间戳确认**

### 新增

- `docs/真机取证结论.md`：**正式结论文档**。含两次测试的完整证据、确定性范围、
  旧版失败的解释、与官方历史资料的张力、官方插件安装方式的二进制证据、
  明确标注的推测、各功能最终状态、以及四条按代价排序的推进路径。
- README 更新为当前真实状态（不再宣称「位置 [1] 是推荐安装点」）。

### 结论

**代码侧全部完成并通过验证；「插件被 CSP 加载」这一步未打通，因此「一键灰度」
处于「已实现但未完成真实软件验证」状态。** 障碍不在代码，在安装机制。

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
