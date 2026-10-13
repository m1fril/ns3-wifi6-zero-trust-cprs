#!/usr/bin/env python3
"""Local web UI for the orchestrator: edit configs, start/stop runs, watch instances, live
control, mesh topology view and run verification.

  python3 ui.py [-c config.yaml] [--port 8765]   ->  http://127.0.0.1:8765

Every *.yaml / *.json next to this file is offered as a config; -c picks the default one,
whose run.output_dir is where runs are listed.

Runs are launched as `orchestrator.py run` subprocesses, so supervision and shutdown
behave exactly as on the CLI. Listens on 127.0.0.1 only.
"""

import argparse
import csv
import json
import os
import re
import signal
import subprocess
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

import loadgen
import orchestrator
import verify

HERE = Path(__file__).resolve().parent
NAME_RE = re.compile(r"^[\w.-]{1,64}$")  # run / instance names become path components
METRICS = ("ACTIVE_STAS", "AGGREGATE_THROUGHPUT_MBPS", "SENT_COUNT", "RECV_COUNT", "LATENCY_P95_MS",
           "HOPS_MEAN", "NETEM_DROP_COUNT", "RETRY_DROP_COUNT", "QUEUE_DROP_COUNT", "STACK_DROP_COUNT")
EVENTS = ("NODE_DOWN", "NODE_UP")
CONFIG_SUFFIXES = (".yaml", ".yml", ".json")


class State:
    config_path = None
    port = None
    proc = None      # current `orchestrator.py run` subprocess
    run_name = None


def output_dir():
    try:
        return Path(orchestrator.load_config(str(State.config_path), [])["run"]["output_dir"])
    except Exception:
        return HERE / "runs"


def config_file(name=None):
    """A config next to ui.py, chosen by file name; None means the default (-c)."""
    if not name:
        return State.config_path
    path = HERE / safe(name)
    if path.suffix not in CONFIG_SUFFIXES or not path.is_file():
        raise ValueError(f"unknown config {name!r}")
    return path


def list_configs():
    return sorted(p.name for p in HERE.iterdir() if p.suffix in CONFIG_SUFFIXES and not p.name.startswith("."))


def safe(name):
    if not name or not NAME_RE.match(name) or name.startswith("."):
        raise ValueError(f"bad name {name!r}")
    return name


def pid_alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except (OSError, TypeError):
        return False


def read_json(path):
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return None


def last_sim_time(instance_dir):
    """Progress: timestamp of the last complete CSV row."""
    for f in instance_dir.glob("Bianchi-*.csv"):
        with open(f, "rb") as fh:
            fh.seek(max(0, f.stat().st_size - 512))
            rows = fh.read().decode(errors="ignore").strip().splitlines()
        for row in reversed(rows[1:] if len(rows) > 1 else rows):
            try:
                return float(row.split(",")[0])
            except ValueError:
                continue
    return 0.0


def run_info(name):
    d = output_dir() / safe(name)
    status = read_json(d / "status.json") or {"state": "unknown", "instances": {}}
    if status["state"] in ("running", "stopping") and not pid_alive(status.get("pid")):
        status["state"] = "interrupted"
    summary = read_json(d / "summary.json") or {}
    cfg = read_json(d / "config.resolved.json") or {}
    status["topology"] = cfg.get("sim", {}).get("args", {}).get("topology", "infra")
    status["verify"] = read_json(d / "verify.json")
    for inst, row in status["instances"].items():
        row["sim_time"] = last_sim_time(d / inst)
        row.update({k: v for k, v in summary.get(inst, {}).items() if k not in row})
    return {"name": name, **status, "mtime": (d / "status.json").stat().st_mtime if (d / "status.json").exists() else 0}


def list_runs():
    out = output_dir()
    runs = [run_info(p.name) for p in out.iterdir() if p.is_dir() and NAME_RE.match(p.name)] if out.exists() else []
    return sorted(runs, key=lambda r: -r["mtime"])


def metrics(run, inst):
    """Per-second series, node events and (mesh) topology for the charts and the map.
    ponytail: re-reads the whole CSV per poll; tail-follow if CSVs get big."""
    d = output_dir() / safe(run) / safe(inst)
    series, events, links = {m: [] for m in METRICS}, [], []
    nodes, mac_drops = {}, {}
    for f in d.glob("Bianchi-*.csv"):
        with open(f, newline="") as fh:
            for row in csv.DictReader(fh):
                a, t, nid = row["Action"], float(row["Time_s"]), row["NodeID"]
                if a in series:
                    series[a].append([t, float(row["Value"] or 0)])
                elif a in EVENTS:
                    events.append([t, a, row["NodeType"], nid])
                elif a == "WIFI_DROP":  # one row per packet: bin it like the other counters
                    sec = float(int(t) + 1)
                    mac_drops[sec] = mac_drops.get(sec, 0) + 1
                elif a in ("POS_X", "POS_Y", "STA_CELL"):
                    node = nodes.setdefault(nid, {"type": row["NodeType"]})
                    node[{"POS_X": "x", "POS_Y": "y", "STA_CELL": "cell"}[a]] = float(row["Value"] or 0)
                elif a in ("PEER_LINK_OPEN", "PEER_LINK_CLOSE"):
                    links.append([t, nid, str(int(float(row["Value"] or 0))), a == "PEER_LINK_OPEN"])
    series["MAC_ENQUEUE_DROP_COUNT"] = sorted(mac_drops.items())
    # gateways are the sinks: their index (order of node ids) is the cell number
    gws = sorted((n for n, v in nodes.items() if v["type"] in ("GW", "AP")), key=int)
    for i, n in enumerate(gws):
        nodes[n]["cell"] = i
    positioned = {n: v for n, v in nodes.items() if "x" in v}
    return {"series": series, "events": events, "nodes": positioned, "links": links}


def node_detail(run, inst, node):
    """One node through the run: per-second NODE_* counters, its up/down events, its peer links."""
    d = output_dir() / safe(run) / safe(inst)
    if not node.isdigit():
        raise ValueError("bad node id")
    series, events, links = {}, [], []
    for f in d.glob("Bianchi-*.csv"):
        with open(f, newline="") as fh:
            for row in csv.DictReader(fh):
                a, t = row["Action"], float(row["Time_s"])
                if a in ("PEER_LINK_OPEN", "PEER_LINK_CLOSE") and (row["NodeID"] == node or str(int(float(row["Value"] or 0))) == node):
                    links.append([t, row["NodeID"], str(int(float(row["Value"] or 0))), a == "PEER_LINK_OPEN"])
                if row["NodeID"] != node:
                    continue
                if a in EVENTS:
                    events.append([t, a])
                elif a.startswith("NODE_") or a in ("THROUGHPUT_MBPS", "ROUTE_DISCOVERY_MS"):
                    series.setdefault(a, []).append([t, float(row["Value"] or 0)])
    return {"node": node, "series": series, "events": events, "links": links}


def tail(path, lines=200):
    try:
        with open(path, "rb") as f:
            f.seek(max(0, path.stat().st_size - 64_000))
            return "\n".join(f.read().decode(errors="replace").splitlines()[-lines:])
    except OSError:
        return ""


def start_run(name, config=None):
    if State.proc and State.proc.poll() is None:
        raise ValueError(f"run {State.run_name!r} is still active")
    config = config_file(config)
    cfg = orchestrator.load_config(str(config), [f"run.name={safe(name)}"])  # validates
    if not os.access(cfg["sim"]["binary"], os.X_OK):
        raise ValueError(f"binary not found: {cfg['sim']['binary']} (./ns3 build network-project-main)")
    run_dir = Path(cfg["run"]["output_dir"]) / name
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "summary.json").unlink(missing_ok=True)
    with open(run_dir / "console.log", "w") as out:
        State.proc = subprocess.Popen(
            [sys.executable, str(HERE / "orchestrator.py"), "run", "-c", str(config), "--set", f"run.name={name}"],
            cwd=HERE, stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
    State.run_name = name


def save_config(text, name=None):
    """Validate on a temp copy next to the original (relative paths resolve the same), then swap in."""
    target = config_file(name)
    tmp = target.with_name(".config.check" + target.suffix)
    tmp.write_text(text)
    try:
        orchestrator.load_config(str(tmp), [])
    except (Exception, SystemExit) as e:  # SystemExit = PyYAML missing
        tmp.unlink()
        raise ValueError(f"{type(e).__name__}: {e}") from None
    tmp.replace(target)


def stop_run(name):
    """SIGTERM to the run's orchestrator (also works for runs started from the CLI)."""
    status = read_json(output_dir() / safe(name) / "status.json") or {}
    if pid_alive(status.get("pid")) and status.get("state") in ("running", "stopping"):
        os.kill(status["pid"], signal.SIGTERM)
    elif State.proc and State.proc.poll() is None and State.run_name == name:
        State.proc.send_signal(signal.SIGTERM)  # launched but has not written status.json yet
    else:
        raise ValueError(f"run {name!r} is not running")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def trusted(self):
        """Blocks DNS rebinding and cross-site POSTs: this server can launch binaries."""
        hosts = {f"127.0.0.1:{State.port}", f"localhost:{State.port}"}
        if self.headers.get("Host") not in hosts:
            return False
        origin = self.headers.get("Origin")
        return origin is None or origin.split("//", 1)[-1] in hosts

    def do_GET(self):
        if not self.trusted():
            return self.send(403, {"error": "forbidden"})
        url = urlparse(self.path)
        q = {k: v[0] for k, v in parse_qs(url.query).items()}
        try:
            if url.path == "/":
                return self.send(200, (HERE / "ui.html").read_bytes(), "text/html; charset=utf-8")
            if url.path == "/api/configs":
                return self.send(200, {"default": State.config_path.name, "files": list_configs()})
            if url.path == "/api/config":
                path = config_file(q.get("name"))
                return self.send(200, {"name": path.name, "path": str(path), "text": path.read_text()})
            if url.path == "/api/runs":
                active = State.run_name if State.proc and State.proc.poll() is None else None
                return self.send(200, {"active": active, "runs": list_runs()})
            if url.path == "/api/run":
                return self.send(200, run_info(q["run"]))
            if url.path == "/api/metrics":
                return self.send(200, metrics(q["run"], q["inst"]))
            if url.path == "/api/node":
                return self.send(200, node_detail(q["run"], q["inst"], q["node"]))
            if url.path == "/api/log":
                d = output_dir() / safe(q["run"])
                path = d / safe(q["inst"]) / "stdout.log" if q.get("inst") else d / "orchestrator.log"
                return self.send(200, {"text": tail(path)})
            if url.path == "/api/timeline":
                return self.send(200, {"text": tail(output_dir() / safe(q["run"]) / safe(q["inst"]) / "timeline.txt", 2000)})
            self.send(404, {"error": "not found"})
        except (ValueError, KeyError) as e:
            self.send(400, {"error": str(e)})

    def do_POST(self):
        if not self.trusted() or self.headers.get("Content-Type") != "application/json":
            return self.send(403, {"error": "forbidden"})
        try:
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            path = urlparse(self.path).path
            if path == "/api/config":
                save_config(body["text"], body.get("name"))
                return self.send(200, {"ok": True})
            if path == "/api/start":
                start_run(body.get("name") or time.strftime("run-%Y%m%d-%H%M%S"), body.get("config"))
                return self.send(200, {"ok": True, "name": State.run_name})
            if path == "/api/stop":
                stop_run(body["run"])  # the orchestrator then stops its instances gracefully
                return self.send(200, {"ok": True})
            if path == "/api/verify":
                return self.send(200, verify.verify(output_dir() / safe(body["run"])))
            if path == "/api/ctl":
                inst_dir = output_dir() / safe(body["run"]) / safe(body["inst"])
                return self.send(200, {"ok": True, "queued": orchestrator.send_ctl(inst_dir, body["command"])})
            self.send(404, {"error": "not found"})
        except (ValueError, KeyError, TypeError, OSError, loadgen.ConfigError) as e:
            self.send(400, {"error": str(e)})


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-c", "--config", default=str(HERE / "config.yaml"))
    ap.add_argument("--port", type=int, default=8765)
    a = ap.parse_args()
    State.config_path, State.port = Path(a.config).resolve(), a.port
    server = ThreadingHTTPServer(("127.0.0.1", a.port), Handler)
    print(f"sim-orchestrator UI: http://127.0.0.1:{a.port}  (config: {State.config_path})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if State.proc and State.proc.poll() is None:
            print("stopping active run...")
            State.proc.send_signal(signal.SIGTERM)
            State.proc.wait()


if __name__ == "__main__":
    main()
