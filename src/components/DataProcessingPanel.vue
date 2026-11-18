<script setup>
import * as Cesium from 'cesium'
import { computed, nextTick, onBeforeUnmount, ref, watch } from 'vue'
import '../js/huaxian.js'

const props = defineProps({
  viewer: {
    type: Object,
    default: null,
  },
  initialConvertFile: {
    type: File,
    default: null,
  },
})

const emit = defineEmits(['initial-file-consumed', 'pipeline-file'])

const API_BASE = 'http://localhost:5000'
const tools = [
  { id: 'tif2asc', name: 'Format Conversion', index: '01' },
  { id: 'roughness', name: 'Roughness Calculation', index: '02' },
  { id: 'draw', name: 'Draw Breach', index: '03' },
  { id: 'rowcol', name: 'ASC Grid Indices', index: '04' },
  { id: 'spatial-reference', name: 'Spatial Reference', index: '05' },
  { id: 'asc-file', name: 'ASC File', index: '06' },
  { id: 'roughness-file', name: 'Roughness File', index: '07' },
  { id: 'breach-file', name: 'Breach File', index: '08' },
]

const menuExpanded = ref(true)
const activeTool = ref(null)
const convertFile = ref(null)
const convertFileSource = ref('')
const convertLoading = ref(false)
const convertMessage = ref('')
const convertError = ref('')
const spatialReferenceFile = ref(null)
const sharedAscFile = ref(null)
const sharedRoughnessFile = ref(null)
const sharedBreachFile = ref(null)
const roughnessShowBounds = ref(false)
const roughnessLoading = ref(false)
const roughnessMessage = ref('')
const roughnessError = ref('')
const lineTxtFile = ref(null)
const lineTxtFileSource = ref('')
const rowcolLoading = ref(false)
const rowcolMessage = ref('')
const rowcolError = ref('')
const drawingStatus = ref('Ready to draw')
const boundsEntities = []
const visibleTools = computed(() => tools.filter((tool) => {
  if (tool.id === 'asc-file') return sharedAscFile.value
  if (tool.id === 'roughness-file') return sharedRoughnessFile.value
  if (tool.id === 'breach-file') return sharedBreachFile.value
  return true
}))

function getViewer() {
  return props.viewer || window.cesiumViewer || null
}

function selectTool(toolId) {
  if (activeTool.value === toolId) {
    activeTool.value = null
    return
  }
  activeTool.value = toolId
  if (toolId === 'draw') {
    nextTick(async () => {
      try {
        await ensureDrawingModuleReady()
        drawingStatus.value = 'Terrain drawing tools are ready.'
      } catch (error) {
        drawingStatus.value = `Could not load drawing tools: ${error.message}`
      }
    })
  }
}

function setFile(event, targetRef) {
  targetRef.value = event.target.files?.[0] ?? null
}

function apiErrorMessage(error) {
  if (error instanceof TypeError && /fetch/i.test(error.message)) {
    return 'Cannot connect to the data service. Start backend/app.py on port 5000.'
  }
  return error?.message || 'Request failed'
}

function clearBoundsOnMap() {
  const viewer = getViewer()
  if (!viewer) return
  boundsEntities.splice(0).forEach((entity) => viewer.entities.remove(entity))
}

function displayBoundsOnMap(bounds) {
  const viewer = getViewer()
  if (!viewer) return
  const coordinates = bounds?.boundary_points?.length ? bounds.boundary_points : bounds?.coordinates
  if (!Array.isArray(coordinates) || !coordinates.length) return

  clearBoundsOnMap()
  const positions = coordinates
    .filter((coordinate) => Array.isArray(coordinate) && coordinate.length >= 2)
    .map((coordinate) => Cesium.Cartesian3.fromDegrees(Number(coordinate[0]), Number(coordinate[1]), 0))
  if (positions.length < 2) return

  if (Cesium.Cartesian3.distance(positions[0], positions[positions.length - 1]) > 1) {
    positions.push(Cesium.Cartesian3.clone(positions[0]))
  }

  const polygon = viewer.entities.add({
    name: 'Data Processing Boundary',
    polygon: {
      hierarchy: positions,
      material: Cesium.Color.RED.withAlpha(0.02),
      outline: true,
      outlineColor: Cesium.Color.RED,
    },
  })
  const polyline = viewer.entities.add({
    name: 'Data Processing Boundary Line',
    polyline: {
      positions,
      width: 3,
      material: Cesium.Color.RED,
      clampToGround: true,
    },
  })
  boundsEntities.push(polygon, polyline)
  viewer.flyTo(polyline, { duration: 1.2 })
}

async function fetchAndDisplayTiffBounds(file) {
  if (!file || !getViewer()) return
  try {
    const formData = new FormData()
    formData.append('file', file)
    const response = await fetch(`${API_BASE}/get_tiff_bounds`, {
      method: 'POST',
      body: formData,
    })
    const result = await response.json()
    if (response.ok && result.success && result.bounds) displayBoundsOnMap(result.bounds)
  } catch (error) {
    console.warn('TIFF 边界读取失败：', error)
  }
}

function triggerDownload(path, filename) {
  const link = document.createElement('a')
  link.style.display = 'none'
  link.href = path.startsWith('http') ? path : `${API_BASE}${path}`
  if (filename) link.download = filename
  document.body.appendChild(link)
  link.click()
  window.setTimeout(() => link.remove(), 100)
}

function formatStoredFileSize(bytes) {
  if (!Number.isFinite(bytes)) return '--'
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / 1024 / 1024).toFixed(1)} MB`
}

async function fileFromDownload(path, filename) {
  const url = path.startsWith('http') ? path : `${API_BASE}${path}`
  const response = await fetch(url)
  if (!response.ok) throw new Error('Could not read the ASC output file.')
  const blob = await response.blob()
  return new File([blob], filename, { type: blob.type || 'text/plain' })
}

function registerSpatialReference(file) {
  convertFile.value = file
  spatialReferenceFile.value = file
  sharedAscFile.value = null
  invalidateDerivedFiles()
  emit('pipeline-file', { kind: 'dem', file: null })
  convertMessage.value = ''
  convertError.value = ''
  roughnessMessage.value = ''
  roughnessError.value = ''
  rowcolMessage.value = ''
  rowcolError.value = ''
}

function invalidateDerivedFiles() {
  sharedRoughnessFile.value = null
  sharedBreachFile.value = null
  emit('pipeline-file', { kind: 'roughness', file: null })
  emit('pipeline-file', { kind: 'breach', file: null })
}

function clearDependentResults() {
  roughnessMessage.value = ''
  roughnessError.value = ''
  rowcolMessage.value = ''
  rowcolError.value = ''
}

function handleSharedAscFileChange(event) {
  sharedAscFile.value = event.target.files?.[0] ?? null
  invalidateDerivedFiles()
  emit('pipeline-file', { kind: 'dem', file: sharedAscFile.value })
  clearDependentResults()
}

async function handleSharedTifFileChange(event) {
  const file = event.target.files?.[0] ?? null
  convertFile.value = file
  spatialReferenceFile.value = file
  convertFileSource.value = file ? 'manual' : ''
  invalidateDerivedFiles()
  clearDependentResults()
  await fetchAndDisplayTiffBounds(file)
}

async function handleConvertFileChange(event) {
  const file = event.target.files?.[0] ?? null
  registerSpatialReference(file)
  convertFileSource.value = convertFile.value ? 'manual' : ''
  await fetchAndDisplayTiffBounds(convertFile.value)
}

async function handleConvertSubmit() {
  if (!convertFile.value || convertLoading.value) return
  convertLoading.value = true
  convertMessage.value = ''
  convertError.value = ''
  try {
    const formData = new FormData()
    formData.append('file', convertFile.value)
    const response = await fetch(`${API_BASE}/convert`, { method: 'POST', body: formData })
    const result = await response.json()
    if (!response.ok || !result.success) throw new Error(result.message || 'Format conversion failed')
    convertMessage.value = result.message || 'Format conversion complete'
    if (result.download_url) {
      const outputFilename = result.output_filename || `${convertFile.value.name.replace(/\.tiff?$/i, '')}.asc`
      sharedAscFile.value = await fileFromDownload(result.download_url, outputFilename)
      emit('pipeline-file', { kind: 'dem', file: sharedAscFile.value })
      triggerDownload(result.download_url, outputFilename)
    }
  } catch (error) {
    convertError.value = `Conversion failed: ${apiErrorMessage(error)}`
  } finally {
    convertLoading.value = false
  }
}

async function handleRoughnessSubmit() {
  if (!sharedAscFile.value || !spatialReferenceFile.value || roughnessLoading.value) return
  roughnessLoading.value = true
  roughnessMessage.value = ''
  roughnessError.value = ''
  try {
    const formData = new FormData()
    formData.append('file', sharedAscFile.value)
    formData.append('tif_file', spatialReferenceFile.value)
    formData.append('show_bounds', roughnessShowBounds.value ? 'true' : 'false')
    const response = await fetch(`${API_BASE}/roughness`, { method: 'POST', body: formData })
    const result = await response.json()
    if (!response.ok || !result.success) throw new Error(result.message || 'Roughness calculation failed')
    roughnessMessage.value = result.message || 'Roughness calculation complete'
    if (roughnessShowBounds.value && result.bounds) displayBoundsOnMap(result.bounds)
    if (result.download_url) {
      const outputFilename = result.output_filename || result.roughness_file || 'roughness.asc'
      sharedRoughnessFile.value = await fileFromDownload(result.download_url, outputFilename)
      emit('pipeline-file', { kind: 'roughness', file: sharedRoughnessFile.value })
      triggerDownload(result.download_url, outputFilename)
    }
  } catch (error) {
    roughnessError.value = `Calculation failed: ${apiErrorMessage(error)}`
  } finally {
    roughnessLoading.value = false
  }
}

function handleLineTxtFileChange(event) {
  setFile(event, lineTxtFile)
  lineTxtFileSource.value = lineTxtFile.value ? 'manual' : ''
  sharedBreachFile.value = null
  emit('pipeline-file', { kind: 'breach', file: null })
  rowcolMessage.value = ''
  rowcolError.value = ''
}

function parseCoordinatesFromTxt(content) {
  const coordinates = []
  let pendingLongitude = null
  let pendingLatitude = null

  for (const rawLine of content.split(/\r?\n/)) {
    const line = rawLine.trim()
    if (!line) continue
    const longitudeMatch = line.match(/(?:^|\s)(?:lon|longitude)\s*[:=]\s*(-?\d+(?:\.\d+)?)/i)
    const latitudeMatch = line.match(/(?:^|\s)(?:lat|latitude)\s*[:=]\s*(-?\d+(?:\.\d+)?)/i)
    if (longitudeMatch) pendingLongitude = Number(longitudeMatch[1])
    if (latitudeMatch) pendingLatitude = Number(latitudeMatch[1])

    if (!longitudeMatch && !latitudeMatch) {
      const values = line.match(/-?\d+(?:\.\d+)?/g)?.map(Number) ?? []
      if (values.length >= 2 && Math.abs(values[0]) <= 180 && Math.abs(values[1]) <= 90) {
        coordinates.push({ lon: values[0], lat: values[1] })
      }
    }

    if (pendingLongitude !== null && pendingLatitude !== null) {
      if (Math.abs(pendingLongitude) <= 180 && Math.abs(pendingLatitude) <= 90) {
        coordinates.push({ lon: pendingLongitude, lat: pendingLatitude })
      }
      pendingLongitude = null
      pendingLatitude = null
    }
  }
  return coordinates
}

async function handleRowcolSubmit() {
  if (!sharedAscFile.value || !spatialReferenceFile.value || !lineTxtFile.value || rowcolLoading.value) return
  rowcolLoading.value = true
  rowcolMessage.value = ''
  rowcolError.value = ''
  try {
    const coordinates = parseCoordinatesFromTxt(await lineTxtFile.value.text())
    if (!coordinates.length) throw new Error('No valid longitude/latitude coordinates found in the TXT file.')

    const formData = new FormData()
    formData.append('ascii_file', sharedAscFile.value)
    formData.append('tif_file', spatialReferenceFile.value)
    formData.append('coordinates', JSON.stringify(coordinates))
    const response = await fetch(`${API_BASE}/get_ascii_values`, { method: 'POST', body: formData })
    const result = await response.json()
    if (!response.ok || !result.success) throw new Error(result.message || 'Could not calculate grid indices.')
    rowcolMessage.value = result.message || `Processed ${coordinates.length} coordinate points.`
    if (result.download_url) {
      const outputFilename = result.output_filename || 'breach.dat'
      sharedBreachFile.value = await fileFromDownload(result.download_url, outputFilename)
      emit('pipeline-file', { kind: 'breach', file: sharedBreachFile.value })
      triggerDownload(result.download_url, outputFilename)
    }
  } catch (error) {
    rowcolError.value = `Export failed: ${apiErrorMessage(error)}`
  } finally {
    rowcolLoading.value = false
  }
}

async function ensureDrawingModuleReady() {
  window.Cesium = Cesium
  const viewer = getViewer()
  if (!viewer) throw new Error('The 3D viewer is not ready.')
  window.cesiumViewer = viewer
  if (!window.drawingModule) throw new Error('Terrain drawing tools are not ready.')
  window.drawingModule?.init?.()
  window.drawingModule?.initializeDrawingEventListeners?.()
}

async function startDrawing() {
  try {
    await ensureDrawingModuleReady()
    window.drawingModule?.startDrawing?.()
    drawingStatus.value = 'Drawing: left-click to add points; right-click to finish the line.'
  } catch (error) {
    drawingStatus.value = `Could not start drawing: ${error.message}`
  }
}

function stopDrawing() {
  window.drawingModule?.stopDrawing?.()
  drawingStatus.value = 'Drawing stopped.'
}

function clearDrawing() {
  window.drawingModule?.clearDrawing?.()
  if (lineTxtFileSource.value === 'draw') {
    lineTxtFile.value = null
    lineTxtFileSource.value = ''
    sharedBreachFile.value = null
    emit('pipeline-file', { kind: 'breach', file: null })
    rowcolMessage.value = ''
    rowcolError.value = ''
  }
  drawingStatus.value = 'Lines and points cleared.'
}

async function exportTerrainLine() {
  const exported = await window.drawingModule?.exportLineTerrainDataTXT?.()
  if (!exported?.content || !exported?.filename) return
  lineTxtFile.value = new File([exported.content], exported.filename, {
    type: 'text/plain;charset=utf-8',
  })
  lineTxtFileSource.value = 'draw'
  sharedBreachFile.value = null
  emit('pipeline-file', { kind: 'breach', file: null })
  rowcolMessage.value = 'Breach coordinates loaded. Ready to export grid indices.'
  rowcolError.value = ''
}

function checkTerrainTiles() {
  window.drawingModule?.checkAllTilesExist?.()
}

function calculateGridPoints() {
  window.drawingModule?.calculateAllIntermediatePoints?.()
}

watch(
  () => props.viewer,
  (viewer) => {
    if (!viewer) return
    window.Cesium = Cesium
    window.cesiumViewer = viewer
  },
  { immediate: true },
)

watch(
  () => props.initialConvertFile,
  (file) => {
    if (!file) return
    menuExpanded.value = true
    activeTool.value = 'tif2asc'
    registerSpatialReference(file)
    convertFileSource.value = 'agent'
    emit('initial-file-consumed')
    fetchAndDisplayTiffBounds(file)
  },
  { immediate: true },
)

onBeforeUnmount(() => {
  if (
    activeTool.value === 'draw'
    && document.getElementById('startDrawing')
    && window.cesiumViewer?.cesiumWidget?.screenSpaceEventHandler
  ) {
    try {
      window.drawingModule?.stopDrawing?.()
    } catch (error) {
      console.warn('停止地形绘制工具失败：', error)
    }
  }
  clearBoundsOnMap()
})
</script>

<template>
  <aside class="panel data-processing-panel">
    <header class="data-panel-header">
      <div>
        <span class="panel-eyebrow">DATA WORKBENCH</span>
        <h1>Data Processing</h1>
      </div>
    </header>

    <div class="data-panel-layout">
      <nav class="data-tree" aria-label="Data processing tools">
        <button class="data-tree-group" type="button" @click="menuExpanded = !menuExpanded">
          <span class="tree-arrow" :class="{ expanded: menuExpanded }">▶</span>
          <strong>Data Processing</strong>
        </button>
        <div v-show="menuExpanded" class="data-tree-children">
          <template v-for="tool in visibleTools" :key="tool.id">
            <div v-if="tool.id === 'spatial-reference'" class="data-existing-title">
              <span>Stored data</span>
              <i></i>
            </div>
            <button
              class="data-tool-nav"
              :class="{ active: activeTool === tool.id }"
              type="button"
              @click="selectTool(tool.id)"
            >
              <span>{{ tool.index }}</span>
              {{ tool.name }}
              <i
                v-if="
                  (tool.id === 'spatial-reference' && spatialReferenceFile)
                    || tool.id === 'asc-file'
                    || tool.id === 'roughness-file'
                    || tool.id === 'breach-file'
                "
                class="stored-tool-dot"
                aria-label="File ready"
              ></i>
            </button>
          </template>
        </div>
      </nav>
    </div>
    <Transition name="tool-drawer">
      <section v-if="activeTool" class="data-tool-content data-tool-drawer">
        <button class="tool-drawer-close" type="button" aria-label="Close tool panel" @click="activeTool = null">
          <span aria-hidden="true">›</span>
          Close
        </button>
        <div v-show="activeTool === 'tif2asc'" class="data-tool-block">
          <div class="tool-heading">
            <span>01</span>
            <div><h2>Format Conversion</h2><p>Convert TIF terrain data to ASC.</p></div>
          </div>
          <form @submit.prevent="handleConvertSubmit">
            <label class="upload-field">
              <span>Select a TIFF file</span>
              <input
                class="convert-file-input"
                type="file"
                accept=".tif,.tiff"
                @change="handleConvertFileChange"
              >
              <span class="convert-file-picker">
                <strong>Choose file</strong>
                <small>{{ convertFile?.name || 'No file selected' }}</small>
              </span>
            </label>
            <p v-if="convertFileSource === 'agent'" class="agent-file-ready">
              <span aria-hidden="true">✓</span>
              Validated by the assistant and loaded. Ready to convert.
            </p>
            <button class="data-primary-button" type="submit" :disabled="!convertFile || convertLoading">
              {{ convertLoading ? 'Converting…' : 'Convert & download ASC' }}
            </button>
            <p v-if="convertMessage" class="data-result success">{{ convertMessage }}</p>
            <p v-if="convertError" class="data-result error">{{ convertError }}</p>
          </form>
        </div>

        <div v-show="activeTool === 'roughness'" class="data-tool-block">
          <div class="tool-heading">
            <span>02</span>
            <div><h2>Roughness Calculation</h2><p>Generate a roughness raster from ASC/TXT terrain data.</p></div>
          </div>
          <form @submit.prevent="handleRoughnessSubmit">
            <span class="dependency-title">Shared inputs are filled automatically. Replace them if needed.</span>
            <label class="upload-field shared-upload-field">
              <span>
                ASC/TXT data file
                <i v-if="sharedAscFile">06 · Auto-filled</i>
              </span>
              <input
                class="shared-file-input"
                type="file"
                accept=".asc,.txt"
                @change="handleSharedAscFileChange"
              >
              <span class="convert-file-picker">
                <strong>{{ sharedAscFile ? 'Replace file' : 'Choose file' }}</strong>
                <small>{{ sharedAscFile?.name || 'No file selected' }}</small>
              </span>
            </label>
            <label class="upload-field shared-upload-field">
              <span>
                TIF reference file
                <i v-if="spatialReferenceFile">05 · Auto-filled</i>
              </span>
              <input
                class="shared-file-input"
                type="file"
                accept=".tif,.tiff"
                @change="handleSharedTifFileChange"
              >
              <span class="convert-file-picker">
                <strong>{{ spatialReferenceFile ? 'Replace file' : 'Choose file' }}</strong>
                <small>{{ spatialReferenceFile?.name || 'No file selected' }}</small>
              </span>
            </label>
            <label class="data-check">
              <input v-model="roughnessShowBounds" type="checkbox">
              <span>Show data bounds on the 3D map</span>
            </label>
            <button
              class="data-primary-button"
              type="submit"
              :disabled="!sharedAscFile || !spatialReferenceFile || roughnessLoading"
            >
              {{ roughnessLoading ? 'Calculating…' : 'Calculate & download roughness' }}
            </button>
            <p v-if="roughnessMessage" class="data-result success">{{ roughnessMessage }}</p>
            <p v-if="roughnessError" class="data-result error">{{ roughnessError }}</p>
          </form>
        </div>

        <div v-show="activeTool === 'rowcol'" class="data-tool-block">
          <div class="tool-heading">
            <span>04</span>
            <div><h2>ASC Grid Indices</h2><p>Export grid row and column indices for the drawn coordinates.</p></div>
          </div>
          <form @submit.prevent="handleRowcolSubmit">
            <span class="dependency-title">Shared inputs are filled automatically. Replace them if needed.</span>
            <label class="upload-field shared-upload-field">
              <span>
                ASC data file
                <i v-if="sharedAscFile">06 · Auto-filled</i>
              </span>
              <input
                class="shared-file-input"
                type="file"
                accept=".asc,.txt"
                @change="handleSharedAscFileChange"
              >
              <span class="convert-file-picker">
                <strong>{{ sharedAscFile ? 'Replace file' : 'Choose file' }}</strong>
                <small>{{ sharedAscFile?.name || 'No file selected' }}</small>
              </span>
            </label>
            <label class="upload-field shared-upload-field">
              <span>
                TIF reference file
                <i v-if="spatialReferenceFile">05 · Auto-filled</i>
              </span>
              <input
                class="shared-file-input"
                type="file"
                accept=".tif,.tiff"
                @change="handleSharedTifFileChange"
              >
              <span class="convert-file-picker">
                <strong>{{ spatialReferenceFile ? 'Replace file' : 'Choose file' }}</strong>
                <small>{{ spatialReferenceFile?.name || 'No file selected' }}</small>
              </span>
            </label>
            <label class="upload-field shared-upload-field">
              <span>
                Coordinate file (TXT)
                <i v-if="lineTxtFileSource === 'draw'">03 · Auto-filled</i>
              </span>
              <input
                class="shared-file-input"
                type="file"
                accept=".txt"
                @change="handleLineTxtFileChange"
              >
              <span class="convert-file-picker">
                <strong>{{ lineTxtFile ? 'Replace file' : 'Choose file' }}</strong>
                <small>{{ lineTxtFile?.name || 'No file selected' }}</small>
              </span>
            </label>
            <button
              class="data-primary-button"
              type="submit"
              :disabled="!sharedAscFile || !spatialReferenceFile || !lineTxtFile || rowcolLoading"
            >
              {{ rowcolLoading ? 'Exporting…' : 'Export grid indices' }}
            </button>
            <p v-if="rowcolMessage" class="data-result success">{{ rowcolMessage }}</p>
            <p v-if="rowcolError" class="data-result error">{{ rowcolError }}</p>
          </form>
        </div>

        <div v-show="activeTool === 'draw'" class="data-tool-block terrain-tool">
          <div class="tool-heading">
            <span>03</span>
            <div><h2>Draw Breach</h2><p>Draw a breach line on the 3D map and extract its terrain profile.</p></div>
          </div>
          <div class="drawing-actions">
            <button id="startDrawing" type="button" @click="startDrawing">Start drawing</button>
            <button id="stopDrawing" type="button" @click="stopDrawing">Stop drawing</button>
            <button id="clearDrawing" type="button" @click="clearDrawing">Clear lines</button>
          </div>
          <p class="drawing-status">{{ drawingStatus }}</p>
          <div class="coordinate-section">
            <div class="section-label">Coordinates</div>
            <div id="coordinatesList" class="coordinates-list">
              <p class="text-muted">No points yet</p>
            </div>
          </div>
          <div class="terrain-actions">
            <button type="button" @click="exportTerrainLine">Export terrain profile (TXT)</button>
            <button type="button" @click="checkTerrainTiles">Check terrain tiles</button>
            <button type="button" @click="calculateGridPoints">Calculate terrain grid</button>
          </div>
          <div id="intermediatePointsContainer" class="intermediate-points d-none">
            <div class="section-label">Terrain grid results</div>
            <div id="intermediatePointsList"></div>
          </div>
        </div>

        <div v-show="activeTool === 'spatial-reference'" class="data-tool-block">
          <div class="tool-heading">
            <span>05</span>
            <div><h2>Spatial Reference</h2><p>The geospatial reference from your selected TIF file.</p></div>
          </div>
          <div v-if="spatialReferenceFile" class="stored-file-card ready">
            <span class="stored-file-icon">TIF</span>
            <div>
              <strong>{{ spatialReferenceFile.name }}</strong>
              <small>{{ formatStoredFileSize(spatialReferenceFile.size) }} · TIFF spatial reference</small>
            </div>
            <i>Ready</i>
          </div>
          <div v-else class="stored-file-empty">
            <span>No spatial reference file</span>
            <p>Open “01 Format Conversion” and select a TIF file first.</p>
            <button type="button" @click="activeTool = 'tif2asc'">Open Format Conversion</button>
          </div>
          <p v-if="spatialReferenceFile" class="stored-file-note">
            Roughness and grid index calculations reuse this file automatically.
          </p>
        </div>

        <div v-show="activeTool === 'asc-file'" class="data-tool-block">
          <div class="tool-heading">
            <span>06</span>
            <div><h2>ASC File</h2><p>Shared elevation data generated by format conversion.</p></div>
          </div>
          <div v-if="sharedAscFile" class="stored-file-card ready">
            <span class="stored-file-icon asc">ASC</span>
            <div>
              <strong>{{ sharedAscFile.name }}</strong>
              <small>{{ formatStoredFileSize(sharedAscFile.size) }} · ESRI ASCII Grid</small>
            </div>
            <i>Ready</i>
          </div>
          <p class="stored-file-note">
            Generated by “01 Format Conversion” and shared by roughness and grid index calculations.
          </p>
        </div>

        <div v-show="activeTool === 'roughness-file'" class="data-tool-block">
          <div class="tool-heading">
            <span>07</span>
            <div><h2>Roughness File</h2><p>Simulation input generated by the roughness calculation.</p></div>
          </div>
          <div v-if="sharedRoughnessFile" class="stored-file-card ready">
            <span class="stored-file-icon asc">ASC</span>
            <div>
              <strong>{{ sharedRoughnessFile.name }}</strong>
              <small>{{ formatStoredFileSize(sharedRoughnessFile.size) }} · Simulation roughness</small>
            </div>
            <i>Ready</i>
          </div>
          <p class="stored-file-note">
            Generated by “02 Roughness Calculation” for use in Flood Simulation.
          </p>
        </div>

        <div v-show="activeTool === 'breach-file'" class="data-tool-block">
          <div class="tool-heading">
            <span>08</span>
            <div><h2>Breach File</h2><p>DAT data generated from ASC grid indices.</p></div>
          </div>
          <div v-if="sharedBreachFile" class="stored-file-card ready">
            <span class="stored-file-icon dat">DAT</span>
            <div>
              <strong>{{ sharedBreachFile.name }}</strong>
              <small>{{ formatStoredFileSize(sharedBreachFile.size) }} · Simulation breach data</small>
            </div>
            <i>Ready</i>
          </div>
          <p class="stored-file-note">
            Generated by “04 ASC Grid Indices” for use in Flood Simulation.
          </p>
        </div>
      </section>
    </Transition>
  </aside>
</template>
