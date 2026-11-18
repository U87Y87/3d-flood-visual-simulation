"""Execute the TIFF-to-rendering service workflow in an isolated process."""

import argparse
import json
import os
import re
import shutil
import subprocess
import time
from pathlib import Path

import numpy as np
import rasterio
from pyproj import CRS, Transformer

from ASC文件获取粗糙度 import ascii_to_roughness_slope_based, get_ascii_values_by_coordinates
from TIFF转ASC格式 import dem_to_ascii_gdal

ROOT = Path(__file__).resolve().parents[1]


def save_status(directory, **changes):
    path = directory / "status.json"
    state = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
    state.update(changes, updatedAt=time.time())
    temporary = directory / "status.tmp"
    temporary.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding="utf-8")
    temporary.replace(path)


def check_cancelled(directory):
    if (directory / "cancel").exists():
        raise InterruptedError("Workflow cancelled")


def read_coordinates(path):
    text = path.read_text(encoding="utf-8-sig")
    number = r"([-+]?[0-9]+(?:\.[0-9]+)?)"
    pairs = re.findall(
        rf"^[ \t]*(?:lon|longitude)\s*[:=]\s*{number}[ \t]*\n"
        rf"[ \t]*(?:lat|latitude)\s*[:=]\s*{number}[ \t]*$",
        text, re.MULTILINE | re.IGNORECASE,
    )
    if not pairs:
        raise ValueError("The breach TXT must contain paired lon: and lat: lines.")
    declared = re.search(r"Total points:\s*(\d+)", text, re.IGNORECASE)
    if declared and int(declared[1]) != len(pairs):
        raise ValueError("The breach TXT point count does not match its coordinates.")
    coordinates = [{"lon": float(lon), "lat": float(lat)} for lon, lat in pairs]
    if any(not (-180 <= p["lon"] <= 180 and -90 <= p["lat"] <= 90) for p in coordinates):
        raise ValueError("Breach coordinates must be WGS 84 longitude and latitude.")
    return coordinates


def prepare_inputs(directory):
    inputs = directory / "inputs"
    tif = inputs / "terrain.tiff"
    coordinates = read_coordinates(inputs / "breach.txt")
    with rasterio.open(tif) as raster:
        crs = CRS.from_wkt(raster.crs.to_wkt()) if raster.crs else None
        epsg = crs.to_epsg() if crs else None
        if epsg is None or not (32601 <= epsg <= 32660 or 32701 <= epsg <= 32760):
            raise ValueError("The TIFF must use WGS 84 / UTM with a defined zone and hemisphere.")
        transform = raster.transform
        if transform.b != 0 or transform.d != 0 or transform.a <= 0 or transform.e >= 0:
            raise ValueError("The TIFF must be a north-up, unrotated raster.")
        if not np.isclose(raster.res[0], raster.res[1], rtol=1e-6):
            raise ValueError("The DEM must have square cells.")
        converter = Transformer.from_crs("EPSG:4326", crs, always_xy=True)
        elevations = raster.read(1, masked=True)
        for point in coordinates:
            row, col = raster.index(*converter.transform(point["lon"], point["lat"]))
            if not (0 < row < raster.height - 1 and 0 < col < raster.width - 1):
                raise ValueError("A breach point lies outside the interior of this TIFF.")
            if np.ma.is_masked(elevations[row, col]) or not np.isfinite(elevations[row, col]):
                raise ValueError("A breach point lies on a NoData cell.")
        width, height = raster.width, raster.height

    dem = inputs / "dem.asc"
    roughness = inputs / "cao.asc"
    breach = inputs / "InputRiver.dat"
    dem_to_ascii_gdal(tif, dem)
    check_cancelled(directory)
    save_status(directory, stage="roughness", message="Calculating roughness.")
    ascii_to_roughness_slope_based(dem, roughness)
    check_cancelled(directory)
    save_status(directory, stage="breach", message="Calculating breach grid indices.")
    if not get_ascii_values_by_coordinates(dem, coordinates, breach, tif_path=tif):
        raise RuntimeError("Breach grid indexing failed.")
    count, *lines = breach.read_text(encoding="utf-8").splitlines()
    cells = [tuple(map(int, line.split())) for line in lines]
    dem_grid = np.loadtxt(dem, skiprows=6)
    if not cells or int(count) != len(cells) or len(set(cells)) != len(cells):
        raise ValueError("The breach line produced invalid or empty grid indices.")
    for col, row in cells:
        if not (0 < col < width - 1 and 0 < row < height - 1):
            raise ValueError("A sampled breach cell lies outside the DEM interior.")
        value = dem_grid[height - 1 - row, col]
        if not np.isfinite(value) or value == 9999:
            raise ValueError("A sampled breach cell lies on NoData.")
    roughness_grid = np.loadtxt(roughness, skiprows=6)
    valid = roughness_grid != 9999
    if not valid.any() or not np.isfinite(roughness_grid[valid]).all():
        raise ValueError("The roughness grid contains invalid values.")
    return {
        "epsg": epsg, "utmZone": epsg % 100,
        "hemisphere": "N" if epsg < 32700 else "S",
        "columns": width, "rows": height, "breachCells": len(cells),
    }


def run_command(directory, command, stage, expected):
    check_cancelled(directory)
    log_path = directory / (stage + ".log")
    environment = {**os.environ, "MESH_WORKERS": "4"}
    with log_path.open("w", encoding="utf-8") as log:
        process = subprocess.Popen(
            command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
            env=environment,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
        )
        try:
            while process.poll() is None:
                check_cancelled(directory)
                if stage == "simulation":
                    completed = len(list((directory / "depth").glob("*.txt")))
                else:
                    completed = len(list((directory / "mesh").glob("*.mesh.json")))
                save_status(directory, stage=stage, completedFrames=min(completed, expected))
                time.sleep(0.5)
        except BaseException:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            raise
        if process.returncode != 0:
            detail = log_path.read_text(encoding="utf-8", errors="replace")[-1500:]
            raise RuntimeError(f"{stage} exited with code {process.returncode}. {detail}")


def run_workflow(directory, frames=100):
    directory = Path(directory).resolve()
    if not 1 <= frames <= 100:
        raise ValueError("Frame count must be between 1 and 100.")
    try:
        save_status(directory, state="running", stage="dem", frameCount=frames,
                    message="Converting TIFF to ASC.", completedFrames=0)
        metadata = prepare_inputs(directory)
        save_status(directory, **metadata)
        solver = Path(os.environ.get(
            "FLOOD_SOLVER_PATH", str(ROOT / "solver/x64/Release/FloodTest.exe")
        )).resolve()
        if not solver.is_file():
            raise FileNotFoundError("Build solver/FloodTest.sln in Release x64, or set FLOOD_SOLVER_PATH.")
        save_status(directory, stage="simulation", message=f"Simulating {frames} frames.", completedFrames=0)
        run_command(directory, [
            str(solver), "--workflow", str(directory), str(frames),
            str(metadata["utmZone"]), metadata["hemisphere"],
        ], "simulation", frames)
        for frame in range(1, frames + 1):
            check_cancelled(directory)
            path = directory / "json" / f"{frame}.json"
            data = json.loads(path.read_text(encoding="utf-8"))
            points = data.get("points")
            if not isinstance(points, list):
                raise RuntimeError(f"Invalid JSON frame {frame}.")
            if any(not all(np.isfinite(point[key]) for key in ("x", "y", "value")) for point in points):
                raise RuntimeError(f"Non-finite values in JSON frame {frame}.")
        save_status(directory, stage="mesh", message="Converting JSON frames to meshes.", completedFrames=0)
        node = os.environ.get("FLOOD_NODE_PATH") or shutil.which("node")
        if not node:
            raise FileNotFoundError("Node.js was not found. Install Node.js or set FLOOD_NODE_PATH.")
        run_command(directory, [
            node, str(ROOT / "src/build-mesh-cache.cjs"),
            str(directory / "json"), str(directory / "mesh"),
        ], "mesh", frames)
        index = json.loads((directory / "mesh/index.json").read_text(encoding="utf-8"))
        if len(index["frames"]) != frames:
            raise RuntimeError("Mesh conversion did not produce all requested frames.")
        for frame in index["frames"]:
            mesh = directory / "mesh" / Path(frame["name"]).name
            if not mesh.is_file():
                raise RuntimeError("A converted mesh file is missing.")
        check_cancelled(directory)
        save_status(directory, state="completed", stage="complete",
                    message="Ready for visualization.", completedFrames=frames,
                    meshFolder=str(directory / "mesh"), jsonFolder=str(directory / "json"))
    except InterruptedError as error:
        save_status(directory, state="cancelled", message=str(error))
    except Exception as error:
        save_status(directory, state="failed", message=str(error))
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("--frames", type=int, default=100)
    arguments = parser.parse_args()
    run_workflow(arguments.directory, arguments.frames)
