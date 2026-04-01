# RCPSP Web Runner (Node.js)

Local web app that lets you upload:
- a **problem description PDF** (optional), and
- an **`.rcp` instance file** (required),

then runs the C++ solver and shows the solver output in the browser.

## 1) Build the C++ solver (once)

Open `retake_final.sln` in Visual Studio and build `Debug x64` (or `Release x64`).

The server auto-detects these common output paths:
- `x64\\Debug\\retake_final.exe`
- `x64\\Release\\retake_final.exe`
- `retake_final\\x64\\Debug\\retake_final.exe`
- `retake_final\\x64\\Release\\retake_final.exe`

If your exe is elsewhere, set:

```powershell
$env:SOLVER_EXE="C:\full\path\to\retake_final.exe"
```

## 2) Install & run

From `web-node/`:

```powershell
npm install
npm run dev
```

Open `http://127.0.0.1:3000`.

