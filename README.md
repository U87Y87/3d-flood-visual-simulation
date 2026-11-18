# 3D flood visual dynamic simulation

Research prototype associated with **Accelerating 3D flood visual dynamic simulation: a task-driven approach to online chained service construction using multi-agent collaboration**.

The repository contains GIS preprocessing, a cellular-automata flood solver, mesh generation, Cesium visualization, and a task-oriented assistant. The assistant selects tool recommendations through keyword rules. The Automatic workflow panel executes the TIFF-to-simulation-to-rendering sequence after users select terrain and breach inputs. Intermediate files and results are shared with the existing panels.

## Environment

- Use Windows for the complete desktop interface and C++ build. Folder selectors use Windows dialogs.
- Install Node.js 22.12 or later on a compatible supported release line, with npm.
- Install Python 3 with Tk support and the packages in `backend/requirements.txt`.
- For the solver, install Visual Studio 2022 or its Build Tools with the C++ desktop workload, MSVC v143, and a Windows SDK.
- Cesium ion imagery and world terrain use your own client-side ion token. The synthetic mesh test and preprocessing example run independently of map credentials.

## Automatic 100-frame workflow

Build the [C++ solver](#build-the-c-solver), install the Python and JavaScript dependencies, and start the Python API and Vite in separate terminals.

1. Open **Automatic workflow** in the sidebar.
2. Select [`FFF.tiff`](examples/data/FFF.tiff) and [`index (2).txt`](examples/data/index%20%282%29.txt).
3. Click **Run 100-frame workflow**.

The application automatically converts the TIFF to `dem.asc`, generates `cao.asc` and `InputRiver.dat`, and identifies the TIFF's UTM zone and hemisphere. It then launches the solver, stops the calculation after 100 output frames, converts the 100 JSON frames to meshes, and opens **Visualization** with animation playing.

For the bundled inputs, expect **WGS 84 / UTM 49N**, **755 × 518** cells, **30 breach cells**, **100 numbered JSON files**, and **100 mesh files with an index**. Outputs remain in `flood-work/workflows/<job-id>/`. Generated inputs have download links in the workflow panel. Use **Cancel workflow** to stop a running job, or **Show result** to reopen a completed result. The panel resumes status checks after reloading the page.

Each run has its own inputs, output folders, status file, and child process. The solver uses the existing dam settings in `runWorkflow`, with 80 calculation steps per frame. The frame limit specifies a bounded simulation segment. Physical parameter calibration and reproduction of the paper's experimental results require the authors' case-specific settings.

The Python API launches the built solver directly for each run. Configure `FLOOD_SOLVER_PATH` if using another executable location, and `FLOOD_NODE_PATH` if Node.js is outside the server's PATH.

To execute the same 100-frame example from the repository root through the existing application handlers:

```powershell
python tests/workflow_example.py
```

Expected final message:

```text
PASS: UTM 49N, 30 breach cells, 100 JSON frames and 100 mesh frames.
```

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

After installing the Python requirements, run the workflow validation and cancellation checks:

```powershell
python -m unittest discover -s tests -p test_workflow.py
```

Expected summary is **6 tests, OK**.

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

The Automatic workflow uses the bounded `--workflow` solver process to produce standard JSON and automatically convert and display meshes. The legacy manual HTTP simulation interface still returns plain text and has a separate frontend response mismatch. Use the Automatic workflow panel for the integrated example.

## Task-driven Multi-agent Collaboration and Service Orchestration

### Request interpretation and roles

`src/components/AgentAssistant.vue` contains `toolRules` and `buildAnswer`. It lowercases the request, scores keyword matches, and selects the highest-scoring rule. The selected object contains an agent label, a reply, and tool IDs used for navigation. Users supply file inputs and parameters through the relevant panels.

The role names and responsibilities follow manuscript sections **2.2.2**, **2.4.2**, and **3.1**. The five definitions are shared by the interface and chat-provider instructions through `src/js/agentRoles.js`.

| Role | Responsibility in the manuscript |
| --- | --- |
| Requirement Analyst | Parses user requirements and constraints and generates standardized task instructions. |
| Task Creator | Decomposes instructions into task nodes and initializes parameters. |
| Decision Maker | Coordinates collaboration, matches atomic services, and dispatches tasks according to constraints and node states. |
| Task Executor | Invokes atomic services for preprocessing, numerical simulation, visualization, and analysis. |
| Organizer | Reports task counts, participating agents, and execution order to the Decision Maker, then validates and integrates the results. |

The assistant labels input interpretation and TIFF validation as **Requirement Analyst**, and tool-selection advice as **Decision Maker**. Optional remote replies retain the selected paper role, while the provider badge identifies the chat source. The roster presents all five role definitions.

The automatic service workflow executes the fixed dependencies in `backend/workflow.py`. AutoGen-based role instances, inter-agent message queues, and requirement-derived planning require the corresponding method implementation.

### Communication and execution

Vue events and HTTP requests carry the current workflow. `navigate` opens a tool panel. `tif-file` routes validated TIFF input into preprocessing. `pipeline-file` passes the DEM, roughness, and breach files to shared state in `src/App.vue`. Once all three exist, `handlePipelineFile` opens Flood Simulation with those files filled in.

The optional provider in `server/agentApi.js` generates advisory text. `src/js/agentClient.js` retains rule-based tool suggestions and handles fallback. The Automatic workflow panel initiates and monitors the full service sequence through `/workflow/jobs`. Individual panels also provide buttons for each operation.

| Operation | Source and entry point | Result or dependency |
| --- | --- | --- |
| Automatic execution | `WorkflowPanel.vue`; `backend/workflow.py`; `backend/workflow_routes.py` | Isolated 100-frame run with status, cancellation, and automatic display |
| Request-to-tool recommendation | `AgentAssistant.vue`, `buildAnswer` | Rule-selected role, reply, tool IDs |
| TIFF routing and file sharing | `src/App.vue`, `handleAgentTifFile`, `handlePipelineFile` | Panel navigation and shared input files |
| DEM conversion | `backend/app.py`, `POST /convert` | ASC DEM from a TIFF |
| Roughness calculation | `backend/app.py`, `POST /roughness` | Roughness ASC from DEM and TIFF reference |
| Breach indexing | `backend/app.py`, `POST /get_ascii_values` | DAT from DEM, TIFF, and breach coordinates |
| Simulation request | `Caculate.vue`, `runFloodSimulation`; `vite.config.js` proxy | DEM, roughness, breach DAT, and UTM zone |
| Flood calculation | `solver/FloodTest/FloodTest.cpp`; `CAFloodNEW.cpp` | Water-depth TXT frames |
| Mesh generation | `meshConvert.vue`; `vite.config.js`, `/api/build-mesh`; `src/build-mesh-cache.cjs` | Mesh frames and index from JSON frames |
| Visualization | `Render.vue`, `/api/mesh-index`, `/api/mesh-frame` | Frame selection and animation |

See the [automatic workflow example](docs/task-driven-workflow.md) for the two input files, the service execution sequence, expected outputs, and implementation details.

## Verification scope

Verified on 2026-11-19 with the bundled TIFF and breach TXT through the application handlers and the browser interface. The run detected UTM 49N, generated matching 755-column by 518-row inputs and 30 breach cells, and completed 100 JSON and 100 mesh frames. The browser automatically loaded the meshes, displayed frame 100, and resumed playback. The 11 Node tests and 6 workflow validation and cancellation tests passed, and the frontend and Release x64 solver builds succeeded. The solver build retains warnings in legacy variants and console text.

The automated checks cover the 11 Node tests, synthetic mesh generation, and the preprocessing handlers with the bundled inputs. C++ compilation and a production frontend build can be checked using the commands above and `npm run build`. UI walkthroughs describe the controls present in the source. The full TIFF-to-100-frame workflow was executed with the bundled inputs and generated 100 JSON and mesh frames. Paper experiment reproduction and the correspondence to the proposed multi-agent method require additional author validation.

The repository currently has no project-level open-source license. License selection remains with the authors.

## Computer Code Availability

The source code and research prototype associated with this study are publicly available in the [GitHub repository](https://github.com/U87Y87/3d-flood-visual-simulation).

The repository provides implementations of flood-related geospatial data preprocessing, numerical simulation, mesh generation, and three-dimensional visualization, together with the available task-oriented assistant and service interfaces. It also includes example datasets, test scripts, and documentation for installing dependencies, configuring the execution environment, and running the provided examples.

The source code can be accessed and downloaded anonymously without registration. Instructions for reproducing the available computational workflows are provided in this README.

## Source layout

- `src/`, `js/`, and `server/` contain visualization, mesh conversion, and assistant interfaces.
- `backend/` contains Python GIS preprocessing.
- `solver/` contains the C++ solver and Visual Studio solution.
- `examples/data/` contains the DEM and breach-line inputs.
- `examples/frames/` contains synthetic water-depth frames.
- `tests/` contains executable tests, and `docs/` contains configuration and workflow instructions.
