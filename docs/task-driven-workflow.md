# Automatic TIFF-to-visualization example

Start the Python API from `backend` with `python app.py`, and Vite from the repository root with `npm run dev -- --host 127.0.0.1`. Install dependencies and build `solver/FloodTest.sln` in Release x64 first, following the [README](../README.md).

## Run in the existing application

1. Open **Automatic workflow**.
2. Select `examples/data/FFF.tiff` as terrain and `examples/data/index (2).txt` as the breach line.
3. Click **Run 100-frame workflow**.
4. Wait for preprocessing, simulation, and mesh generation to finish. The application opens the generated meshes in **Visualization**, flies to the flood, and starts animation.

The original Data Processing, Flood Simulation, Grid Conversion, and Visualization panels remain available for individual operations. The automatic panel fills the generated simulation input files and UTM zone into shared application state.

## Service dependencies and outputs

| Stage | Inputs | Operation | Output |
| --- | --- | --- | --- |
| DEM | WGS 84 / UTM TIFF | Existing `dem_to_ascii_gdal` | `inputs/dem.asc` |
| Roughness | DEM | Existing slope-based roughness function | `inputs/cao.asc` |
| Breach | DEM, TIFF projection, TXT longitude/latitude pairs | Existing grid-index calculation | `inputs/InputRiver.dat` |
| Simulation | Three generated inputs, detected UTM zone and hemisphere | C++ cellular-automata solver, 80 steps per frame | `depth/1.txt` through `100.txt`, `json/1.json` through `100.json` |
| Mesh | 100 standard JSON frames | Existing `src/build-mesh-cache.cjs` | 100 `.mesh.json` files and `mesh/index.json` |
| Display | Mesh folder returned in job status | Existing Cesium loader and playback | Animated flood geometry in the application |

For the bundled input pair, verify 755 columns, 518 rows, UTM 49N, 30 indexed breach cells, and exactly 100 JSON and mesh frames. Downloadable generated inputs are linked in the workflow panel. The output folder is `flood-work/workflows/<job-id>/`.

The solver exits after generating frame 100. The Python worker then verifies JSON outputs, launches mesh conversion, verifies the mesh index, and records completion. Failures and cancellation stop the current calculation and leave a status message and logs in the run directory.

## Implementation

- `backend/workflow_routes.py` accepts input uploads, starts an isolated worker, serves status and generated inputs, and handles cancellation.
- `backend/workflow.py` validates inputs, derives the WGS 84 / UTM zone and hemisphere, executes service dependencies, and checks outputs.
- `solver/FloodTest/FloodTest.cpp` provides `--workflow directory frames zone N|S` and the bounded solver loop.
- `CAFloodNEW.cpp` applies the supplied UTM zone and zero-based south-origin breach indexing and exports standard JSON.
- `WorkflowPanel.vue` submits the two input files and polls job progress.
- `App.vue` receives shared input files and the completed mesh folder, then calls the existing `Render.vue` loader and playback.

The manuscript defines five roles: **Requirement Analyst**, **Task Creator**, **Decision Maker**, **Task Executor**, and **Organizer**. Their responsibilities are listed in the [README role table](../README.md#request-interpretation-and-roles). The interface and optional chat-provider instructions share these definitions in `src/js/agentRoles.js`. Input validation uses the Requirement Analyst label, and tool recommendations use the Decision Maker label. The scheduler follows a fixed dependency sequence. AutoGen-based collaboration and request-derived planning require the corresponding method implementation.

## Terminal example

From the repository root:

```powershell
python tests/workflow_example.py
```

This executes the application's job handlers and the actual solver and mesh converter. It verifies frame counts and output coordinates and prints the generated mesh folder.

The smaller [preprocessing example](../tests/preprocessing_example.py) and synthetic [mesh frames](../examples/frames) remain available for checking individual services.

## Simulation settings

The automatic solver retains the existing configured dam volume of 200,000,000, dam width 30, dam height 3, breach width 30, and breach height 3. It retains 80 calculation steps per frame. These settings define the provided software demonstration and require domain validation for each research case.

The automatic workflow uses the process entry point `--workflow`, bypassing the legacy HTTP upload/download response mismatch. The legacy manual simulation HTTP interface retains its existing response contract.
