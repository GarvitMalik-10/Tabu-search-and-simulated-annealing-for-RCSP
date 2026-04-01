import os
import subprocess
import tempfile
from pathlib import Path

from fastapi import FastAPI, File, Form, UploadFile
from fastapi.responses import HTMLResponse


APP_DIR = Path(__file__).resolve().parent
REPO_ROOT = APP_DIR.parent

# You can override this with an env var if you want to point to a prebuilt exe.
# Example: set SOLVER_EXE=C:\path\to\retake_final\x64\Debug\retake_final.exe
DEFAULT_SOLVER_EXE_CANDIDATES = [
    REPO_ROOT / "x64" / "Debug" / "retake_final.exe",
    REPO_ROOT / "x64" / "Release" / "retake_final.exe",
    REPO_ROOT / "retake_final" / "x64" / "Debug" / "retake_final.exe",
    REPO_ROOT / "retake_final" / "x64" / "Release" / "retake_final.exe",
]


def _pick_solver_exe() -> Path:
    env = os.environ.get("SOLVER_EXE")
    if env:
        p = Path(env)
        if not p.exists():
            raise RuntimeError(f"SOLVER_EXE is set but does not exist: {p}")
        return p
    for p in DEFAULT_SOLVER_EXE_CANDIDATES:
        if p.exists():
            return p
    raise RuntimeError(
        "Solver exe not found. Build the Visual Studio project, or set SOLVER_EXE to the executable path.\n"
        f"Tried:\n- " + "\n- ".join(str(p) for p in DEFAULT_SOLVER_EXE_CANDIDATES)
    )


app = FastAPI()


@app.get("/", response_class=HTMLResponse)
def index() -> str:
    return """
<!doctype html>
<html>
  <head>
    <meta charset="utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1"/>
    <title>RCPSP Solver</title>
    <style>
      body { font-family: ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Arial; margin: 2rem; }
      .card { max-width: 880px; padding: 1.25rem; border: 1px solid #ddd; border-radius: 12px; }
      label { display: block; margin: 0.75rem 0 0.25rem; font-weight: 600; }
      input, select { width: 100%; padding: 0.6rem; border: 1px solid #ccc; border-radius: 10px; }
      button { margin-top: 1rem; padding: 0.7rem 1rem; border: 0; border-radius: 10px; background: #111827; color: white; cursor: pointer; }
      pre { white-space: pre-wrap; background: #0b1020; color: #e5e7eb; padding: 1rem; border-radius: 12px; overflow: auto; }
      .muted { color: #6b7280; font-size: 0.95rem; }
    </style>
  </head>
  <body>
    <div class="card">
      <h2 style="margin:0 0 0.25rem 0;">RCPSP Solver (GarvitRetake)</h2>
      <div class="muted">Upload the <code>.rcp</code> instance file that the C++ solver reads. PDF is optional and stored only for your reference.</div>

      <form action="/solve" method="post" enctype="multipart/form-data">
        <label>Problem description PDF (optional)</label>
        <input type="file" name="problem_pdf" accept="application/pdf"/>

        <label>Instance file (.rcp) (required)</label>
        <input type="file" name="instance_rcp" accept=".rcp" required/>

        <label>Allowed computation time (seconds)</label>
        <input type="number" name="allowed_time" value="5" min="0.1" step="0.1" required/>

        <label>Method</label>
        <select name="method">
          <option value="TS">TS (Tabu Search)</option>
          <option value="SA">SA (Simulated Annealing)</option>
          <option value="A">A (TS then SA)</option>
        </select>

        <button type="submit">Run solver</button>
      </form>
    </div>
  </body>
</html>
""".strip()


@app.post("/solve", response_class=HTMLResponse)
async def solve(
    instance_rcp: UploadFile = File(...),
    problem_pdf: UploadFile | None = File(default=None),
    allowed_time: float = Form(...),
    method: str = Form(...),
) -> str:
    method = (method or "").strip().upper()
    if method not in {"TS", "SA", "A"}:
        return f"<pre>Invalid method: {method}. Use TS, SA, or A.</pre>"
    if allowed_time <= 0:
        return "<pre>allowed_time must be > 0</pre>"

    try:
        solver_exe = _pick_solver_exe()
    except Exception as e:
        return f"<pre>{e}</pre>"

    with tempfile.TemporaryDirectory(prefix="rcpsp_") as td:
        td_path = Path(td)
        rcp_path = td_path / (Path(instance_rcp.filename or "instance").stem + ".rcp")
        rcp_path.write_bytes(await instance_rcp.read())

        if problem_pdf is not None and (problem_pdf.filename or "").lower().endswith(".pdf"):
            pdf_path = td_path / (Path(problem_pdf.filename).stem + ".pdf")
            pdf_path.write_bytes(await problem_pdf.read())

        cmd = [
            str(solver_exe),
            "--rcp",
            str(rcp_path),
            "--time",
            str(allowed_time),
            "--method",
            method,
        ]

        try:
            # Add a small cushion over allowed_time for process startup and printing.
            completed = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=max(allowed_time + 2.0, allowed_time * 1.25),
                cwd=str(REPO_ROOT),
            )
            out = (completed.stdout or "") + ("\n" + completed.stderr if completed.stderr else "")
            exit_code = completed.returncode
        except subprocess.TimeoutExpired:
            out = "Process timed out."
            exit_code = -1

    safe = (
        out.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
    )
    return f"""
<!doctype html>
<html>
  <head>
    <meta charset="utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1"/>
    <title>RCPSP Solver - Result</title>
    <style>
      body {{ font-family: ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Arial; margin: 2rem; }}
      a {{ color: #2563eb; text-decoration: none; }}
      pre {{ white-space: pre-wrap; background: #0b1020; color: #e5e7eb; padding: 1rem; border-radius: 12px; overflow: auto; }}
      .meta {{ color: #6b7280; margin-bottom: 0.75rem; }}
    </style>
  </head>
  <body>
    <div><a href="/">← Run another</a></div>
    <h2 style="margin-bottom:0.25rem;">Result</h2>
    <div class="meta">Exit code: {exit_code} | Method: {method} | Time: {allowed_time}</div>
    <pre>{safe}</pre>
  </body>
</html>
""".strip()

