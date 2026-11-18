"""Regression checks for workflow input validation and cancellation."""

import io
import json
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "backend"))

import workflow
from app import app


class WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name)
        self.original_root = app.config.get("WORKFLOW_FOLDER")
        app.config["WORKFLOW_FOLDER"] = self.directory
        self.client = app.test_client()

    def tearDown(self):
        if self.original_root is None:
            app.config.pop("WORKFLOW_FOLDER", None)
        else:
            app.config["WORKFLOW_FOLDER"] = self.original_root
        self.temporary.cleanup()

    def test_breach_parser_uses_coordinate_fields(self):
        points = workflow.read_coordinates(ROOT / "examples/data/index (2).txt")
        self.assertEqual(len(points), 14)
        self.assertEqual(points[0], {"lon": 112.930085, "lat": 35.389460})

    def test_breach_point_count_must_match(self):
        path = self.directory / "bad.txt"
        path.write_text("Total points: 2\nlon: 112.9\nlat: 35.3\n", encoding="utf-8")
        with self.assertRaises(ValueError):
            workflow.read_coordinates(path)

    def test_invalid_upload_does_not_start_a_worker(self):
        with patch("workflow_routes.subprocess.Popen") as spawn:
            response = self.client.post("/workflow/jobs", data={
                "tif": (io.BytesIO(b"not a TIFF"), "input.zip"),
                "breach": (io.BytesIO(b"data"), "line.txt"),
            })
        self.assertEqual(response.status_code, 400)
        spawn.assert_not_called()
        self.assertEqual(list(self.directory.iterdir()), [])

    def test_spawn_failure_is_reported_in_job_status(self):
        with patch("workflow_routes.subprocess.Popen", side_effect=OSError("solver launch test")):
            response = self.client.post("/workflow/jobs", data={
                "tif": (io.BytesIO(b"TIFF"), "input.tiff"),
                "breach": (io.BytesIO(b"TXT"), "line.txt"),
            })
        self.assertEqual(response.status_code, 500)
        job = response.get_json()["id"]
        state = self.client.get("/workflow/jobs/" + job).get_json()
        self.assertEqual(state["state"], "failed")

    def test_cancellation_terminates_the_child(self):
        timer = threading.Timer(0.2, lambda: (self.directory / "cancel").touch())
        timer.start()
        try:
            with self.assertRaises(InterruptedError):
                workflow.run_command(
                    self.directory, [sys.executable, "-c", "import time; time.sleep(20)"],
                    "simulation", 100,
                )
        finally:
            timer.join()

    def test_job_output_paths_are_restricted(self):
        self.assertEqual(self.client.get("/workflow/jobs/invalid").status_code, 404)
        job = "a" * 32
        directory = self.directory / job
        directory.mkdir()
        (directory / "status.json").write_text(json.dumps({"state": "completed"}))
        self.assertEqual(self.client.get(f"/workflow/jobs/{job}/files/worker.log").status_code, 404)


if __name__ == "__main__":
    unittest.main()
