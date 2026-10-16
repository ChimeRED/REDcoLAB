# Skill 错误记录

- 记录 ID：`ERR-20261015-001`
- 触发技能：`plugin-dev`
- 发生时间：2026-10-15 23:10 +08:00
- 发生阶段：测试（模拟宿主 filterRun 阶段）
- 严重程度：高（会导致 CSP 宿主进程崩溃）
- 当前状态：已解决

## 错误现象

- 执行命令：
  ```powershell
  python <legacy>\native\mockhost.py $env:TEMP\REDcoLAB_probe.dll
  ```
- 退出码：`1`
- 关键报错：
  ```text
  OSError: exception: access violation writing 0x0000000000000000
    File "mockhost.py", line 475, in main
      ok &= call(SEL_FILTER_RUN, "filterRun")
  ```
  插件日志显示最后一步停在 `[run] rgb index=0,1,2` 之后，未打印任何后续日志。
- 受影响范围：滤镜执行全流程不可用；真实 CSP 中同样会崩溃。

## 已尝试处理

1. 加日志逐行定位：确认发生在 `destination` 获取之后、`getChannelOrderProc` 之后。
2. 核对 SDK 真实定义（`TriglavPlugInService.h`）确认 `offscreenService` 在 `serviceSuite` 的索引 2、`propertyService2` 索引 9，与测试替身一致 → 排除偏移错位。
3. 逐段加日志，最终定位到 `TriglavPlugInFilterRunIsAlphaLocked(rec, &alphaLocked, host)`：
   测试替身在 `filterRunRecord` 索引 11 处**没有注册**回调（只注册了 0/7/8/9/10/12/16/17/18），
   而新实现未判空即调用，解引用空函数指针。

## 根因判断

**已确认根因**：新实现对宿主提供的可选回调做了乐观假设，直接调用而不判空。

SDK 的 `TriglavPlugInRecordSuite` / `serviceSuite` 中每个函数指针都可能为 NULL——具体暴露哪些
取决于 CSP 版本与宿主实现。旧版实现之所以「侥幸通过」，只是因为它恰好没有调用那个缺失的回调；
这不是健壮性，而是运气。

补充：`offSvc->getBlockSelectAreaProc` 同样存在未判空隐患，一并修复。

## 后续建议

- 已在 `src/REDcoLAB.cpp` 中加入统一的宿主调用守卫（`HostHasProc` + `CallXxx` 包装函数），
  所有可选宿主回调一律判空，缺失时记录日志并采用安全默认值。
- 此规则已写入 `ai-toolkit/skills/plugin-dev/SKILL.md` 的核心不变量。
- 不再依赖「旧的能跑」作为正确性证据。

## 解决信息

- 解决日期：2026-10-15
- 解决方式：新增宿主调用守卫层；`IsAlphaLocked`、`GetSelectAreaRect`、`HasSelectAreaOffscreen`、
  `GetSelectAreaOffscreen`、`UpdateDestinationOffscreenRect`、`SetProgressTotal`、`SetProgressDone`、
  `Process`、`GetDestinationOffscreen`、`GetProperty` 全部改为安全调用。
- 验证：模拟宿主 `RESULT: PASS`，`grayscale check: OK (all R==G==B)`，8 个像素全部处理。
- 关联提交或任务：REDcoLAB 1.0.0 插件本体重写。
