# Skill 错误记录

- 记录 ID：`ERR-20261015-003`
- 触发技能：`plugin-dev`
- 发生时间：2026-10-15 23:50 +08:00
- 发生阶段：测试（最小宿主契约测试）
- 严重程度：高（会导致 CSP 宿主进程崩溃）
- 当前状态：已解决

## 错误现象

- 执行命令：
  ```powershell
  python tools\mockhost.py dist\REDcoLAB.cpm --minimal
  ```
- 退出码：`1`
- 关键报错：
  ```text
  OSError: exception: access violation writing 0x0000000000000000
    File "mockhost.py", line 496, in main
      ok &= call(SEL_FILTER_INIT, "filterInitialize")
  ```
  插件日志停在模块注册之后，未进入 `filterInit` 完成日志。
- 受影响范围：`filterInitialize`（参数注册）阶段；真实 CSP 中同样会崩溃。

## 已尝试处理

1. 先确认这不是「必需回调被误删」：最初的最小宿主连 `getHostVersionProc` 也删了，
   插件正确地拒绝加载（返回失败而非崩溃）——**那是我的测试设计错误**，已改为保留必需回调、
   只删可选回调。
2. 修正测试替身后仍崩溃，且崩在 `filterInitialize`。
3. 排查 `filterInitialize` 分支内所有对宿主函数指针的直接调用，发现 8 处未判空：
   `addItemProc` ×2、`setIntegerValueProc`、`setIntegerDefaultValueProc`、
   `setIntegerMinValueProc`、`setIntegerMaxValueProc`、`releaseProc`，
   以及所有 `TriglavPlugInFilterInitializeSet*` 宏（它们内部通过记录里的函数指针调用）。
4. 最小宿主剥掉了 `propertyService.releaseProc`（索引 2），据此复现并确认。

## 根因判断

**已确认根因**：上一轮（ERR-20261015-001）我只为**滤镜执行阶段**加了宿主调用守卫，
**初始化阶段被漏掉了**。`filterInitialize` 里仍然直接解引用宿主函数指针。

根因的更深一层：我把「加守卫」当成了一次性的局部修补，而不是一条**贯穿全部宿主交互的规则**。
只要还有任何一个阶段没覆盖，同类崩溃就会以另一种形式复现 —— 这次它换了个位置出现。

## 后续建议

- 已新增属性服务与滤镜初始化记录的完整守卫层：
  `PropertyServiceUsable` + `CallPropertyAddItem` / `CallPropertySetInteger*` /
  `CallPropertyRelease` / `CallPropertyAddEnumerationItem` / `CallPropertySetEnumeration*` /
  `CallPropertyStoreValue`，以及 `CallSetFilterCategory` / `CallSetFilterName` /
  `CallSetCanPreview` / `CallSetTargetKinds` / `CallSetProperty` / `CallSetPropertyCallback`。
- **属性服务不可用时明确失败并记日志**（参数是滤镜的核心，缺了就不能假装能工作），
  而不是静默降级。
- 最小宿主测试已固化为 `tools/run_tests.ps1` 的常驻环节（每个产物都跑 normal + minimal 两种契约）。
- 规则已更新到 `plugin-dev` Skill 的核心不变量：**任何宿主提供的函数指针，调用前必须判空，无例外。**

## 解决信息

- 解决日期：2026-10-15
- 解决方式：补齐初始化阶段的守卫层；`PropertyServiceUsable` 前置校验。
- 验证：`tools\run_tests.ps1` 四个组合（cpm/dll × normal/minimal）全部 `RESULT: PASS`，退出码 0。
- 关联提交或任务：REDcoLAB 1.0.0 守卫层补全。
