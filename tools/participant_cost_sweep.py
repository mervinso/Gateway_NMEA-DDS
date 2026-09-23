#!/usr/bin/env python3
# tools/participant_cost_sweep.py — barrido de la Tarea 4 (RQ3): costo de
# descubrimiento por participante.
#
# Ejecuta tools/participant_cost_bench sobre P ∈ {0, 1, 2, 4, 8, 16, 24, 32}
# con R repeticiones por nivel, en orden aleatorizado con semilla registrada,
# y escribe:
#
#   <out>/runs.jsonl      una línea JSON por ejecución (dato crudo, inmutable)
#   <out>/manifest.json   procedencia: host, kernel, git, transporte, orden, semilla
#
# El nivel P = 0 es la línea base del paso 2 (el proceso sin ningún participante).
# El transporte se fija a UDPv4 puro vía FASTDDS_BUILTIN_TRANSPORTS, igual que en
# la campaña de medición, y queda registrado en el manifiesto.
#
# El ajuste OLS de d sobre P(P−1)/2 NO vive aquí: es analysis/rq3 en el
# repositorio de la tesis. Este script solo produce el dato crudo y su
# procedencia. El resultado del host ARM se copia a
# dds-own/data/raw/participant_cost/.

import argparse
import datetime as dt
import json
import os
import platform
import random
import subprocess
import sys
from pathlib import Path

DEFAULT_LEVELS = "0,1,2,4,8,16,24,32"


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def git_info(repo: Path) -> dict:
    head = sh(["git", "-C", str(repo), "rev-parse", "HEAD"]).stdout.strip()
    describe = sh(["git", "-C", str(repo), "describe", "--always", "--dirty"]).stdout.strip()
    status = sh(["git", "-C", str(repo), "status", "--porcelain"]).stdout
    return {"head": head, "describe": describe, "dirty": bool(status.strip())}


def fastdds_lib(bench: Path) -> str:
    out = sh(["ldd", str(bench)]).stdout
    libs = [line.strip() for line in out.splitlines() if "fastdds" in line]
    return "; ".join(libs) if libs else "desconocida (ldd sin coincidencias)"


def main() -> int:
    ap = argparse.ArgumentParser(description="Barrido Tarea 4: P participantes ociosos, CPU estacionario.")
    ap.add_argument("--bench", default=str(Path(__file__).resolve().parent.parent / "build" / "participant_cost_bench"))
    ap.add_argument("--out", default="participant_cost_out")
    ap.add_argument("--levels", default=DEFAULT_LEVELS, help=f"niveles P separados por coma (defecto {DEFAULT_LEVELS})")
    ap.add_argument("--reps", type=int, default=5, help="repeticiones por nivel (defecto 5, fijado por la Tarea 3)")
    ap.add_argument("--window-s", type=float, default=60.0)
    ap.add_argument("--settle-s", type=float, default=10.0)
    ap.add_argument("--domain", type=int, default=199)
    ap.add_argument("--seed", type=int, default=20260922, help="semilla del orden de ejecución (registrada en el manifiesto)")
    ap.add_argument("--transports", default="UDPv4", help="valor de FASTDDS_BUILTIN_TRANSPORTS (defecto UDPv4)")
    args = ap.parse_args()

    bench = Path(args.bench).resolve()
    if not bench.is_file():
        print(f"no existe el binario del bench: {bench}", file=sys.stderr)
        return 2

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    runs_path = out / "runs.jsonl"
    if runs_path.exists():
        print(f"{runs_path} ya existe; el dato crudo es inmutable — usa otro --out", file=sys.stderr)
        return 2

    levels = [int(x) for x in args.levels.split(",")]
    schedule = [(p, r) for p in levels for r in range(1, args.reps + 1)]
    random.Random(args.seed).shuffle(schedule)

    env = dict(os.environ)
    env["FASTDDS_BUILTIN_TRANSPORTS"] = args.transports

    repo = Path(__file__).resolve().parent.parent
    manifest = {
        "task": "rq3-task4-participant-cost",
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "host": {
            "hostname": platform.node(),
            "uname": " ".join(platform.uname()),
            "nproc": os.cpu_count(),
            # El CPU medido es el del proceso (getrusage), así que otra carga solo
            # entra por contención de planificación; se registra para poder juzgarla.
            "loadavg_at_start": os.getloadavg(),
        },
        "git": git_info(repo),
        "bench": str(bench),
        "fastdds_lib": fastdds_lib(bench),
        "env": {"FASTDDS_BUILTIN_TRANSPORTS": args.transports},
        "params": {
            "levels": levels,
            "reps": args.reps,
            "window_s": args.window_s,
            "settle_s": args.settle_s,
            "domain": args.domain,
            "seed": args.seed,
        },
        "order": [{"participants": p, "rep": r} for p, r in schedule],
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    total = len(schedule)
    failures = 0
    with runs_path.open("a", encoding="utf-8") as fh:
        for i, (p, r) in enumerate(schedule, 1):
            print(f"[{i:3d}/{total}] P={p:2d} rep={r} ...", file=sys.stderr, flush=True)
            proc = subprocess.run(
                [str(bench), "--participants", str(p),
                 "--window-s", str(args.window_s),
                 "--settle-s", str(args.settle_s),
                 "--domain", str(args.domain)],
                capture_output=True, text=True, env=env,
            )
            if proc.returncode != 0:
                failures += 1
                print(f"    FALLO (rc={proc.returncode}): {proc.stderr.strip()}", file=sys.stderr)
                record = {"participants": p, "rep": r, "failed": True,
                          "returncode": proc.returncode, "stderr": proc.stderr.strip()}
            else:
                record = json.loads(proc.stdout)
                record["rep"] = r
                record["utc"] = dt.datetime.now(dt.timezone.utc).isoformat()
            fh.write(json.dumps(record) + "\n")
            fh.flush()

    # Resumen orientativo (el ajuste real de d es del lado del análisis).
    by_level: dict[int, list[float]] = {}
    for line in runs_path.read_text(encoding="utf-8").splitlines():
        rec = json.loads(line)
        if not rec.get("failed"):
            by_level.setdefault(rec["participants"], []).append(rec["cpu_per_s"])
    print("\nP   n  media cpu/s", file=sys.stderr)
    for p in sorted(by_level):
        vals = by_level[p]
        print(f"{p:2d} {len(vals):2d}  {sum(vals)/len(vals):.6f}", file=sys.stderr)

    manifest["host"]["loadavg_at_end"] = os.getloadavg()
    manifest["completed_utc"] = dt.datetime.now(dt.timezone.utc).isoformat()
    manifest["failures"] = failures
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    print(f"\ncrudo:      {runs_path}", file=sys.stderr)
    print(f"manifiesto: {out / 'manifest.json'}", file=sys.stderr)
    if failures:
        print(f"{failures} ejecuciones fallaron — revisa runs.jsonl", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
