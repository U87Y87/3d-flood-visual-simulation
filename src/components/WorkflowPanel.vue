<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'

defineProps({ renderError: { type: String, default: '' } })
const emit = defineEmits(['inputs-ready', 'completed'])
const terrain = ref(null)
const breach = ref(null)
const job = ref(null)
const error = ref('')
const submitting = ref(false)
const controller = new AbortController()
let timer
let loadedInputsFor = ''
let displayedJob = ''
const running = computed(() => submitting.value || ['queued', 'running'].includes(job.value?.state))
const steps = [
  ['dem', 'TIFF to ASC'],
  ['roughness', 'Roughness'],
  ['breach', 'Breach indices'],
  ['simulation', 'Simulation · 100 frames'],
  ['mesh', 'JSON to mesh'],
  ['complete', 'Visualization'],
]
const stagePosition = computed(() => steps.findIndex(([id]) => id === job.value?.stage))

async function readResponse(response) {
  const data = await response.json()
  if (!response.ok) throw new Error(data.error || data.message || 'Workflow request failed.')
  return data
}

async function shareInputs(state) {
  if (loadedInputsFor === state.id || Object.keys(state.artifacts ?? {}).length !== 3) return
  const files = {}
  for (const [kind, name] of [['dem', 'dem.asc'], ['roughness', 'cao.asc'], ['breach', 'InputRiver.dat']]) {
    const response = await fetch(state.artifacts[name], { signal: controller.signal })
    if (!response.ok) throw new Error('Could not load a generated simulation input.')
    files[kind] = new File([await response.blob()], name)
  }
  loadedInputsFor = state.id
  emit('inputs-ready', { ...files, utmZone: state.utmZone })
}

async function poll() {
  try {
    const response = await fetch('/api/workflow/jobs/' + job.value.id, { signal: controller.signal })
    if (response.status === 404) {
      sessionStorage.removeItem('floodWorkflowJob')
      error.value = 'The previous workflow is unavailable. Select inputs to start another run.'
      job.value = null
      return
    }
    const state = await readResponse(response)
    job.value = state
    error.value = ''
    if (state.utmZone) await shareInputs(state)
    if (state.state === 'completed' && displayedJob !== state.id) {
      displayedJob = state.id
      emit('completed', state)
    }
    if (['queued', 'running'].includes(state.state)) timer = setTimeout(poll, 1000)
  } catch (cause) {
    if (controller.signal.aborted) return
    error.value = cause.message
    timer = setTimeout(poll, 3000)
  }
}

async function start() {
  if (!terrain.value || !breach.value || running.value) return
  clearTimeout(timer)
  error.value = ''
  submitting.value = true
  try {
    const form = new FormData()
    form.append('tif', terrain.value)
    form.append('breach', breach.value)
    const response = await fetch('/api/workflow/jobs', { method: 'POST', body: form, signal: controller.signal })
    job.value = await readResponse(response)
    sessionStorage.setItem('floodWorkflowJob', job.value.id)
    await poll()
  } catch (cause) {
    if (!controller.signal.aborted) error.value = cause.message
  } finally {
    submitting.value = false
  }
}

async function cancel() {
  try {
    const response = await fetch('/api/workflow/jobs/' + job.value.id + '/cancel', {
      method: 'POST', signal: controller.signal,
    })
    await readResponse(response)
  } catch (cause) {
    if (!controller.signal.aborted) error.value = cause.message
  }
}

onMounted(() => {
  const id = sessionStorage.getItem('floodWorkflowJob')
  if (id) {
    job.value = { id }
    poll()
  }
})
onBeforeUnmount(() => {
  clearTimeout(timer)
  controller.abort()
})
</script>

<template>
  <aside class="panel workflow-panel">
    <h1>Automatic workflow</h1>
    <label class="workflow-file">
      Terrain (TIF / TIFF)
      <input type="file" accept=".tif,.tiff" :disabled="running" @change="terrain = $event.target.files?.[0] ?? null">
    </label>
    <label class="workflow-file">
      Breach line (TXT)
      <input type="file" accept=".txt" :disabled="running" @change="breach = $event.target.files?.[0] ?? null">
    </label>
    <p class="hint">Prepare inputs, detect the UTM zone, simulate 100 frames, generate meshes, and display the result.</p>
    <button class="convert-button" :disabled="!terrain || !breach || running" @click="start">
      {{ running ? 'Workflow running…' : 'Run 100-frame workflow' }}
    </button>
    <button v-if="job && ['queued', 'running'].includes(job.state)" class="workflow-cancel" @click="cancel">Cancel workflow</button>
    <ol v-if="job" class="workflow-steps">
      <li v-for="([id, label], position) in steps" :key="id"
          :class="{ done: position < stagePosition || job.state === 'completed', current: position === stagePosition }">
        {{ label }}
        <span v-if="position === stagePosition && ['simulation', 'mesh'].includes(id)">{{ job.completedFrames }} / 100</span>
      </li>
    </ol>
    <p v-if="job?.utmZone" class="status">WGS 84 / UTM {{ job.utmZone }}{{ job.hemisphere }} · {{ job.columns }} × {{ job.rows }} · {{ job.breachCells }} breach cells</p>
    <p v-if="job" class="status" role="status">{{ job.state }} · {{ job.message }}</p>
    <nav v-if="job?.artifacts" class="workflow-artifacts" aria-label="Generated inputs">
      <a v-for="(url, name) in job.artifacts" :key="name" :href="url" download>{{ name }}</a>
    </nav>
    <p v-if="job?.meshFolder" class="workflow-path">{{ job.meshFolder }}</p>
    <button v-if="job?.state === 'completed'" class="convert-button" @click="emit('completed', job)">Show result</button>
    <p v-if="error || renderError" class="workflow-error" role="alert">{{ error || renderError }}</p>
  </aside>
</template>

<style scoped>
.workflow-panel { width: 365px; max-height: calc(100vh - 100px); overflow-y: auto; }
.workflow-file { display: grid; gap: 8px; margin: 16px 0; color: #d5e3ed; font-size: 13px; }
.workflow-file input { width: 100%; color: #a9c5d5; }
.workflow-steps { display: grid; gap: 12px; padding-left: 22px; color: #8da6b7; font-size: 13px; }
.workflow-steps li span { float: right; }
.workflow-steps .done { color: #61e5a2; }
.workflow-steps .current { color: #50d4ed; }
.workflow-artifacts { display: flex; flex-wrap: wrap; gap: 12px; margin: 14px 0; }
.workflow-artifacts a { color: #50d4ed; font-size: 12px; }
.workflow-path { overflow-wrap: anywhere; color: #a9c5d5; font-size: 12px; }
.workflow-error { color: #ff9292; font-size: 13px; }
.workflow-cancel { margin-top: 8px; padding: 8px 12px; border: 1px solid #486174; border-radius: 6px; background: transparent; color: #d5e3ed; cursor: pointer; }
</style>
