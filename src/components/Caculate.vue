<script setup>
import { ref, watch } from 'vue'

const props = defineProps({
  initialFiles: {
    type: Object,
    default: () => ({ dem: null, roughness: null, breach: null }),
  },
})

const emit = defineEmits(['files-change'])

const demFile = ref(null)
const roughnessFile = ref(null)
const breachFile = ref(null)
const demInput = ref(null)
const roughnessInput = ref(null)
const breachInput = ref(null)
const utmZone = ref('')
const simulationStatus = ref('Select simulation input files to begin.')
const isSimulating = ref(false)

function currentFiles() {
  return {
    dem: demFile.value,
    roughness: roughnessFile.value,
    breach: breachFile.value,
  }
}

function handleFloodFile(event, kind) {
  const file = event.target.files?.[0] ?? null
  if (kind === 'dem') demFile.value = file
  else if (kind === 'roughness') roughnessFile.value = file
  else breachFile.value = file
  emit('files-change', currentFiles())
  simulationStatus.value = file ? 'File selected. Ready to simulate.' : 'Select simulation input files to begin.'
}

async function downloadZip(url, fallbackName) {
  const response = await fetch(`/api${url}`)
  if (!response.ok) throw new Error(`Failed to download ${fallbackName}: ${await response.text()}`)

  const blob = await response.blob()
  const objectUrl = URL.createObjectURL(blob)
  const link = document.createElement('a')
  link.href = objectUrl
  link.download = fallbackName
  document.body.appendChild(link)
  link.click()
  link.remove()
  setTimeout(() => URL.revokeObjectURL(objectUrl), 2000)
}

async function runFloodSimulation() {
  if (!demFile.value || !roughnessFile.value || !breachFile.value) {
    simulationStatus.value = 'Select DEM, roughness, and breach files first.'
    return
  }

  const normalizedUtmZone = String(utmZone.value).trim()
  if (!/^(?:[1-9]|[1-5]\d|60)$/.test(normalizedUtmZone)) {
    simulationStatus.value = 'Enter a whole-number UTM zone from 1 to 60.'
    return
  }

  isSimulating.value = true
  simulationStatus.value = 'Running flood simulation…'
  try {
    const formData = new FormData()
    formData.append('dem', demFile.value, 'dem.asc')
    formData.append('cao', roughnessFile.value, 'cao.asc')
    formData.append('river', breachFile.value, 'InputRiver.dat')
    const response = await fetch(`/api/upload-flood-files?utmZone=${encodeURIComponent(normalizedUtmZone)}`, {
      method: 'POST',
      body: formData,
    })
    const result = await response.json()
    if (!response.ok) throw new Error(result.error ?? 'Simulation failed.')

    simulationStatus.value = 'Simulation complete. Downloading TXT archive…'
    await downloadZip(result.txtZipUrl, 'flood_txt.zip')
    simulationStatus.value = 'TXT downloaded. Downloading JSON archive…'
    await downloadZip(result.jsonZipUrl, 'flood_json.zip')
    simulationStatus.value = 'Archives downloaded. Restarting the backend…'

    if (result.restartUrl) {
      const restartResponse = await fetch(`/api${result.restartUrl}`, { method: 'POST' })
      const restartResult = await restartResponse.json().catch(() => ({}))
      if (!restartResponse.ok) throw new Error(restartResult.error || 'Backend restart request failed.')
    }
    simulationStatus.value = 'Downloads complete. Backend is restarting.'
  } catch (error) {
    simulationStatus.value = `Simulation failed: ${error.message}`
  } finally {
    isSimulating.value = false
  }
}

watch(
  () => props.initialFiles,
  (files) => {
    if (files.utmZone !== undefined) utmZone.value = String(files.utmZone)
    if (files.dem !== undefined) demFile.value = files.dem
    if (files.roughness !== undefined) roughnessFile.value = files.roughness
    if (files.breach !== undefined) breachFile.value = files.breach
    if (demFile.value && roughnessFile.value && breachFile.value) {
      simulationStatus.value = 'All three inputs are loaded. Set the UTM zone to continue.'
    }
  },
  { immediate: true },
)
</script>

<template>
  <aside class="panel simulation-panel">
    <h1>Flood Simulation</h1>
    <label>DEM file (ASC)</label>
    <div class="folder-choice">
      <button type="button" @click="demInput?.click()">{{ demFile ? 'Replace file' : 'Choose file' }}</button>
      <span>{{ demFile?.name || 'No selection' }}</span>
      <input
        ref="demInput"
        class="simulation-file-input"
        type="file"
        accept=".asc"
        @change="handleFloodFile($event, 'dem')"
      >
    </div>
    <label>Roughness file (ASC)</label>
    <div class="folder-choice">
      <button type="button" @click="roughnessInput?.click()">{{ roughnessFile ? 'Replace file' : 'Choose file' }}</button>
      <span>{{ roughnessFile?.name || 'No selection' }}</span>
      <input
        ref="roughnessInput"
        class="simulation-file-input"
        type="file"
        accept=".asc"
        @change="handleFloodFile($event, 'roughness')"
      >
    </div>
    <label>Breach file (DAT)</label>
    <div class="folder-choice">
      <button type="button" @click="breachInput?.click()">{{ breachFile ? 'Replace file' : 'Choose file' }}</button>
      <span>{{ breachFile?.name || 'No selection' }}</span>
      <input
        ref="breachInput"
        class="simulation-file-input"
        type="file"
        accept=".dat"
        @change="handleFloodFile($event, 'breach')"
      >
    </div>
    <label for="utm-zone">UTM zone (1–60)</label>
    <input
      id="utm-zone"
      v-model="utmZone"
      class="utm-zone-input"
      type="number"
      min="1"
      max="60"
      step="1"
      placeholder="e.g. 48"
    >
    <button class="convert-button" :disabled="isSimulating" @click="runFloodSimulation">
      {{ isSimulating ? 'Simulating…' : 'Run simulation' }}
    </button>
    <p class="status">{{ simulationStatus }}</p>
  </aside>
</template>
