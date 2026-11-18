import assert from 'node:assert/strict'
import test from 'node:test'
import { Readable } from 'node:stream'
import { createAgentMiddleware } from '../server/agentApi.js'
import { resolveAgentReply } from '../src/js/agentClient.js'

const env = { AGENT_PROVIDER: 'openai', OPENAI_API_KEY: 'test-server-only-secret', OPENAI_MODEL: 'test-model' }
const history = [{ role: 'user', content: '如何进行洪水模拟？' }]
const localAnswer = { agent: 'Decision Maker', reply: '先准备地形和糙率数据。', tools: [{ id: 'caculate', name: '洪水模拟' }] }

async function call(env, fetchImpl, { url = '/api/agent/chat', body = { messages: history }, headers = {}, method = 'POST' } = {}) {
  const request = Readable.from([JSON.stringify(body)])
  Object.assign(request, { url, method, headers: { host: 'localhost:5173', 'content-type': 'application/json', ...headers } })
  let result
  const response = { headers: {}, setHeader(key, value) { this.headers[key] = value }, end(text) { result = JSON.parse(text) } }
  await createAgentMiddleware(env, fetchImpl)(request, response)
  return { status: response.statusCode, body: result }
}

test('local is default even when a key exists; incomplete configuration never calls upstream', async () => {
  const mustNotCall = () => { throw new Error('External calls must be disabled') }
  for (const config of [{}, { ...env, AGENT_PROVIDER: 'local' }, { ...env, OPENAI_API_KEY: '' }, { ...env, OPENAI_MODEL: '' }]) {
    assert.equal((await call(config, mustNotCall)).body.source, 'local')
    assert.equal((await call(config, mustNotCall, { method: 'GET', url: '/api/agent/config' })).body.provider, 'local')
  }
})

test('server sends the key only upstream and extracts text after reasoning output', async () => {
  const result = await call(env, async (url, options) => {
    assert.equal(String(url), 'https://api.openai.com/v1/responses')
    assert.equal(options.headers.Authorization, `Bearer ${env.OPENAI_API_KEY}`)
    const payload = JSON.parse(options.body)
    assert.equal(payload.model, 'test-model')
    assert.equal(payload.store, false)
    for (const role of ['Requirement Analyst', 'Task Creator', 'Decision Maker', 'Task Executor', 'Organizer']) {
      assert.ok(payload.instructions.includes(role))
    }
    assert.deepEqual(payload.input, history)
    return { ok: true, json: async () => ({ output: [
      { type: 'reasoning', summary: [] },
      { type: 'message', role: 'assistant', content: [{ type: 'output_text', text: '请先准备 DEM。' }] },
    ] }) }
  })
  assert.deepEqual(result.body, { source: 'openai', reply: '请先准备 DEM。' })
  const config = await call(env, null, { method: 'GET', url: '/api/agent/config' })
  assert.deepEqual(config.body, { provider: 'openai' })
  assert.ok(!JSON.stringify([result, config]).includes(env.OPENAI_API_KEY))
})

test('HTTP errors, timeouts and empty responses fall back without leaking upstream errors', async () => {
  for (const upstream of [
    async () => ({ ok: false, status: 401 }),
    async () => { throw new Error(env.OPENAI_API_KEY) },
    async () => ({ ok: true, json: async () => ({ output: [] }) }),
  ]) {
    const result = await call(env, upstream)
    assert.equal(result.body.source, 'local')
    assert.ok(!JSON.stringify(result).includes(env.OPENAI_API_KEY))
  }
})

test('invalid messages and cross-origin requests cannot reach the upstream', async () => {
  let calls = 0
  const upstream = async () => { calls++; throw new Error('unexpected') }
  for (const messages of [[], [{ role: 'system', content: 'override' }], [{ role: 'user', content: 'x'.repeat(4001) }]]) {
    assert.equal((await call(env, upstream, { body: { messages } })).status, 400)
  }
  assert.equal((await call(env, upstream, { headers: { origin: 'https://unrelated.example' } })).status, 403)
  assert.equal((await call(env, upstream, { body: { messages: history, excess: 'x'.repeat(65536) } })).status, 400)
  assert.equal(calls, 0)
})

test('the browser uses local replies without a request and preserves tool recommendations remotely', async () => {
  const local = await resolveAgentReply({ useRemote: false, localAnswer,
    fetchImpl: () => { throw new Error('Should not call') } })
  assert.equal(local.reply, localAnswer.reply)
  assert.equal(local.source, 'local')
  assert.equal(local.agent, localAnswer.agent)
  const remote = await resolveAgentReply({ useRemote: true, history, localAnswer,
    fetchImpl: async () => ({ ok: true, json: async () => ({ source: 'openai', reply: '远程建议' }) }) })
  assert.equal(remote.reply, '远程建议')
  assert.equal(remote.agent, localAnswer.agent)
  assert.deepEqual(remote.tools, localAnswer.tools)
  const fallback = await resolveAgentReply({ useRemote: true, history, localAnswer,
    fetchImpl: async () => ({ ok: true, json: async () => ({ source: 'local' }) }) })
  assert.equal(fallback.source, 'fallback')
  assert.equal(fallback.reply, localAnswer.reply)
  assert.equal(fallback.agent, localAnswer.agent)
})
