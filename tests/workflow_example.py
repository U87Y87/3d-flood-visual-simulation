"""Run the real 100-frame workflow through the application's HTTP handlers."""

import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "backend"))
from app import app


def main():
    client = app.test_client()
    with (ROOT / "examples/data/FFF.tiff").open("rb") as tif:
        with (ROOT / "examples/data/index (2).txt").open("rb") as breach:
            response = client.post("/workflow/jobs", data={
                "tif": (tif, "FFF.tiff"), "breach": (breach, "index.txt"),
            })
    if response.status_code != 202:
        raise RuntimeError(response.get_json())
    job = response.get_json()["id"]
    print(f"Workflow {job}", flush=True)
    previous_stage = None
    deadline = time.monotonic() + 1800
    while time.monotonic() < deadline:
        state = client.get(f"/workflow/jobs/{job}").get_json()
        if state["stage"] != previous_stage:
            print(state["stage"], state["message"], flush=True)
            previous_stage = state["stage"]
        if state["state"] in ("failed", "cancelled"):
            raise RuntimeError(state["message"])
        if state["state"] == "completed":
            break
        time.sleep(1)
    else:
        client.post(f"/workflow/jobs/{job}/cancel")
        raise TimeoutError("The example exceeded 30 minutes and cancellation was requested.")

    json_folder = Path(state["jsonFolder"])
    mesh_folder = Path(state["meshFolder"])
    index = json.loads((mesh_folder / "index.json").read_text(encoding="utf-8"))
    if len(list(json_folder.glob("*.json"))) != 100 or len(index["frames"]) != 100:
        raise RuntimeError("Expected exactly 100 JSON frames and 100 indexed mesh frames.")
    if len(list(mesh_folder.glob("*.mesh.json"))) != 100:
        raise RuntimeError("Expected exactly 100 mesh files.")
    if (state["utmZone"], state["hemisphere"], state["breachCells"]) != (49, "N", 30):
        raise RuntimeError("Unexpected example projection or breach indices.")
    for frame in range(1, 101):
        data = json.loads((json_folder / f"{frame}.json").read_text(encoding="utf-8"))
        if any(not (112 < point["x"] < 114 and 34 < point["y"] < 37) for point in data["points"]):
            raise RuntimeError(f"Frame {frame} is outside the expected study area.")
    print("PASS: UTM 49N, 30 breach cells, 100 JSON frames and 100 mesh frames.")
    print(f"Mesh folder: {mesh_folder}")


if __name__ == "__main__":
    main()
