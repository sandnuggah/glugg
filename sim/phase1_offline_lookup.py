#!/usr/bin/env python3
"""Phase 1 go/no-go: can the leader resolve a Matter service that a sleepy child registered over SRP,
using only the leader's own SRP + DNS-SD server (no Wi-Fi, no mDNS)?

Runs two OpenThread POSIX simulation nodes:
  node 1 "controller" (plays the S3): leader, SRP server, SRP client, DNS client
  node 2 "sensor" (plays a MYGGBETT): sleepy end device, SRP client

The lookups use the same calls and default DNS config that Matter's OpenThread platform code uses
(otDnsClientResolveService, then otDnsClientResolveAddress), so no explicit `dns config` is set.

Usage: sim/phase1_offline_lookup.py [--ot-cli PATH] [--keep]
Build ot-cli-ftd first (see sim/README.md). Stdlib only.
"""

import argparse
import os
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time

DEFAULT_OT_CLI = os.path.expanduser("~/esp/ot-sim/examples/apps/cli/ot-cli-ftd")
LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")

SERVICE = "_matter._tcp"
DOMAIN = "default.service.arpa."
COMPRESSED_FABRIC_ID = "87E1B004E235A130"
SENSOR_INSTANCE = f"{COMPRESSED_FABRIC_ID}-0000000000000001"
SENSOR_HOST = "3A5F0C1B2D4E6F70"
CONTROLLER_INSTANCE = f"{COMPRESSED_FABRIC_ID}-000000000001B669"
CONTROLLER_HOST = "C0FFEE0000000001"
MATTER_PORT = 5540


def txt_hex(entries):
    out = b""
    for e in entries:
        b = e.encode()
        out += bytes([len(b)]) + b
    return out.hex()


# Keys a SIT ICD advertises on its operational record.
SENSOR_TXT = txt_hex(["SII=5000", "SAI=300", "SAT=4000", "T=0", "ICD=0"])


class Node:
    def __init__(self, node_id, name, ot_cli, workdir, log):
        self.name = name
        self.log = log
        self.lines = queue.Queue()
        self.proc = subprocess.Popen(
            [ot_cli, str(node_id)], cwd=workdir, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1)
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        for raw in self.proc.stdout:
            line = raw.rstrip("\r\n")
            self.log.write(f"{time.monotonic():9.2f} [{self.name}] {line}\n")
            self.log.flush()
            self.lines.put(line)

    def cmd(self, command, timeout=10.0):
        """Send a CLI command; return (ok, output lines). Waits for `Done` or `Error ...`."""
        while not self.lines.empty():
            self.lines.get_nowait()
        self.proc.stdin.write(command + "\n")
        self.proc.stdin.flush()
        out, deadline = [], time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                line = self.lines.get(timeout=deadline - time.monotonic())
            except queue.Empty:
                break
            line = line.removeprefix("> ").strip()
            if line == command or not line:
                continue
            if line == "Done":
                return True, out
            if line.startswith("Error"):
                return False, out + [line]
            out.append(line)
        return False, out + [f"timeout after {timeout:.0f} s"]

    def value(self, command):
        ok, out = self.cmd(command)
        return out[0] if ok and out else None

    def wait_for(self, command, predicate, timeout):
        deadline = time.monotonic() + timeout
        out = []
        while time.monotonic() < deadline:
            ok, out = self.cmd(command)
            if ok and predicate(out):
                return True, out
            time.sleep(1)
        return False, out

    def stop(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()


class Report:
    def __init__(self):
        self.results = []

    def check(self, name, ok, detail=""):
        self.results.append((name, ok))
        print(f"  {'PASS' if ok else 'FAIL'}  {name}" + (f"  ({detail})" if detail else ""))
        return ok

    def note(self, text):
        print(f"  ....  {text}")


def joined(out):
    return " | ".join(out)


def start_controller(ot_cli, workdir, log, fresh):
    node = Node(1, "controller", ot_cli, workdir, log)
    if fresh:
        node.cmd("dataset init new")
        node.cmd("dataset commit active")
    node.cmd("ifconfig up")
    node.cmd("thread start")
    node.cmd("srp server enable")
    return node


def register_controller(node):
    # What Matter's SRP client does for the controller's own operational record.
    node.cmd(f"srp client host name {CONTROLLER_HOST}")
    node.cmd("srp client host address auto")
    node.cmd(f"srp client service add {CONTROLLER_INSTANCE} {SERVICE} {MATTER_PORT}")
    node.cmd("srp client autostart enable")


def lookups(r, ctl, sensor_mleid, label):
    ok, out = ctl.cmd(f"dns service {SENSOR_INSTANCE} {SERVICE}.{DOMAIN}", timeout=30)
    r.check(f"{label}: dns service (otDnsClientResolveService) finds the sensor",
            ok and f"Port:{MATTER_PORT}" in joined(out) and SENSOR_HOST.lower() in joined(out).lower(), joined(out))
    has_addr = sensor_mleid.lower() in joined(out).lower()
    r.note(f"{label}: host address {'included' if has_addr else 'NOT included'} in the SRV/TXT answer")

    ok, out = ctl.cmd(f"dns resolve {SENSOR_HOST}.{DOMAIN}", timeout=30)
    r.check(f"{label}: dns resolve (otDnsClientResolveAddress) returns the sensor's ML-EID",
            ok and sensor_mleid.lower() in joined(out).lower(), joined(out))


def run(ot_cli, keep):
    workdir = tempfile.mkdtemp(prefix="ot-phase1-")
    os.makedirs(LOG_DIR, exist_ok=True)
    log_path = os.path.join(LOG_DIR, time.strftime("phase1-%Y%m%d-%H%M%S.log"))
    log = open(log_path, "w")
    r = Report()
    nodes = []
    try:
        print(f"OpenThread: {ot_cli}\nWork dir:   {workdir}\nLog:        {log_path}\n")
        print("1. Controller forms the network and starts its own SRP server")
        ctl = start_controller(ot_cli, workdir, log, fresh=True)
        nodes.append(ctl)
        r.note(f"version {ctl.value('version')}")
        ok, out = ctl.wait_for("state", lambda o: o == ["leader"], 30)
        if not r.check("controller becomes leader", ok, joined(out)):
            return r
        ok, out = ctl.wait_for("srp server state", lambda o: o == ["running"], 30)
        r.check("SRP server running", ok, joined(out))
        ctl_mleid = ctl.value("ipaddr mleid")
        r.note(f"controller ML-EID {ctl_mleid}, SRP server port {ctl.value('srp server port')}, "
               f"addrmode {ctl.value('srp server addrmode')}")

        print("2. Controller's own SRP client registers with its own server (as Matter would)")
        register_controller(ctl)
        ok, out = ctl.wait_for("srp client service", lambda o: "Registered" in joined(o), 30)
        r.check("controller's own service registered on itself", ok, joined(out))
        ok, out = ctl.cmd("dns config")
        server = next((l for l in out if l.startswith("Server:")), "")
        r.check("DNS client default server is the controller's own ML-EID, port 53",
                f"[{ctl_mleid}]:53" in server, server)

        print("3. Sensor joins as a sleepy end device and registers over SRP")
        dataset = ctl.value("dataset active -x")
        sensor = Node(2, "sensor", ot_cli, workdir, log)
        nodes.append(sensor)
        sensor.cmd(f"dataset set active {dataset}")
        sensor.cmd("mode -")
        sensor.cmd("pollperiod 2000")
        sensor.cmd("ifconfig up")
        sensor.cmd("thread start")
        ok, out = sensor.wait_for("state", lambda o: o == ["child"], 60)
        if not r.check("sensor attaches as child", ok, joined(out)):
            return r
        r.note(f"sensor mode {sensor.value('mode')}, poll period {sensor.value('pollperiod')} ms")
        sensor_mleid = sensor.value("ipaddr mleid")
        sensor.cmd(f"srp client host name {SENSOR_HOST}")
        sensor.cmd("srp client host address auto")
        sensor.cmd(f"srp client service add {SENSOR_INSTANCE} {SERVICE},_I{COMPRESSED_FABRIC_ID} "
                   f"{MATTER_PORT} 0 0 {SENSOR_TXT}")
        sensor.cmd("srp client autostart enable")
        ok, out = sensor.wait_for("srp client service", lambda o: "Registered" in joined(o), 60)
        r.check("sensor's service registered", ok, joined(out))
        r.note(f"sensor's SRP server: {sensor.value('srp client server')}")

        ok, out = ctl.cmd("srp server host")
        r.check("controller's SRP server table has the sensor host",
                ok and SENSOR_HOST.lower() in joined(out).lower() and sensor_mleid.lower() in joined(out).lower(),
                joined(out))

        print("4. Controller resolves the sensor through its own DNS-SD server")
        lookups(r, ctl, sensor_mleid, "fresh")
        ok, out = ctl.cmd(f"dns browse {SERVICE}.{DOMAIN}", timeout=30)
        r.check("dns browse lists the sensor instance", ok and SENSOR_INSTANCE in joined(out), joined(out))
        ok, out = ctl.cmd(f"dns browse _I{COMPRESSED_FABRIC_ID}._sub.{SERVICE}.{DOMAIN}", timeout=30)
        r.check("dns browse by fabric subtype lists the sensor", ok and SENSOR_INSTANCE in joined(out), joined(out))
        ok, out = ctl.cmd(f"dns service {COMPRESSED_FABRIC_ID}-00000000DEADBEEF {SERVICE}.{DOMAIN}", timeout=30)
        r.check("unknown instance is NotFound (negative control)", not ok and "NotFound" in joined(out), joined(out))

        print("5. Controller loses power and reboots (SRP registrations are not persisted)")
        old_port = ctl.value("srp server port")
        ctl.stop()
        nodes.remove(ctl)
        time.sleep(3)
        ctl = start_controller(ot_cli, workdir, log, fresh=False)
        nodes.append(ctl)
        ok, out = ctl.wait_for("state", lambda o: o == ["leader"], 60)
        r.check("controller is leader again with the saved dataset", ok, joined(out))
        ok, out = ctl.wait_for("srp server state", lambda o: o == ["running"], 30)
        new_port = ctl.value("srp server port")
        r.check("SRP server running on a new port (tells clients to re-register)",
                ok and new_port != old_port, f"port {old_port} -> {new_port}")
        register_controller(ctl)
        t0 = time.monotonic()
        ok, out = ctl.wait_for("srp server host", lambda o: SENSOR_HOST.lower() in joined(o).lower(), 120)
        r.check("sensor re-registers after the reboot", ok,
                f"after {time.monotonic() - t0:.0f} s" if ok else joined(out))
        if ok:
            lookups(r, ctl, sensor_mleid, "after reboot")
        return r
    finally:
        for n in nodes:
            n.stop()
        log.close()
        if keep:
            print(f"\nKept work dir {workdir}")
        else:
            shutil.rmtree(workdir, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--ot-cli", default=DEFAULT_OT_CLI, help="path to simulation ot-cli-ftd")
    ap.add_argument("--keep", action="store_true", help="keep the node settings directory")
    args = ap.parse_args()
    if not os.access(args.ot_cli, os.X_OK):
        sys.exit(f"ot-cli-ftd not found at {args.ot_cli}; build it first (see sim/README.md)")
    r = run(args.ot_cli, args.keep)
    failed = [n for n, ok in r.results if not ok]
    print(f"\n{len(r.results) - len(failed)}/{len(r.results)} checks passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
