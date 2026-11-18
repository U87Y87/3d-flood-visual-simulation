# 智能体助手的可选 ChatGPT 接口

默认保持现有本地规则对话。未配置、未启用、接口超时、鉴权失败或返回空回复时，都使用本地回答和现有工具推荐。

## 论文角色定义

角色名称和职责按论文第 2.2.2、2.4.2 和 3.1 节统一，在 `src/js/agentRoles.js` 中维护。

- Requirement Analyst（需求分析者）解析需求及约束，生成标准化任务指令。
- Task Creator（任务创建者）分解任务节点并初始化参数。
- Decision Maker（决策者）协调协作、匹配服务并下发执行任务。
- Task Executor（任务执行者）调用数据处理、模拟、可视化及分析原子服务。
- Organizer（组织者）向决策者报告任务数、参与角色和执行顺序，并校验、整合执行结果。

界面列出五个角色。输入解释和 TIFF 校验使用 Requirement Analyst 标签，工具推荐使用 Decision Maker 标签。远程回复保留所选角色，服务提供方显示在单独的状态标识中。

## 填写 Token 并启用

1. 将项目根目录的 `.env.example` 复制为 `.env.local`。
2. 在 `OPENAI_API_KEY=` 后填写自己的 OpenAI API Key（Token）。不要在 Vue 文件中填写，也不要加 `VITE_` 前缀。
3. 在 `OPENAI_MODEL=` 后填写账号实际可用的 API 模型 ID。ChatGPT 界面中的模型显示名称不应直接当作 API ID。
4. 将 `AGENT_PROVIDER=local` 改为 `AGENT_PROVIDER=openai`，重启 `npm run dev`。

要恢复默认本地对话，将 `AGENT_PROVIDER` 改回 `local` 并重启。只填写 Token 不会自动启用外部调用。

## 接入位置

- `server/agentApi.js`：服务端读取 Token，调用 `POST /v1/responses`，解析文本输出。密钥不会返回浏览器，也不会记录上游原始错误。
- `src/js/agentClient.js`：浏览器对话请求、上下文裁剪和失败回退。
- `src/components/AgentAssistant.vue`：保留现有本地规则及工具按钮，按服务端配置选择对话来源。
- `vite.config.js`：为开发服务及 `npm run preview` 挂载 `/api/agent/config` 和 `/api/agent/chat`。

对话接口提供文本建议。Automatic workflow 入口执行已经实现的固定依赖服务链并自动加载结果。论文中的五角色 AutoGen 实例、角色间消息队列和按需求动态规划仍需要对应的方法实现。

正式独立部署时，在 Node 服务中复用 `createAgentMiddleware` 挂载同样的两个路由；只部署 `dist` 静态文件时不包含该服务端接口，助手继续使用本地对话。

接口格式参照 [OpenAI 文本生成文档](https://developers.openai.com/api/docs/guides/text)。代码测试使用模拟响应，不调用真实付费接口。
