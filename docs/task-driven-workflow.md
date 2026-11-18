# Task-driven workflow example

## Task and environment

Use this request in **AI Assistant**:

> Prepare TIFF terrain data, calculate roughness and breach indices, and show me the tools for a flood workflow.

In default local mode, the keyword-scoring rule selects **Data Agent** and provides a **Data Processing** tool button. File selection and parameter entry take place in the panels. The request demonstrates the existing recommendation mechanism.

Start the Python API from `backend` with `python app.py`, and start Vite from the repository root with `npm run dev -- --host 127.0.0.1`. Use the [README](../README.md) to install dependencies first.

## Run the service sequence in the existing interface

1. In **AI Assistant → Validate & route data**, select `examples/data/FFF.tiff`. The assistant validates its extension and header, then opens **Data Processing → Format Conversion** with the file filled in.
2. Click **Convert & download ASC**. The `/convert` endpoint produces `FFF.asc`.
3. Open **Roughness Calculation** and click **Calculate & download roughness** using the shared DEM and reference TIFF. The `/roughness` endpoint produces `roughness_FFF.asc`.
4. Open **ASC Grid Indices**, select `examples/data/index (2).txt`, and click **Export grid indices**. The `/get_ascii_values` endpoint produces a DAT with 30 cells from the 14 coordinate points.
5. The `pipeline-file` events populate all three inputs in **Flood Simulation**. Set the UTM zone to **49** for this northern-hemisphere example.

The two grids have 755 columns and 518 rows. The DAT begins with 30, followed by zero-based column/row pairs measured from the west/south. Repeated coordinates in a cell are deduplicated by the backend sampling routine.

Run `python tests/preprocessing_example.py` from the repository root to verify the three preprocessing endpoints and their downloadable outputs independently of browser interaction.

## Continue with the synthetic rendering example

To exercise mesh generation and visualization, use the two bundled synthetic JSON frames:

1. Open **Grid Conversion** and select `examples/frames` as the source and `examples/generated` as the output directory. Click **Start conversion**.
2. Expect `0.mesh.json`, `1.mesh.json`, and `index.json`. Each frame contains 9 points and 8 triangles.
3. Open **Visualization → Choose mesh folder**, select `examples/generated`, and use **Zoom to flood**, **Next frame**, and **Play animation**.

The synthetic JSON inputs are provided separately from the TIFF/breach preprocessing outputs. Their coordinates describe a separate illustrative scene. This section validates the mesh-to-rendering portion of the workflow.

## Integration work for a full simulated chain

The solver exposes `POST /upload-all`, `POST /stop`, and `GET /download`. The frontend simulation panel expects JSON containing `txtZipUrl`, `jsonZipUrl`, and an optional `restartUrl`, and uses proxy routes with corresponding download names. Aligning these interfaces is required before the panel can complete its download sequence.

The active solver function exports numbered TXT water-depth grids. The mesh converter consumes JSON frames. The included `backend/grid_to_tin.py` provides a separate conversion component, and integration of this stage into the solver completion flow remains pending.

The interface includes four named agent roles. The inspected implementation selects recommendations through keyword scores, shares files through Vue events, and invokes HTTP services through user actions. A structured natural-language task schema, inter-agent messaging, dependency-based service planning, and automatic execution remain pending.

## Verification and publication

Automated checks exercise the preprocessing outputs and synthetic mesh outputs separately. Full simulation-to-visualization execution and reproduction of the manuscript's multi-agent experiments remain pending. The authors should relate the manuscript's method claims to the corresponding executable implementation before resubmission.

The README includes a **Computer Code Availability** paragraph for the manuscript. The manuscript file should receive that section through the authors' normal editing process.
