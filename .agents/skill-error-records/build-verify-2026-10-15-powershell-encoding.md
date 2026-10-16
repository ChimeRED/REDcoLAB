# Skill 错误记录

- 记录 ID：`ERR-20261015-002`
- 触发技能：`build-verify`
- 发生时间：2026-10-15 22:40 +08:00
- 发生阶段：脚本 / 工具
- 严重程度：中（曾导致校验器误报失败）
- 当前状态：已解决

## 错误现象

- 执行命令：
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .agents\tools\validate-agent-framework.ps1
  ```
- 退出码：`1`
- 关键报错：
  ```text
  [FAIL] skill-frontmatter: build-verify: no frontmatter; plugin-dev: no frontmatter; release-archive: no frontmatter
  ```
  但三个 `SKILL.md` 明明都有标准 Frontmatter。
- 受影响范围：框架校验器假失败；同时 `.ps1` 脚本本身出现语法错误。

## 已尝试处理

1. 直接检查文件字节：确认文件头就是 `2D 2D 2D 0A`（`---\n`），行首正确。
2. 用 `Get-Content -Raw` 读出来打印 `[regex]::Matches` 结果，发现文本里 `---` 的**位置与文件字节不符**，
   且中文字符全部变成乱码（如 `淇敼`）。
3. 确认根因后改用 `[System.IO.File]::ReadAllText($path, UTF8Encoding($false))` 读取，问题消失。
4. 顺带发现两个 `.ps1` 脚本自身也因为同一原因被读坏（中文变成 `瑙﹀彂鏉′欢`，乱码字节中含引号），
   导致 PowerShell 解析报 `Unexpected token`。改为纯 ASCII 脚本后通过。

## 根因判断

**已确认根因**：Windows PowerShell 5.1 在不指定编码时，用**系统 ANSI 代码页**读取文件内容，
而本项目的文件是 **UTF-8（无 BOM）**。

- 脚本读 `.md`/`.json` 时中文被误解码为乱码，乱码字节序列里恰好出现 `'` `"` 等字符，破坏正则与解析；
- `.ps1` 脚本自身被读取执行时同理，中文字符串会破坏语法。

这是本项目此前没有记录过的真实坑，属于框架级根因，不是单点问题。

## 后续建议

- `.ps1` 脚本一律保持**纯 ASCII**（注释也用英文）。
- 脚本读取任何 UTF-8 文本文件时必须显式指定编码。
- 该约束已写入 `ai-toolkit/AGENTS.md` §8，并由 `validate-agent-framework.ps1` 的
  `skill-frontmatter` 检查看守。
- 校验器本身也补强：`Read-Utf8Text` 统一读取入口。

## 解决信息

- 解决日期：2026-10-15
- 解决方式：校验器与解析器增加 UTF-8 显式读取；两个脚本改写为纯 ASCII。
- 验证：`validate-agent-framework.ps1` 输出 `Summary: 15 passed, 0 failed, 0 warning`，退出码 0；
  解析器三个 Skill 全部 `primary` 命中，未知 Skill 返回 `missing` + 退出码 1。
- 关联提交或任务：REDcoLAB 治理框架 1.0.0。
