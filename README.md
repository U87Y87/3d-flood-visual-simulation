# 3D flood visual dynamic simulation

Research software for **Accelerating 3D flood visual dynamic simulation: a task-driven approach to online chained service construction using multi-agent collaboration**.

The prototype connects terrain and roughness preprocessing, a cellular-automata flood solver, mesh generation, and Cesium-based flood visualization. It includes a rule-based assistant for selecting tools and an optional chat provider.

## Quick test

Install [Node.js](https://nodejs.org/) 22.12 or later on a supported release line. From the repository root, run:

```powershell
npm ci
node --test tests/floodRisk.test.js tests/agentApi.test.js
node src/build-mesh-cache.cjs ../examples/frames ../examples/generated
```

The tests cover flood-depth interpolation, building and road inundation, and agent fallback with mock responses. The last command converts two small synthetic water-depth frames and writes `examples/generated/index.json` and two `.mesh.json` files. These checks run without map credentials, the Python service, or a running flood solver.

### DEM, roughness, and breach-index preprocessing example

Use the bundled [DEM GeoTIFF](examples/data/FFF.tiff) and [breach-line TXT file](examples/data/index%20%282%29.txt) to test the existing **Data Processing** workflow. Start the [Python preprocessing API](#run-the-processing-services-on-windows) and [web interface](#run-the-visualization) in separate PowerShell windows, then open the URL printed by Vite.

1. In **Data Processing → Format Conversion**, select `examples/data/FFF.tiff` and click **Convert & download ASC**. The application downloads `FFF.asc` and stores both the ASC and its TIFF spatial reference for the following steps.
2. In **Roughness Calculation**, use the automatically filled ASC and TIFF inputs. Click **Calculate & download roughness** to download `roughness_FFF.asc`. Both ASCII grids should have **755 columns and 518 rows**.
3. In **ASC Grid Indices**, keep the same ASC and TIFF inputs. Select `examples/data/index (2).txt` and click **Export grid indices**. The application uses the 14 WGS 84 longitude/latitude pairs in the TXT to recalculate the breach indices for the TIFF grid. The downloaded `.dat` file should start with **30**, the number of valid grid cells, followed by one zero-based `column row` pair per line, counting columns from the west and rows from the south.

The bundled TXT provides a previously drawn breach line for this example. To create another input, use **Draw Breach → Export terrain profile (TXT)**.

## Run the visualization

```powershell
npm run dev -- --host 127.0.0.1
```

Open the local URL printed by Vite. The visualization starts with a plain globe. For Cesium ion imagery and world terrain, set `VITE_CESIUM_ION_TOKEN` in `.env.local` and restart Vite. The browser receives that token, so use a token intended for client-side use. For optional assistant configuration, see [the assistant documentation](docs/chatgpt-agent.md).

## Run the processing services on Windows

For the Python preprocessing API, use Python 3 with Tk support. From the repository root, enter the `backend` directory, install the dependencies in your virtual environment, and start the service:

```powershell
cd backend
python -m pip install -r requirements.txt
python app.py
```

The frontend connects to the preprocessing API at `http://localhost:5000`.

### Input coordinate reference system

Gridded spatial inputs for preprocessing must use **WGS 1984 (WGS 84)** as the geographic coordinate system underlying a **UTM projected coordinate system**. Choose the UTM zone and hemisphere for the study area. Reproject inputs to that WGS 84 / UTM coordinate system before processing, and use the same zone and hemisphere for data processed together. The included GeoTIFF uses WGS 84 / UTM zone 49N.

For the flood solver, install Visual Studio 2022 Build Tools with MSVC v143, a Windows SDK and the C++ desktop workload. In a Visual Studio Developer PowerShell, run these commands from the repository root:

```powershell
msbuild solver/FloodTest.sln /p:Configuration=Release /p:Platform=x64
.\solver\x64\Release\FloodTest.exe
```

The solver listens on `127.0.0.1:18080`. Its working files are written under `flood-work` in the directory from which it is launched. In the application, select a DEM and matching roughness grid in ESRI ASCII format, a breach/inflow DAT file compatible with the solver, and the UTM zone for the inputs. The visualization quick test uses synthetic frames. Reproducing a study case requires the corresponding terrain, roughness and inflow inputs.

## Source layout

- `src/`, `js/` and `server/` contain the visualization, mesh conversion and assistant interfaces.
- `backend/` contains Python GIS preprocessing.
- `solver/` contains the C++ solver and Visual Studio project.
- `examples/frames/` and `tests/` contain synthetic input and runnable tests.
