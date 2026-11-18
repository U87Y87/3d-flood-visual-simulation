"""Verify the existing preprocessing endpoints using the bundled example inputs."""

import io
import json
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "backend"))

from app import app


def download_result(client, route, data):
    response = client.post(route, data=data)
    result = response.get_json()
    if response.status_code != 200 or not result or not result.get("success"):
        raise RuntimeError(f"{route} failed: {result}")
    download = client.get(result["download_url"])
    if download.status_code != 200:
        raise RuntimeError(f"Could not download the result of {route}")
    return result, download.data


def read_header(data):
    fields = (line.split() for line in data.splitlines()[:6])
    return {name.decode("ascii"): float(value) for name, value in fields}


def main():
    text = (ROOT / "examples/data/index (2).txt").read_text(encoding="utf-8-sig")
    number = r"([-+]?[0-9]+(?:\.[0-9]+)?)"
    pairs = re.findall(
        rf"^[ \t]*lon:[ \t]*{number}[ \t]*\n"
        rf"[ \t]*lat:[ \t]*{number}[ \t]*$",
        text,
        re.MULTILINE | re.IGNORECASE,
    )
    if len(pairs) != 14:
        raise RuntimeError("Expected 14 coordinate pairs in the bundled breach file")
    coordinates = [{"lon": float(lon), "lat": float(lat)} for lon, lat in pairs]
    tif = (ROOT / "examples/data/FFF.tiff").read_bytes()

    with tempfile.TemporaryDirectory() as temporary:
        uploads = Path(temporary) / "uploads"
        outputs = Path(temporary) / "outputs"
        uploads.mkdir()
        outputs.mkdir()
        app.config.update(
            TESTING=True,
            UPLOAD_FOLDER=str(uploads),
            OUTPUT_FOLDER=str(outputs),
        )
        client = app.test_client()
        dem, dem_data = download_result(client, "/convert", {
            "file": (io.BytesIO(tif), "FFF.tiff"),
        })
        roughness, roughness_data = download_result(client, "/roughness", {
            "file": (io.BytesIO(dem_data), dem["output_filename"]),
            "tif_file": (io.BytesIO(tif), "FFF.tiff"),
            "show_bounds": "false",
        })
        _, breach_data = download_result(client, "/get_ascii_values", {
            "ascii_file": (io.BytesIO(dem_data), dem["output_filename"]),
            "tif_file": (io.BytesIO(tif), "FFF.tiff"),
            "coordinates": json.dumps(coordinates),
        })

        header = read_header(dem_data)
        if header != read_header(roughness_data):
            raise RuntimeError("DEM and roughness headers differ")
        if (header["ncols"], header["nrows"]) != (755, 518):
            raise RuntimeError("Unexpected grid dimensions")
        if dem["output_filename"] != "FFF.asc" or roughness["roughness_file"] != "roughness_FFF.asc":
            raise RuntimeError("Unexpected preprocessing output filenames")

        import numpy as np

        dem_grid = np.loadtxt(io.BytesIO(dem_data), skiprows=6)
        roughness_grid = np.loadtxt(io.BytesIO(roughness_data), skiprows=6)
        valid = roughness_grid != header["NODATA_value"]
        values = roughness_grid[valid]
        if roughness_grid.shape != (518, 755) or not values.size:
            raise RuntimeError("Roughness grid has no usable cells")
        if not np.isfinite(values).all() or values.min() < 0 or values.max() > 1:
            raise RuntimeError("Expected finite normalized slope-based roughness")

        count, *records = breach_data.splitlines()
        indices = [tuple(map(int, record.split())) for record in records]
        if int(count) != 30 or len(indices) != 30 or len(set(indices)) != 30:
            raise RuntimeError("Expected 30 unique breach cells matching the DAT header")
        for col, row in indices:
            if not (0 <= col < 755 and 0 <= row < 518):
                raise RuntimeError("Breach index is outside the DEM")
            value = dem_grid[517 - row, col]
            if value == header["NODATA_value"] or not np.isfinite(value):
                raise RuntimeError("Breach index points to an invalid DEM cell")

    print("PASS: FFF.asc and roughness_FFF.asc, 755 columns x 518 rows; 30 breach cells from 14 points.")


if __name__ == "__main__":
    main()
