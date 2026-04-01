import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawn } from "node:child_process";

import express from "express";
import multer from "multer";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const REPO_ROOT = path.resolve(__dirname, "..");

const app = express();
const upload = multer({ storage: multer.memoryStorage() });

const DEFAULT_SOLVER_EXE_CANDIDATES = [
  path.join(REPO_ROOT, "x64", "Debug", "retake_final.exe"),
  path.join(REPO_ROOT, "x64", "Release", "retake_final.exe"),
  path.join(REPO_ROOT, "retake_final", "x64", "Debug", "retake_final.exe"),
  path.join(REPO_ROOT, "retake_final", "x64", "Release", "retake_final.exe"),
];

function pickSolverExe() {
  const env = process.env.SOLVER_EXE;
  if (env) {
    if (!fs.existsSync(env)) throw new Error(`SOLVER_EXE is set but does not exist: ${env}`);
    return env;
  }
  for (const p of DEFAULT_SOLVER_EXE_CANDIDATES) {
    if (fs.existsSync(p)) return p;
  }
  throw new Error(
    "Solver exe not found. Build the Visual Studio project, or set SOLVER_EXE.\n" +
      "Tried:\n" +
      DEFAULT_SOLVER_EXE_CANDIDATES.map((p) => `- ${p}`).join("\n")
  );
}

function htmlEscape(s) {
  return String(s).replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;");
}

app.get("/", (_req, res) => {
  res.type("html").send(`<!doctype html>
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
      code { background: #f3f4f6; padding: 0.1rem 0.25rem; border-radius: 6px; }
    </style>
  </head>
  <body>
    <div class="card">
      <h2 style="margin:0 0 0.25rem 0;">RCPSP Solver (GarvitRetake)</h2>
      <div class="muted">Upload the <code>.rcp</code> instance file that the C++ solver reads. PDF is optional and stored only temporarily.</div>

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
</html>`);
});

app.post(
  "/solve",
  upload.fields([
    { name: "instance_rcp", maxCount: 1 },
    { name: "problem_pdf", maxCount: 1 },
  ]),
  (req, res) => {
    const method = String(req.body?.method || "").trim().toUpperCase();
    const allowedTime = Number(req.body?.allowed_time);

    if (!["TS", "SA", "A"].includes(method)) {
      return res.status(400).type("html").send(`<pre>Invalid method: ${htmlEscape(method)}. Use TS, SA, or A.</pre>`);
    }
    if (!Number.isFinite(allowedTime) || allowedTime <= 0) {
      return res.status(400).type("html").send(`<pre>allowed_time must be > 0</pre>`);
    }

    const instanceFile = req.files?.instance_rcp?.[0];
    if (!instanceFile) {
      return res.status(400).type("html").send(`<pre>Missing instance_rcp (.rcp) upload.</pre>`);
    }

    let solverExe;
    try {
      solverExe = pickSolverExe();
    } catch (e) {
      return res.status(500).type("html").send(`<pre>${htmlEscape(e.message)}</pre>`);
    }

    const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "rcpsp-"));
    const rcpPath = path.join(tmpDir, "instance.rcp");
    fs.writeFileSync(rcpPath, instanceFile.buffer);

    const pdfFile = req.files?.problem_pdf?.[0];
    if (pdfFile && (pdfFile.originalname || "").toLowerCase().endsWith(".pdf")) {
      fs.writeFileSync(path.join(tmpDir, "problem.pdf"), pdfFile.buffer);
    }

    const args = ["--rcp", rcpPath, "--time", String(allowedTime), "--method", method];
    const child = spawn(solverExe, args, { cwd: REPO_ROOT, windowsHide: true });

    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (d) => (stdout += d.toString("utf8")));
    child.stderr.on("data", (d) => (stderr += d.toString("utf8")));

    const killAfterMs = Math.max(allowedTime * 1000 + 2000, allowedTime * 1250);
    const t = setTimeout(() => {
      try {
        child.kill();
      } catch {}
    }, killAfterMs);

    child.on("close", (code) => {
      clearTimeout(t);
      try {
        fs.rmSync(tmpDir, { recursive: true, force: true });
      } catch {}

      const combined = stdout + (stderr ? "\n" + stderr : "");
      const safe = htmlEscape(combined);
      res.type("html").send(`<!doctype html>
<html>
  <head>
    <meta charset="utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1"/>
    <title>RCPSP Solver - Result</title>
    <style>
      body { font-family: ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Arial; margin: 2rem; }
      a { color: #2563eb; text-decoration: none; }
      pre { white-space: pre-wrap; background: #0b1020; color: #e5e7eb; padding: 1rem; border-radius: 12px; overflow: auto; }
      .meta { color: #6b7280; margin-bottom: 0.75rem; }
    </style>
  </head>
  <body>
    <div><a href="/">← Run another</a></div>
    <h2 style="margin-bottom:0.25rem;">Result</h2>
    <div class="meta">Exit code: ${code} | Method: ${htmlEscape(method)} | Time: ${allowedTime}</div>
    <pre>${safe}</pre>
  </body>
</html>`);
    });
  }
);

const port = Number(process.env.PORT || 3000);
app.listen(port, () => {
  console.log(`RCPSP web runner listening on http://127.0.0.1:${port}`);
});

