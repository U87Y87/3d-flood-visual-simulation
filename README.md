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

### GeoTIFF preprocessing test

Use [`examples/data/FFF.tiff`](examples/data/FFF.tiff) to test the DEM-to-ASCII conversion. Install the Python dependencies, then run the following commands from the repository root:

```powershell
python -m pip install -r backend/requirements.txt
python -c "import sys; from pathlib import Path; sys.path.insert(0, 'backend'); from TIFF转ASC格式 import dem_to_ascii_gdal; p = Path('examples/generated/FFF.asc'); p.parent.mkdir(parents=True, exist_ok=True); print(dem_to_ascii_gdal('examples/data/FFF.tiff', str(p)))"
```

The command writes `examples/generated/FFF.asc`. This test covers DEM preprocessing. Running a flood simulation also requires a matching roughness grid and a breach/inflow file.

## Run the visualization

```powershell
npm run dev -- --host 127.0.0.1
```

Open the local URL printed by Vite. The visualization starts with a plain globe. For Cesium ion imagery and world terrain, set `VITE_CESIUM_ION_TOKEN` in `.env.local` and restart Vite. The browser receives that token, so use a token intended for client-side use. For optional assistant configuration, see [the assistant documentation](docs/chatgpt-agent.md).

## Run the processing services on Windows

For the Python preprocessing API, use Python 3 with Tk support and install the dependencies in a virtual environment:

```powershell
python -m pip install -r backend/requirements.txt
python -m flask --app backend/app.py run --host 127.0.0.1 --port 5000
```

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
