# REDcoLAB 变更记录

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
