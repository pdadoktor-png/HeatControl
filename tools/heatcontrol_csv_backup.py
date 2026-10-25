#!/usr/bin/env python3
"""Backup HeatControl CSV files or archive-import previously saved CSVs."""
import argparse
import datetime
import hashlib
import json
import pathlib
import urllib.error
import urllib.parse
import urllib.request

p = argparse.ArgumentParser()
p.add_argument("mode", choices=["backup", "archive-import"])
p.add_argument("--host", default="192.168.0.102")
p.add_argument("--history", type=pathlib.Path)
p.add_argument("--events", type=pathlib.Path)
args = p.parse_args()
base = "http://" + args.host
def request(path, data=None):
    with urllib.request.urlopen(base + path, data=data, timeout=15) as response:
        return response.read()

if args.mode == "backup":
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    folder = pathlib.Path("heatcontrol-backup-" + stamp)
    folder.mkdir()
    for name in ("history.csv", "events.csv", "events-old.csv"):
        try:
            raw = request("/api/storage/export?file=" + name)
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                print(name, "nicht vorhanden")
                continue
            raise
        target = folder / name
        target.write_bytes(raw)
        print(target, len(raw), "Byte SHA256", hashlib.sha256(raw).hexdigest())
    print("Sicherung abgeschlossen:", folder)
else:
    if not args.history and not args.events:
        p.error("Bitte --history und/oder --events angeben")
    for kind, source in (("history", args.history), ("events", args.events)):
        if source is None:
            continue
        raw = source.read_bytes()
        if not raw:
            raise SystemExit(str(source) + " ist leer")
        if b"\x00" in raw:
            raise SystemExit(str(source) + " enthaelt Nullbytes; keine CSV")
        print("Archivimport:", source, len(raw), "Byte")
        for offset in range(0, len(raw), 1024):
            chunk = raw[offset:offset+1024]
            body = urllib.parse.urlencode({
                "file": kind, "offset": str(offset),
                "data": chunk.decode("utf-8")
            }).encode("utf-8")
            response = json.loads(request("/api/storage/import-archive", body))
            if response.get("bytes") != offset + len(chunk):
                raise RuntimeError("Laengenpruefung fehlgeschlagen")
        print("Gespeichert als /import-" + kind + ".csv; aktive Logs unveraendert")
