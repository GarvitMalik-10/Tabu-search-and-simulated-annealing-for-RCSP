# RCPSP Web Runner (Option A)

This is a small local web app that lets you:
- Upload a problem description **PDF** (optional, stored only temporarily)
- Upload an **`.rcp` instance file** (required)
- Choose `TS`, `SA`, or `A`
- Run the existing C++ solver and view its output in the browser

## 1) Build the C++ solver (once)

Open `retake_final.sln` in Visual Studio and build `Debug x64` (or `Release x64`).

The web app auto-detects these common output paths:
- `x64\\Debug\\retake_final.exe`
- `x64\\Release\\retake_final.exe`
- `retake_final\\x64\\Debug\\retake_final.exe`
- `retake_final\\x64\\Release\\retake_final.exe`

If your exe is elsewhere, set an env var:

```powershell
$env:SOLVER_EXE="C:\full\path\to\retake_final.exe"
```

## 2) Run the web app

From `web/`:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
uvicorn main:app --reload
```

Then open `http://127.0.0.1:8000`.

