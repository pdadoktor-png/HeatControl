"""Limit blocking HTTP request-line/header reads in Arduino-ESP32 WebServer 2.0.17.

This build hook patches the locally installed PlatformIO framework, not GitHub.
It fails loudly if the upstream code differs from the verified version.
"""
from pathlib import Path
Import("env")

framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
source = framework / "libraries" / "WebServer" / "src" / "WebServer.cpp"
if not source.is_file():
    raise RuntimeError(f"WebServer source not found: {source}")

original = """      if (_currentClient.available()) {
        if (_parseRequest(_currentClient)) {"""
patched = """      if (_currentClient.available()) {
        // HeatControl: bound each Stream::readStringUntil() wait in Parsing.cpp.
        // Milliseconds; not a whole-request deadline.
        _currentClient.setTimeout(250);
        if (_parseRequest(_currentClient)) {"""

text = source.read_text(encoding="utf-8")
if patched in text:
    print("[HeatControl] WebServer HTTP read timeout patch already active (250 ms)")
elif text.count(original) == 1:
    source.write_text(text.replace(original, patched, 1), encoding="utf-8")
    print("[HeatControl] Applied WebServer HTTP read timeout patch (250 ms)")
else:
    raise RuntimeError("WebServer.cpp differs from verified Arduino-ESP32 2.0.17; refusing to patch")
