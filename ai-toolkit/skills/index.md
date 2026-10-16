# REDcoLAB Skills 索引

先读本文件，禁止先完整读取所有 Skill 正文。

主维护源：`ai-toolkit/skills/`。兼容/外部目录：`.agents/skills/`（当前为空，优先级低于主源）。

## 加载策略

1. 每个任务先选择 **1 个主 Skill**；下面的路由表已按此设计。
2. 只有跨域任务才叠加其它 Skill，叠加顺序见「组合场景」。
3. Skill 必须通过解析器取真实路径，不得凭记忆拼路径：
   `powershell -NoProfile -ExecutionPolicy Bypass -File .agents\tools\resolve-skill-path.ps1 -Skill <name>`
   一次解析多个用逗号：`-Skill plugin-dev,build-verify`；机器可读加 `-Json`。
4. 先读 `SKILL.md` 轻入口，再按它的「按需读取」表读取 references（当前无 references 目录）。
5. 解析器报 `missing` / `conflict` 时停止加载并写错误记录到 `.agents/skill-error-records/`。

## 任务入口

### 插件本体开发

- `plugin-dev`
  - 适用：修改 `src/*.cpp` / `src/*.h` —— 插件入口 `TriglavPluginCall`、四个 selector 的分支、滤镜参数项（枚举/整数/布尔）、目标图层类型（targetKinds）、灰度算法、选区与 Alpha 处理、预览开关、运行日志。
  - 不适用：只改构建/安装/测试脚本（用 `build-verify`）；只做归档发布（用 `release-archive`）；只改文档。
  - 需要叠加：改动影响产物行为时叠加 `build-verify` 做构建与客观验证。
  - 最终验证：`tools\build.ps1` 零警告通过 + `tools\run_tests.ps1` 全部 PASS。

### 构建、安装与验证

- `build-verify`
  - 适用：`tools\build.ps1` / `tools\install_plugin.ps1` / `tools\run_tests.ps1` / `tools\mockhost.py` 相关任务；编译器与 SDK 头文件就位检查；`.cpm` 与 `.dll` 双格式产出；客观灰度校验（R==G==B、Alpha 不变）；真机加载取证。
  - 不适用：纯业务代码实现（用 `plugin-dev`）；归档与推送（用 `release-archive`）。
  - 需要叠加：改了 `src/` 后必须先用 `plugin-dev`，再回到本 Skill 验证。
  - 最终验证：模拟宿主 PASS + 导出符号检查 + 灰度断言通过；真机部分明确标注「待真机验证」或附实测输出。

### 归档与发布

- `release-archive`
  - 适用：`tools\archive.ps1`；版本号升级、CHANGELOG 更新、归档打包、commit / tag / push、GitHub Release；`_baseline/` 基线留存。
  - 不适用：日常改代码与验证（用前两个 Skill）。
  - 需要叠加：归档前必须已完成 `build-verify`。
  - 最终验证：归档包**不含** SDK 头文件；`git status` 干净；tag 与版本号一致；**推送前已获用户确认**。

## 组合场景

- 改一个灰度算法或参数项：`plugin-dev` → `build-verify`
- 修构建脚本或安装路径问题：`build-verify`
- 只调文档：无（普通文档改动）
- 发一个正式版本：`build-verify` → `release-archive`（**推送需用户确认**）
- 排查「CSP 里看不到滤镜菜单」：`build-verify`（真机取证优先）

## 路由测试集

当前为手工路由记录，尚未做成自动化 evals。

| 任务描述 | 期望主 Skill | 期望叠加 |
|---|---|---|
| 「给灰度滤镜加一个『保留 Alpha』开关」 | `plugin-dev` | `build-verify` |
| 「换一个灰度算法，用 sRGB 相对亮度」 | `plugin-dev` | `build-verify` |
| 「装完插件 CSP 滤镜菜单里没有红红子LAB」 | `build-verify` | 无 |
| 「把 dist 里的产物打包发 Release」 | `release-archive` | 无 |
| 「README 补一句已知限制」 | 无（普通文档改动） | 无 |
| 「给 Skill 索引加一个 Skill」 | 无（改的是框架自身） | 无，但要跑 `.agents\tools\validate-agent-framework.ps1` |

## 维护

- 新增 / 删除 / 改名 Skill 后必须同步本文件，并升级 `../VERSION`、写 `../CHANGELOG.md`。
- 误触发 / 漏触发时：先改 `description`，再补「不适用」条目，再调本文件的路由描述，然后重跑路由测试。
