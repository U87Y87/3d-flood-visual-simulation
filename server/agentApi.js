// Server-only module. Never import this file from src/ or expose the API key via VITE_*.
const INSTRUCTIONS = `You are the assistant for the Multi-Agent Flood Chain Service System. Respond in English. Available modules are Data Processing, Flood Simulation, Grid Conversion, Visualization, and Case Studies.
Explain the workflow and required inputs. Provide advice only; never claim to have read user files, run simulations, changed the map, or executed tools.
Do not invent water depth, velocity, damage, or simulation results without user-provided evidence. Keep replies concise.`

function configFromEnv(env) {
  const key = String(env.OPENAI_API_KEY ?? '').trim() // 在 .env.local 的此同名变量中填写 Token / API Key。
  const model = String(env.OPENAI_MODEL ?? '').trim()
  const enabled = env.AGENT_PROVIDER === 'openai' && Boolean(key && model)
  const timeout = Number(env.OPENAI_TIMEOUT_MS)
  return {
    key, model, enabled,
    baseUrl: String(env.OPENAI_BASE_URL || 'https://api.openai.com/v1').replace(/\/+$/, ''),
    timeout: Number.isFinite(timeout) && timeout > 0 ? Math.min(timeout, 60000) : 20000,
  }
}

function send(response, status, body) {
  if (response.destroyed || response.writableEnded) return
  response.statusCode = status
  response.setHeader('Content-Type', 'application/json; charset=utf-8')
  response.setHeader('Cache-Control', 'no-store')
  response.end(JSON.stringify(body))
}

async function readMessages(request) {
  let size = 0
  const chunks = []
  for await (const chunk of request) {
    size += Buffer.byteLength(chunk)
    if (size > 65536) throw new Error('invalid_request')
    chunks.push(Buffer.from(chunk))
  }
  const { messages } = JSON.parse(Buffer.concat(chunks).toString('utf8'))
  if (!Array.isArray(messages) || !messages.length || messages.length > 20) throw new Error('invalid_request')
  const cleaned = messages.map((message) => {
    if (!message || !['user', 'assistant'].includes(message.role)
      || typeof message.content !== 'string' || !message.content.trim() || message.content.length > 4000) {
      throw new Error('invalid_request')
    }
    return { role: message.role, content: message.content }
  })
  if (cleaned.at(-1).role !== 'user') throw new Error('invalid_request')
  return cleaned
}

export function createAgentMiddleware(env = {}, fetchImpl = globalThis.fetch) {
  const config = configFromEnv(env)
  return async (request, response, next = () => {}) => {
    const route = request.url?.split('?')[0]
    if (!['/api/agent/config', '/api/agent/chat'].includes(route)) return next()
    if (route === '/api/agent/config' && request.method === 'GET') {
      // Only public mode information is returned. The token stays on the server.
      return send(response, 200, { provider: config.enabled ? 'openai' : 'local' })
    }
    if (route !== '/api/agent/chat' || request.method !== 'POST') {
      return send(response, 405, { error: 'method_not_allowed' })
    }
    // Prevent a third-party browser page from consuming the configured API key.
    if (request.headers.origin) {
      try {
        if (new URL(request.headers.origin).host !== request.headers.host) throw new Error()
      } catch {
        return send(response, 403, { error: 'origin_not_allowed' })
      }
    }
    if (!config.enabled) return send(response, 200, { source: 'local', reason: 'disabled_or_unconfigured' })
    if (!request.headers['content-type']?.toLowerCase().startsWith('application/json')) {
      return send(response, 415, { error: 'json_required' })
    }
    let messages
    try { messages = await readMessages(request) } catch {
      return send(response, 400, { error: 'invalid_request' })
    }
    try {
      const url = new URL(`${config.baseUrl}/responses`)
      if (url.protocol !== 'https:' && !(url.protocol === 'http:' && ['localhost', '127.0.0.1', '[::1]'].includes(url.hostname))) {
        throw new Error('invalid_endpoint')
      }
      const upstream = await fetchImpl(url, {
        method: 'POST',
        redirect: 'error',
        headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${config.key}` },
        body: JSON.stringify({ model: config.model, instructions: INSTRUCTIONS, input: messages, store: false, max_output_tokens: 1200 }),
        signal: AbortSignal.timeout(config.timeout),
      })
      if (!upstream.ok) throw new Error('upstream_error')
      const data = await upstream.json()
      // Raw Responses API returns message/content arrays, unlike the SDK output_text helper.
      const reply = (data.output ?? [])
        .filter((item) => item.type === 'message' && item.role === 'assistant')
        .flatMap((item) => item.content ?? [])
        .filter((item) => item.type === 'output_text' && typeof item.text === 'string')
        .map((item) => item.text).join('\n').trim()
      if (!reply) throw new Error('empty_reply')
      return send(response, 200, { source: 'openai', reply })
    } catch {
      // Never echo upstream errors or headers: they can contain credentials.
      return send(response, 200, { source: 'local', reason: 'remote_unavailable' })
    }
  }
}

export function agentApiPlugin(env) {
  return {
    name: 'optional-chatgpt-agent-api',
    configureServer(server) { server.middlewares.use(createAgentMiddleware(env)) },
    configurePreviewServer(server) { server.middlewares.use(createAgentMiddleware(env)) },
  }
}
