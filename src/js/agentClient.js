export async function resolveAgentReply({ useRemote, history, localAnswer, signal, fetchImpl = fetch }) {
  const local = { ...localAnswer, source: 'local' }
  if (!useRemote) return local
  try {
    const messages = history.filter((message) => ['user', 'assistant'].includes(message.role)
      && typeof message.content === 'string' && message.content.trim())
      .slice(-20).map(({ role, content }) => ({ role, content: content.slice(0, 4000) }))
    const response = await fetchImpl('/api/agent/chat', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ messages }),
      signal: signal ? AbortSignal.any([signal, AbortSignal.timeout(25000)]) : AbortSignal.timeout(25000),
    })
    if (!response.ok) throw new Error('remote_unavailable')
    const data = await response.json()
    if (data.source !== 'openai' || typeof data.reply !== 'string' || !data.reply.trim()) throw new Error('remote_unavailable')
    return { ...local, reply: data.reply.trim(), source: 'openai' }
  } catch {
    return { ...local, source: 'fallback' }
  }
}
