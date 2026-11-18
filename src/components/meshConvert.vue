<script setup>
import { ref } from 'vue'

const meshSourceDir = ref('')
const meshOutputDir = ref('')
const meshConversionStatus = ref('Ready to convert')
const isConvertingMesh = ref(false)

async function chooseMeshFolder(target) {
  meshConversionStatus.value = 'Opening folder picker…'
  try {
    const response = await fetch('/api/select-folder', { method: 'POST' })
    const result = await response.json()
    if (!response.ok) throw new Error(result.error ?? 'Could not open the folder picker.')
    if (!result.path) {
      meshConversionStatus.value = 'Folder selection canceled.'
      return
    }
    if (target === 'source') meshSourceDir.value = result.path
    else if (target === 'output') meshOutputDir.value = result.path
    meshConversionStatus.value = 'Folder selected.'
  } catch (error) {
    meshConversionStatus.value = `Selection failed: ${error.message}`
  }
}

async function convertJsonToMesh() {
  if (!meshSourceDir.value.trim() || !meshOutputDir.value.trim()) {
    meshConversionStatus.value = 'Select the JSON source and mesh output folders.'
    return
  }

  isConvertingMesh.value = true
  meshConversionStatus.value = 'Converting. Please wait…'
  try {
    const response = await fetch('/api/build-mesh', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        sourceDir: meshSourceDir.value.trim(),
        outputDir: meshOutputDir.value.trim(),
      }),
    })
    const result = await response.json()
    if (!response.ok) throw new Error(result.error ?? 'Mesh conversion failed.')
    meshConversionStatus.value = result.message
  } catch (error) {
    meshConversionStatus.value = `Conversion failed: ${error.message}`
  } finally {
    isConvertingMesh.value = false
  }
}
</script>

<template>
  <aside class="panel converter-panel">
    <h1>JSON to Mesh</h1>
    <p class="hint">Convert numbered JSON files into <code>.mesh.json</code> files and an index.</p>
    <label for="mesh-source">JSON source folder</label>
    <div id="mesh-source" class="folder-choice">
      <button @click="chooseMeshFolder('source')">Choose folder</button>
      <span>{{ meshSourceDir || 'No selection' }}</span>
    </div>
    <label for="mesh-output">Mesh output folder</label>
    <div id="mesh-output" class="folder-choice">
      <button @click="chooseMeshFolder('output')">Choose folder</button>
      <span>{{ meshOutputDir || 'No selection' }}</span>
    </div>
    <button class="convert-button" :disabled="isConvertingMesh" @click="convertJsonToMesh">
      {{ isConvertingMesh ? 'Converting…' : 'Start conversion' }}
    </button>
    <p class="status">{{ meshConversionStatus }}</p>
  </aside>
</template>
