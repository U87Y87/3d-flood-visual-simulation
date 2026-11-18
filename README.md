# 3D flood visual dynamic simulation

Research prototype associated with **Accelerating 3D flood visual dynamic simulation: a task-driven approach to online chained service construction using multi-agent collaboration**.

The repository contains GIS preprocessing, a cellular-automata flood solver, mesh generation, Cesium visualization, and a task-oriented assistant. The assistant selects tool recommendations through keyword rules. Users execute processing steps through the interface, which shares intermediate files between modules.

## Environment

- Use Windows for the complete desktop interface and C++ build. Folder selectors use Windows dialogs.
- Install Node.js 22.12 or later on a compatible supported release line, with npm.
- Install Python 3 with Tk support and the packages in `backend/requirements.txt`.
- For the solver, install Visual Studio 2022 or its Build Tools with the C++ desktop workload, MSVC v143, and a Windows SDK.
- Cesium ion imagery and world terrain use your own client-side ion token. The synthetic mesh test and preprocessing example run independently of map credentials.

## Quick test

Clone the public repository and enter its root directory.

```powershell
git clone https://github.com/U87Y87/3d-flood-visual-simulation.git
cd 3d-flood-visual-simulation
```

### 1. Install JavaScript dependencies

```powershell
npm ci
```

### 2. Run automated tests

```powershell
node --test tests/floodRisk.test.js tests/agentApi.test.js
```

Expected summary:

```text
tests 11
pass 11
fail 0
```

The tests exercise water-depth interpolation, building and road inundation, and the optional chat interface through mocked responses.

### 3. Generate meshes from synthetic frames

```powershell
node src/build-mesh-cache.cjs ../examples/frames ../examples/generated
```

The converter resolves its arguments relative to `src/`. Keep the `../` prefixes when using this command.

Expected files are `examples/generated/0.mesh.json`, `1.mesh.json`, and `index.json`. Each frame has **9 points and 8 triangles**, and the conversion summary reports **2 successes and 0 failures**. These two synthetic frames provide a rendering example.

### 4. Check the existing preprocessing service

```powershell
python -m pip install -r backend/requirements.txt
python tests/preprocessing_example.py
```

This quick test calls the application's existing `/convert`, `/roughness`, and `/get_ascii_values` handlers through Flask's test client. It uses the bundled TIFF and breach TXT, verifies downloaded outputs in temporary directories, and prints:

```text
PASS: FFF.asc and roughness_FFF.asc, 755 columns x 518 rows; 30 breach cells from 14 points.
```

### DEM, roughness, and breach-index example in the application

Start the [Python preprocessing API](#run-the-processing-services-on-windows) and [web interface](#run-the-visualization) in separate PowerShell windows.

1. Open **Data Processing → Format Conversion**, select [`FFF.tiff`](examples/data/FFF.tiff), and click **Convert & download ASC**. The application downloads `FFF.asc` and shares the ASC and TIFF reference with subsequent steps.
2. Open **Roughness Calculation** and use the auto-filled inputs. Click **Calculate & download roughness** to obtain `roughness_FFF.asc`. Both grids have **755 columns and 518 rows**.
3. Open **ASC Grid Indices** and select [`index (2).txt`](examples/data/index%20%282%29.txt). Keep the shared ASC and TIFF inputs, then click **Export grid indices**. The downloaded `.dat` file starts with **30**, followed by 30 zero-based `column row` pairs, counting columns from the west and rows from the south.

The TXT contains 14 WGS 84 longitude/latitude pairs describing a previously drawn breach line. The application recalculates indices from the TIFF georeferencing. To prepare a new line, use **Draw Breach → Export terrain profile (TXT)**. The normalized slope-based roughness calculation is implemented in `backend/ASC文件获取粗糙度.py`.

## Run the visualization

Run from the repository root:

```powershell
npm run dev -- --host 127.0.0.1
```

Open the URL printed by Vite. To inspect the synthetic meshes, open **Visualization → Choose mesh folder** and select the generated `examples/generated` directory. Use **Zoom to flood**, **Next frame**, or **Play animation**. The frame slider should expose two frames.

For ion imagery and world terrain, set `VITE_CESIUM_ION_TOKEN` in `.env.local` and restart Vite. This token is sent to the browser. See [optional assistant configuration](docs/chatgpt-agent.md) for the chat-provider settings.

## Run the processing services on Windows

For the Python preprocessing API, enter `backend` from the repository root and run:

```powershell
cd backend
python -m pip install -r requirements.txt
python app.py
```

The frontend connects to `http://localhost:5000`. Keep this terminal running and use another terminal at the repository root for Vite.

### Input coordinate reference system

Gridded inputs must use **WGS 1984 (WGS 84)** as the geographic coordinate system underlying a **UTM projected coordinate system**. Reproject inputs to the appropriate UTM zone and hemisphere before processing, and align grids used together. The included TIFF uses **WGS 84 / UTM zone 49N**. The breach TXT stores WGS 84 longitude/latitude coordinates for transformation into that grid.

### Build the C++ solver

In a Visual Studio Developer PowerShell at the repository root:

```powershell
msbuild solver/FloodTest.sln /p:Configuration=Release /p:Platform=x64
.\solver\x64\Release\FloodTest.exe
```

The service listens on `127.0.0.1:18080` and stores files under `flood-work` in its launch directory. `runFloodSimulation` in `solver/FloodTest/FloodTest.cpp` contains the configured dam parameters and exports numbered water-depth TXT frames.

The current frontend expects a JSON completion response containing `txtZipUrl` and `jsonZipUrl`. The bundled solver returns plain text from `/upload-all` and exposes `/download` for its result archive. Completing the frontend-to-solver chain requires aligning these response and download interfaces and connecting the TXT-to-JSON stage. Full simulation-to-rendering reproduction remains pending.

## Task-driven Multi-agent Collaboration and Service Orchestration

### Request interpretation and roles

`src/components/AgentAssistant.vue` contains `toolRules` and `buildAnswer`. It lowercases the request, scores keyword matches, and selects the highest-scoring rule. The selected object contains an agent label, a reply, and tool IDs used for navigation. Users supply file inputs and parameters through the relevant panels.

The interface displays Coordinator, Data Agent, Simulation Agent, and Scene Agent roles. Their current implementation consists of rule-selected labels and tool suggestions. Independent agent processes, inter-agent messages, dependency-graph planning, and autonomous workflow execution remain implementation gaps.

### Communication and execution

Vue events and HTTP requests carry the current workflow. `navigate` opens a tool panel. `tif-file` routes validated TIFF input into preprocessing. `pipeline-file` passes the DEM, roughness, and breach files to shared state in `src/App.vue`. Once all three exist, `handlePipelineFile` opens Flood Simulation with those files filled in.

The optional provider in `server/agentApi.js` generates advisory text. `src/js/agentClient.js` retains rule-based tool suggestions and handles fallback. Users initiate service calls through buttons in the interface.

| Operation | Source and entry point | Result or dependency |
| --- | --- | --- |
| Request-to-tool recommendation | `AgentAssistant.vue`, `buildAnswer` | Rule-selected role, reply, tool IDs |
| TIFF routing and file sharing | `src/App.vue`, `handleAgentTifFile`, `handlePipelineFile` | Panel navigation and shared input files |
| DEM conversion | `backend/app.py`, `POST /convert` | ASC DEM from a TIFF |
| Roughness calculation | `backend/app.py`, `POST /roughness` | Roughness ASC from DEM and TIFF reference |
| Breach indexing | `backend/app.py`, `POST /get_ascii_values` | DAT from DEM, TIFF, and breach coordinates |
| Simulation request | `Caculate.vue`, `runFloodSimulation`; `vite.config.js` proxy | DEM, roughness, breach DAT, and UTM zone |
| Flood calculation | `solver/FloodTest/FloodTest.cpp`; `CAFloodNEW.cpp` | Water-depth TXT frames |
| Mesh generation | `meshConvert.vue`; `vite.config.js`, `/api/build-mesh`; `src/build-mesh-cache.cjs` | Mesh frames and index from JSON frames |
| Visualization | `Render.vue`, `/api/mesh-index`, `/api/mesh-frame` | Frame selection and animation |

See the [task-driven workflow example](docs/task-driven-workflow.md) for a request, the user-executed service sequence, expected outputs, and the remaining integration work.

## Verification scope

The automated checks cover the 11 Node tests, synthetic mesh generation, and the preprocessing handlers with the bundled inputs. C++ compilation and a production frontend build can be checked using the commands above and `npm run build`. UI walkthroughs describe the controls present in the source. Full paper experiment reproduction and autonomous multi-agent coordination require further implementation and validation.

The repository currently has no project-level open-source license. License selection remains with the authors.

## Computer Code Availability

The source code and research prototype associated with this study are publicly available at https://github.com/U87Y87/3d-flood-visual-simulation.

The repository provides implementations of flood-related geospatial data preprocessing, numerical simulation, mesh generation, and three-dimensional visualization, together with the available task-oriented assistant and service interfaces. It also includes example datasets, test scripts, and documentation for installing dependencies, configuring the execution environment, and running the provided examples.

The source code can be accessed and downloaded anonymously without registration. Instructions for reproducing the available computational workflows are provided in this README.

## Source layout

- `src/`, `js/`, and `server/` contain visualization, mesh conversion, and assistant interfaces.
- `backend/` contains Python GIS preprocessing.
- `solver/` contains the C++ solver and Visual Studio solution.
- `examples/data/` contains the DEM and breach-line inputs.
- `examples/frames/` contains synthetic water-depth frames.
- `tests/` contains executable tests, and `docs/` contains configuration and workflow instructions.
