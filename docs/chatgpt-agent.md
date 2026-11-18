# 智能体助手的可选 ChatGPT 接口

默认保持现有本地规则对话。未配置、未启用、接口超时、鉴权失败或返回空回复时，都使用本地回答和现有工具推荐。

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

当前预留的是文本对话接入，工具按钮仍由用户点击执行；没有自动运行模拟或自动操作地图。未接入 AutoGen 编排，也不使用 ChatGPT 网页登录凭据。

正式独立部署时，在 Node 服务中复用 `createAgentMiddleware` 挂载同样的两个路由；只部署 `dist` 静态文件时不包含该服务端接口，助手继续使用本地对话。

接口格式参照 [OpenAI 文本生成文档](https://developers.openai.com/api/docs/guides/text)。代码测试使用模拟响应，不调用真实付费接口。
