"""HTTP entry points for isolated workflow runs."""

import json
import os
import re
import subprocess
import sys
import time
import uuid
from pathlib import Path

from flask import Blueprint, current_app, jsonify, request, send_file

workflow_api = Blueprint("workflow", __name__)
ROOT = Path(__file__).resolve().parents[1]


def workflow_root():
    return Path(current_app.config.get("WORKFLOW_FOLDER", ROOT / "flood-work/workflows")).resolve()


def job_directory(job_id):
    if not re.fullmatch(r"[0-9a-f]{32}", job_id):
        return None
    directory = workflow_root() / job_id
    return directory if (directory / "status.json").is_file() else None


@workflow_api.post("/workflow/jobs")
def start_workflow():
    tif = request.files.get("tif")
    breach = request.files.get("breach")
    if not tif or not tif.filename or Path(tif.filename).suffix.lower() not in (".tif", ".tiff"):
        return jsonify(error="Select a TIFF terrain file."), 400
    if not breach or not breach.filename or Path(breach.filename).suffix.lower() != ".txt":
        return jsonify(error="Select a breach TXT file."), 400
    job_id = uuid.uuid4().hex
    directory = workflow_root() / job_id
    inputs = directory / "inputs"
    inputs.mkdir(parents=True)
    tif.save(inputs / "terrain.tiff")
    breach.save(inputs / "breach.txt")
    state = {
        "id": job_id, "state": "queued", "stage": "dem",
        "frameCount": 100, "completedFrames": 0,
        "message": "Preparing workflow.", "createdAt": time.time(),
    }
    status = directory / "status.json"
    status.write_text(json.dumps(state), encoding="utf-8")
    try:
        with (directory / "worker.log").open("w", encoding="utf-8") as log:
            subprocess.Popen(
                [sys.executable, "-u", str(ROOT / "backend/workflow.py"), str(directory)],
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                env={**os.environ, "PYTHONIOENCODING": "utf-8"},
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
            )
    except OSError as error:
        state.update(state="failed", message=str(error))
        status.write_text(json.dumps(state), encoding="utf-8")
        return jsonify(error=str(error), id=job_id), 500
    return jsonify(state), 202


@workflow_api.get("/workflow/jobs/<job_id>")
def workflow_status(job_id):
    directory = job_directory(job_id)
    if directory is None:
        return jsonify(error="Workflow was not found."), 404
    state = json.loads((directory / "status.json").read_text(encoding="utf-8"))
    state["artifacts"] = {
        name: f"/api/workflow/jobs/{job_id}/files/{name}"
        for name, relative in {
            "dem.asc": "inputs/dem.asc",
            "cao.asc": "inputs/cao.asc",
            "InputRiver.dat": "inputs/InputRiver.dat",
        }.items() if (directory / relative).is_file()
    }
    return jsonify(state)


@workflow_api.post("/workflow/jobs/<job_id>/cancel")
def cancel_workflow(job_id):
    directory = job_directory(job_id)
    if directory is None:
        return jsonify(error="Workflow was not found."), 404
    state = json.loads((directory / "status.json").read_text(encoding="utf-8"))
    if state["state"] in ("queued", "running"):
        (directory / "cancel").touch()
    return jsonify(success=True)


@workflow_api.get("/workflow/jobs/<job_id>/files/<name>")
def workflow_file(job_id, name):
    directory = job_directory(job_id)
    allowed = {"dem.asc", "cao.asc", "InputRiver.dat"}
    if directory is None or name not in allowed or not (directory / "inputs" / name).is_file():
        return jsonify(error="Output file was not found."), 404
    return send_file(directory / "inputs" / name, as_attachment=True)
