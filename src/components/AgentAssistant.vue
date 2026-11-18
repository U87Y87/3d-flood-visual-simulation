<script setup>
import { nextTick, onBeforeUnmount, onMounted, ref } from 'vue'
import { resolveAgentReply } from '../js/agentClient.js'

const emit = defineEmits(['navigate', 'tif-file'])

const question = ref('')
const isThinking = ref(false)
const messagesEl = ref(null)
const dataFileInput = ref(null)
const useRemote = ref(false)
const replyMode = ref('Local chat')
const requestController = new AbortController()

onMounted(async () => {
  try {
    const response = await fetch('/api/agent/config', { signal: requestController.signal })
    if (!response.ok) return
    const config = await response.json()
    if (requestController.signal.aborted) return
    useRemote.value = config.provider === 'openai'
    replyMode.value = useRemote.value ? 'ChatGPT' : 'Local chat'
  } catch { /* Keep local chat available when the API is unavailable. */ }
})
onBeforeUnmount(() => requestController.abort())
let messageId = 2

const quickQuestions = [
  'How do I process TIFF terrain?',
  'Run a flood simulation',
  'Explore the Yongchuan case',
]

const agents = [
  { name: 'Coordinator', role: 'Task planning', icon: 'CO', status: 'online' },
  { name: 'Data Agent', role: 'Data preparation', icon: 'DA', status: 'ready' },
  { name: 'Simulation Agent', role: 'Flood Simulation', icon: 'SIM', status: 'ready' },
  { name: 'Scene Agent', role: 'Map interaction', icon: '3D', status: 'ready' },
]

const messages = ref([
  {
    id: 1,
    role: 'assistant',
    agent: 'Coordinator',
    content: 'Hi, I’m your Disaster Chain assistant. Describe your task and I’ll coordinate specialist agents and suggest the right tools.',
    tools: [],
  },
])

const toolRules = [
  {
    keywords: ['tif', 'tiff', 'asc', '格式', '转换', '粗糙度', '行列', '预处理', '数据', 'format', 'convert', 'roughness', 'indices', 'preprocess', 'data'],
    agent: 'Data Agent',
    reply: 'Start with Data Processing to convert TIFF/ASC files, calculate roughness, draw breach lines, and find ASC grid indices before running a simulation.',
    tools: [
      { id: 'data', name: 'Data Processing', detail: 'Formats · Roughness · Grid indices' },
    ],
  },
  {
    keywords: ['洪水', '模拟', '水深', '淹没', '计算', '边界', 'flood', 'simulate', 'simulation', 'depth', 'inundation', 'boundary'],
    agent: 'Simulation Agent',
    reply: 'Use Flood Simulation with terrain, surface roughness, and breach location files. If needed, prepare formats and roughness in Data Processing first.',
    tools: [
      { id: 'caculate', name: 'Flood Simulation', detail: 'Parameters · Boundaries · Solver' },
      { id: 'data', name: 'Data Processing', detail: 'Prepare simulation inputs' },
    ],
  },
  {
    keywords: ['网格', '格网', 'mesh', '网格化', 'grid', 'meshing'],
    agent: 'Data Agent',
    reply: 'Use Grid Conversion to generate a mesh for rendering or computation, then inspect the result in Visualization.',
    tools: [
      { id: 'mesh', name: 'Grid Conversion', detail: 'Mesh generation · Formats' },
      { id: 'render', name: 'Visualization', detail: 'Inspect converted data' },
    ],
  },
  {
    keywords: ['可视化', '渲染', '地图', '地形', '三维', '切片', '场景', 'visualization', 'render', 'map', 'terrain', '3d', 'tile', 'scene'],
    agent: 'Scene Agent',
    reply: 'Inspect results on the 3D map in Visualization. Use Show terrain in the top bar to enable terrain, or Load tiles to import map data.',
    tools: [
      { id: 'render', name: 'Visualization', detail: '3D rendering · Animation' },
      { id: 'top-tools', name: 'Map tools', detail: 'Show terrain · Load tiles' },
    ],
  },
  {
    keywords: ['案例', '永川', '示例', '演示', '监测', 'case', 'yongchuan', 'example', 'demo', 'monitor'],
    agent: 'Scene Agent',
    reply: 'Open Case Studies and load the Yongchuan case to explore the flood animation, maximum water depth, and inundated area over time.',
    tools: [
      { id: 'sample', name: 'Case Studies', detail: 'Yongchuan · Live monitoring' },
    ],
  },
]

function scrollToLatest() {
  nextTick(() => {
    if (messagesEl.value) messagesEl.value.scrollTop = messagesEl.value.scrollHeight
  })
}

function buildAnswer(text) {
  const normalized = text.toLowerCase()
  const matched = toolRules
    .map((rule) => ({
      ...rule,
      score: rule.keywords.reduce((score, keyword) => score + (normalized.includes(keyword) ? 1 : 0), 0),
    }))
    .sort((a, b) => b.score - a.score)[0]

  if (matched?.score > 0) return matched

  return {
    agent: 'Coordinator',
    reply: 'I can help with Data Processing, Flood Simulation, Grid Conversion, Visualization, and Case Studies. Tell me your input format, desired result, or current step for more specific guidance.',
    tools: [
      { id: 'data', name: 'Data Processing', detail: 'Prepare input data' },
      { id: 'caculate', name: 'Flood Simulation', detail: 'Run the model' },
    ],
  }
}

async function sendQuestion(preset) {
  const text = (preset ?? question.value).trim()
  if (!text || isThinking.value) return

  messages.value.push({
    id: messageId++,
    role: 'user',
    content: text,
    tools: [],
  })
  question.value = ''
  isThinking.value = true
  scrollToLatest()

  if (!useRemote.value) await new Promise((resolve) => window.setTimeout(resolve, 420))
  const answer = await resolveAgentReply({
    useRemote: useRemote.value, history: messages.value,
    localAnswer: buildAnswer(text), signal: requestController.signal,
  })
  if (requestController.signal.aborted) return
  replyMode.value = answer.source === 'openai' ? 'ChatGPT' : answer.source === 'fallback' ? 'Local fallback' : 'Local chat'
  messages.value.push({
    id: messageId++,
    role: 'assistant',
    agent: answer.agent,
    content: answer.reply,
    tools: answer.tools,
    source: answer.source,
  })
  isThinking.value = false
  scrollToLatest()
}

function openTool(tool) {
  if (tool.id === 'top-tools') return
  emit('navigate', tool.id)
}

function formatFileSize(bytes) {
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / 1024 / 1024).toFixed(1)} MB`
}

function addValidationMessage(content, valid = false) {
  messages.value.push({
    id: messageId++,
    role: 'assistant',
    agent: 'Data Agent',
    content,
    tools: valid ? [{ id: 'data', name: 'Format Conversion', detail: 'File loaded. Ready to convert.' }] : [],
  })
  scrollToLatest()
}

async function hasTiffSignature(file) {
  const bytes = new Uint8Array(await file.slice(0, 4).arrayBuffer())
  const littleEndian = bytes[0] === 0x49 && bytes[1] === 0x49 && bytes[2] === 0x2a && bytes[3] === 0x00
  const bigEndian = bytes[0] === 0x4d && bytes[1] === 0x4d && bytes[2] === 0x00 && bytes[3] === 0x2a
  return littleEndian || bigEndian
}

async function handleDataFile(event) {
  const file = event.target.files?.[0]
  if (!file) return

  const isTif = /\.tiff?$/i.test(file.name)
  const isEmpty = file.size === 0
  messages.value.push({
    id: messageId++,
    role: 'user',
    content: `Validate file: ${file.name}（${formatFileSize(file.size)}）`,
    tools: [],
  })

  if (!isTif) {
    addValidationMessage('Validation failed: only TIF/TIFF files can be passed to Data Processing. Select a terrain raster file.')
    event.target.value = ''
    return
  }

  if (isEmpty) {
    addValidationMessage('Validation failed: this TIFF file is empty. Check the source file and try again.')
    event.target.value = ''
    return
  }

  try {
    if (!await hasTiffSignature(file)) {
      addValidationMessage('Validation failed: the file has a TIFF extension but an invalid TIFF header. Check for corruption or an incorrect extension.')
      event.target.value = ''
      return
    }
  } catch {
    addValidationMessage('Validation failed: the browser could not read this file. Check file permissions and try again.')
    event.target.value = ''
    return
  }

  addValidationMessage(`Validation passed: “${file.name}” has a valid TIFF extension and header. Opening Data Processing → Format Conversion with the file loaded. Select Convert to continue.`, true)
  window.setTimeout(() => emit('tif-file', file), 260)
}
</script>

<template>
  <aside class="agent-panel" aria-label="Multi-agent assistant">
    <header class="agent-header">
      <div class="agent-brand-icon" aria-hidden="true">
        <span></span>
        <span></span>
        <span></span>
      </div>
      <div>
        <span class="agent-eyebrow">MULTI-AGENT COPILOT</span>
        <h1>AI Assistant</h1>
      </div>
      <span class="agent-online"><i></i>{{ replyMode }}</span>
    </header>

    <section class="agent-roster" aria-label="Agent team">
      <div class="roster-heading">
        <span>Agent team</span>
        <small>4 agents ready</small>
      </div>
      <div class="agent-list">
        <div v-for="agent in agents" :key="agent.name" class="agent-member">
          <span class="member-icon">{{ agent.icon }}</span>
          <span class="member-info">
            <strong>{{ agent.name }}</strong>
            <small>{{ agent.role }}</small>
          </span>
          <i :class="agent.status"></i>
        </div>
      </div>
    </section>

    <section class="data-validation">
      <div>
        <span class="validation-icon" aria-hidden="true">✓</span>
        <span>
          <strong>Validate & route data</strong>
          <small>TIF / TIFF terrain files</small>
        </span>
      </div>
      <button type="button" @click="dataFileInput?.click()">Select file</button>
      <input
        ref="dataFileInput"
        type="file"
        accept=".tif,.tiff,image/tiff"
        aria-label="Select TIFF data to validate"
        @change="handleDataFile"
      >
    </section>

    <section ref="messagesEl" class="chat-messages" aria-live="polite">
      <article
        v-for="message in messages"
        :key="message.id"
        class="chat-message"
        :class="message.role"
      >
        <div v-if="message.role === 'assistant'" class="message-agent">
          <span>AI</span>
          {{ message.agent }}
        </div>
        <p>{{ message.content }}</p>
        <div v-if="message.tools.length" class="recommended-tools">
          <button
            v-for="tool in message.tools"
            :key="tool.id"
            type="button"
            :disabled="tool.id === 'top-tools'"
            @click="openTool(tool)"
          >
            <span class="tool-arrow">→</span>
            <span>
              <strong>{{ tool.name }}</strong>
              <small>{{ tool.detail }}</small>
            </span>
          </button>
        </div>
      </article>

      <div v-if="isThinking" class="thinking" aria-label="Agent is analyzing">
        <span></span><span></span><span></span>
        Finding the right tools…
      </div>
    </section>

    <div v-if="messages.length === 1" class="quick-questions">
      <span>Try asking</span>
      <button
        v-for="item in quickQuestions"
        :key="item"
        type="button"
        @click="sendQuestion(item)"
      >
        {{ item }}
      </button>
    </div>

    <form class="chat-composer" @submit.prevent="sendQuestion()">
      <textarea
        v-model="question"
        rows="2"
        maxlength="300"
        aria-label="Ask the assistant"
        placeholder="Describe your task to find the right tools…"
        @keydown.enter.exact.prevent="sendQuestion()"
      ></textarea>
      <div class="composer-footer">
        <small>Enter to send · Shift + Enter for a new line</small>
        <button type="submit" :disabled="!question.trim() || isThinking" aria-label="Send question">
          <span>Send</span>
          <i>↑</i>
        </button>
      </div>
    </form>
  </aside>
</template>

<style scoped>
.agent-panel {
  position: fixed;
  z-index: 22;
  top: calc(var(--topbar-height) + 18px);
  right: 20px;
  display: flex;
  width: min(480px, calc(100vw - var(--sidebar-width) - 40px));
  height: calc(100vh - var(--topbar-height) - 36px);
  max-height: 900px;
  flex-direction: column;
  overflow: hidden;
  border: 1px solid rgba(85, 215, 238, 0.28);
  border-radius: 16px;
  color: #e9f5ff;
  background: linear-gradient(165deg, rgba(17, 34, 52, 0.98), rgba(7, 19, 32, 0.98));
  box-shadow: 0 24px 65px rgba(0, 0, 0, 0.48), inset 0 1px rgba(255, 255, 255, 0.04);
  backdrop-filter: blur(20px);
}

.agent-header {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  gap: 12px;
  padding: 18px 18px 15px;
  border-bottom: 1px solid rgba(148, 181, 211, 0.14);
  background: linear-gradient(100deg, rgba(33, 185, 216, 0.13), transparent 70%);
}

.agent-brand-icon {
  position: relative;
  display: grid;
  width: 42px;
  height: 42px;
  flex: 0 0 auto;
  place-items: center;
  border: 1px solid rgba(85, 215, 238, 0.42);
  border-radius: 12px;
  background: rgba(33, 185, 216, 0.11);
  box-shadow: inset 0 0 18px rgba(33, 185, 216, 0.08);
}

.agent-brand-icon span {
  position: absolute;
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: #55d7ee;
  box-shadow: 0 0 8px rgba(85, 215, 238, 0.72);
}

.agent-brand-icon span:nth-child(1) { transform: translate(-9px, 6px); }
.agent-brand-icon span:nth-child(2) { transform: translate(0, -8px); }
.agent-brand-icon span:nth-child(3) { transform: translate(9px, 6px); }

.agent-brand-icon::before,
.agent-brand-icon::after {
  position: absolute;
  width: 13px;
  height: 1px;
  background: rgba(85, 215, 238, 0.7);
  content: "";
}

.agent-brand-icon::before { transform: translate(-5px, 0) rotate(-55deg); }
.agent-brand-icon::after { transform: translate(5px, 0) rotate(55deg); }

.agent-header > div:nth-child(2) {
  min-width: 0;
  flex: 1;
}

.agent-eyebrow {
  display: block;
  color: #55d7ee;
  font-size: 9px;
  font-weight: 700;
  letter-spacing: 0.17em;
}

.agent-header h1 {
  margin: 3px 0 0;
  font-size: 20px;
}

.agent-online {
  display: flex;
  align-items: center;
  gap: 5px;
  color: #7f9aae;
  font-size: 10px;
}

.agent-online i,
.agent-member > i {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: #38d996;
  box-shadow: 0 0 8px rgba(56, 217, 150, 0.7);
}

.agent-roster {
  flex: 0 0 auto;
  padding: 12px 16px;
  border-bottom: 1px solid rgba(148, 181, 211, 0.1);
}

.data-validation {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin: 10px 14px 0;
  padding: 10px;
  border: 1px solid rgba(85, 215, 238, 0.2);
  border-radius: 10px;
  background: linear-gradient(110deg, rgba(33, 185, 216, 0.1), rgba(33, 185, 216, 0.025));
}

.data-validation > div {
  display: flex;
  min-width: 0;
  align-items: center;
  gap: 8px;
}

.validation-icon {
  display: grid;
  width: 28px;
  height: 28px;
  flex: 0 0 auto;
  place-items: center;
  border: 1px solid rgba(85, 215, 238, 0.32);
  border-radius: 8px;
  color: #55d7ee;
  background: rgba(33, 185, 216, 0.1);
  font-size: 10px;
  font-weight: 700;
}

.data-validation > div > span:last-child {
  display: flex;
  min-width: 0;
  flex-direction: column;
  gap: 2px;
}

.data-validation strong {
  color: #cce0e9;
  font-size: 10px;
}

.data-validation small {
  color: #638096;
  font-size: 8px;
}

.data-validation button {
  flex: 0 0 auto;
  padding: 7px 9px;
  border: 1px solid rgba(85, 215, 238, 0.35);
  border-radius: 7px;
  color: #dff9fd;
  background: rgba(33, 185, 216, 0.12);
  font-size: 9px;
}

.data-validation button:hover {
  border-color: #55d7ee;
  background: rgba(33, 185, 216, 0.2);
}

.data-validation input {
  position: absolute;
  width: 1px;
  height: 1px;
  overflow: hidden;
  clip: rect(0 0 0 0);
  white-space: nowrap;
}

.roster-heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 9px;
}

.roster-heading span {
  color: #b9cedc;
  font-size: 11px;
  font-weight: 600;
}

.roster-heading small {
  color: #617f94;
  font-size: 9px;
}

.agent-list {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 6px;
}

.agent-member {
  position: relative;
  display: flex;
  min-width: 0;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  padding: 8px 3px 7px;
  border: 1px solid rgba(148, 181, 211, 0.1);
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.025);
  text-align: center;
}

.member-icon {
  display: grid;
  width: 25px;
  height: 25px;
  place-items: center;
  border: 1px solid rgba(85, 215, 238, 0.28);
  border-radius: 7px;
  color: #55d7ee;
  background: rgba(33, 185, 216, 0.08);
  font-size: 10px;
  font-weight: 700;
}

.member-info {
  display: flex;
  min-width: 0;
  flex-direction: column;
}

.member-info strong {
  color: #c8dae5;
  font-size: 9px;
  font-weight: 600;
  white-space: normal;
  overflow-wrap: anywhere;
  line-height: 1.4;
}

.member-info small {
  color: #8da8ba;
  font-size: 8px;
  white-space: normal;
  overflow-wrap: anywhere;
  line-height: 1.4;
}

.agent-member > i {
  position: absolute;
  top: 6px;
  right: 6px;
  width: 4px;
  height: 4px;
}

.chat-messages {
  display: flex;
  min-height: 64px;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 12px;
  padding: 16px;
  overflow-y: auto;
  scrollbar-color: rgba(85, 215, 238, 0.3) transparent;
  scrollbar-width: thin;
}

.chat-message {
  max-width: 91%;
}

.chat-message.assistant { align-self: flex-start; }
.chat-message.user { align-self: flex-end; }

.message-agent {
  display: flex;
  align-items: center;
  gap: 6px;
  margin: 0 0 5px 3px;
  color: #6f91a7;
  font-size: 9px;
}

.message-agent span {
  display: grid;
  width: 18px;
  height: 18px;
  place-items: center;
  border: 1px solid rgba(85, 215, 238, 0.28);
  border-radius: 6px;
  color: #55d7ee;
  background: rgba(33, 185, 216, 0.1);
  font-size: 7px;
  font-weight: 800;
}

.chat-message p {
  margin: 0;
  padding: 11px 13px;
  border: 1px solid rgba(148, 181, 211, 0.12);
  border-radius: 4px 12px 12px;
  color: #bfd2df;
  background: rgba(25, 48, 69, 0.72);
  font-size: 12px;
  line-height: 1.65;
}

.chat-message.user p {
  border-color: rgba(85, 215, 238, 0.25);
  border-radius: 12px 4px 12px 12px;
  color: #ecfbff;
  background: linear-gradient(135deg, rgba(25, 134, 166, 0.78), rgba(20, 94, 130, 0.8));
}

.recommended-tools {
  display: flex;
  flex-direction: column;
  gap: 6px;
  margin-top: 7px;
}

.recommended-tools button {
  display: flex;
  align-items: center;
  gap: 9px;
  width: 100%;
  padding: 8px 10px;
  border: 1px solid rgba(85, 215, 238, 0.18);
  border-radius: 8px;
  color: #d9f6fb;
  background: rgba(33, 185, 216, 0.07);
  text-align: left;
  transition: border-color 0.2s, background 0.2s, transform 0.2s;
}

.recommended-tools button:hover:not(:disabled) {
  border-color: rgba(85, 215, 238, 0.55);
  background: rgba(33, 185, 216, 0.14);
  transform: translateX(2px);
}

.recommended-tools button:disabled {
  cursor: default;
  opacity: 0.72;
}

.tool-arrow { color: #55d7ee; }

.recommended-tools button > span:last-child {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.recommended-tools strong { font-size: 11px; }
.recommended-tools small { color: #7392a6; font-size: 9px; }

.thinking {
  display: flex;
  align-items: center;
  gap: 4px;
  color: #6f91a7;
  font-size: 10px;
}

.thinking span {
  width: 4px;
  height: 4px;
  border-radius: 50%;
  background: #55d7ee;
  animation: agent-thinking 1s infinite ease-in-out;
}

.thinking span:nth-child(2) { animation-delay: 0.12s; }
.thinking span:nth-child(3) { margin-right: 4px; animation-delay: 0.24s; }

@keyframes agent-thinking {
  0%, 60%, 100% { opacity: 0.35; transform: translateY(0); }
  30% { opacity: 1; transform: translateY(-3px); }
}

.quick-questions {
  display: flex;
  flex: 0 0 auto;
  flex-wrap: wrap;
  gap: 6px;
  padding: 0 16px 12px;
}

.quick-questions > span {
  width: 100%;
  color: #637f93;
  font-size: 9px;
}

.quick-questions button {
  padding: 6px 9px;
  border: 1px solid rgba(148, 181, 211, 0.14);
  border-radius: 20px;
  color: #8eacbe;
  background: rgba(255, 255, 255, 0.025);
  font-size: 9px;
  transition: color 0.2s, border-color 0.2s;
}

.quick-questions button:hover {
  border-color: rgba(85, 215, 238, 0.42);
  color: #d9f7fb;
}

.chat-composer {
  flex: 0 0 auto;
  margin: 0 14px 14px;
  padding: 10px 11px 8px;
  border: 1px solid rgba(85, 215, 238, 0.22);
  border-radius: 11px;
  background: rgba(4, 14, 25, 0.76);
  transition: border-color 0.2s, box-shadow 0.2s;
}

.chat-composer:focus-within {
  border-color: rgba(85, 215, 238, 0.58);
  box-shadow: 0 0 0 3px rgba(33, 185, 216, 0.08);
}

.chat-composer textarea {
  display: block;
  width: 100%;
  min-height: 42px;
  resize: none;
  border: 0;
  outline: 0;
  color: #e8f5fc;
  background: transparent;
  font: inherit;
  font-size: 12px;
  line-height: 1.5;
}

.chat-composer textarea::placeholder { color: #567187; }

.composer-footer {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}

.composer-footer small {
  color: #4e6a7e;
  font-size: 8px;
}

.composer-footer button {
  display: flex;
  align-items: center;
  gap: 5px;
  padding: 6px 7px 6px 10px;
  border: 1px solid rgba(85, 215, 238, 0.4);
  border-radius: 7px;
  color: #06131d;
  background: linear-gradient(135deg, #55d7ee, #20a6cf);
  font-size: 10px;
  font-weight: 700;
}

.composer-footer button:disabled {
  cursor: not-allowed;
  filter: saturate(0.25);
  opacity: 0.45;
}

.composer-footer button i {
  display: grid;
  width: 17px;
  height: 17px;
  place-items: center;
  border-radius: 5px;
  background: rgba(5, 28, 40, 0.14);
  font-style: normal;
}

@media (max-height: 760px) {
  .agent-panel { top: calc(var(--topbar-height) + 10px); height: calc(100vh - var(--topbar-height) - 20px); }
  .agent-header { padding-block: 12px; }
  .agent-roster { padding-block: 8px; }
  .agent-member { padding-block: 6px; }
  .chat-messages { padding-block: 10px; }
}

@media (max-width: 720px) {
  .agent-panel {
    right: 10px;
    width: calc(100vw - var(--sidebar-width) - 20px);
  }
}

.agent-online { max-width: 88px; line-height: 1.4; }
.agent-online i { flex-shrink: 0; }
.member-info strong { font-size: 10px; }
.member-info small { margin-top: 2px; font-size: 9px; }
.agent-member { padding: 10px 5px; }
.data-validation strong { font-size: 12px; line-height: 1.4; }
.data-validation small { font-size: 10px; line-height: 1.5; color: #8da8ba; }
.data-validation button { font-size: 11px; }
.chat-message p { font-size: 13px; overflow-wrap: anywhere; }
.quick-questions button { text-align: left; line-height: 1.5; }
.quick-questions > span { color: #8da8ba; font-size: 10px; }
.composer-footer small { color: #8da8ba; font-size: 9px; line-height: 1.5; }
.composer-footer button { flex-shrink: 0; }
.chat-composer { flex-shrink: 0; }
@media (max-width: 440px) {
  .agent-header { display: grid; grid-template-columns: 42px minmax(0, 1fr); gap: 5px 10px; padding: 12px; }
  .agent-brand-icon { grid-row: 1 / 3; }
  .agent-header h1 { font-size: 18px; }
  .agent-eyebrow { font-size: 8px; letter-spacing: 0.08em; }
  .agent-online { grid-column: 2; margin-left: 0; max-width: none; }
  .agent-roster { padding: 10px; }
  .agent-list { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  .roster-heading { gap: 8px; }
  .data-validation { flex-wrap: wrap; margin-inline: 10px; }
  .chat-messages { padding: 10px; }
  .quick-questions { padding-inline: 10px; }
  .composer-footer { align-items: flex-end; }
}
@media (max-height: 600px) {
  .agent-panel { overflow-y: auto; }
  .agent-roster, .quick-questions { display: none; }
  .chat-messages { min-height: 100px; }
}
</style>
