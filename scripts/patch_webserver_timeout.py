"""Patch Arduino-ESP32 2.0.17 WebServer for bounded reads and phase diagnostics.

The patch is idempotent and fails closed if framework source differs.
"""
from pathlib import Path
Import("env")

framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
source = framework / "libraries" / "WebServer" / "src" / "WebServer.cpp"
if not source.is_file():
    raise RuntimeError(f"WebServer source not found: {source}")

text = source.read_text(encoding="utf-8")
read_old = """      if (_currentClient.available()) {
        if (_parseRequest(_currentClient)) {"""
read_new = """      if (_currentClient.available()) {
        // HeatControl: per-Stream-read timeout in milliseconds.
        _currentClient.setTimeout(250);
        if (_parseRequest(_currentClient)) {"""
# Accept both a clean framework and one patched by the previous HeatControl hook.
legacy_read = """      if (_currentClient.available()) {
        // HeatControl: bound each Stream::readStringUntil() wait in Parsing.cpp.
        // Milliseconds; not a whole-request deadline.
        _currentClient.setTimeout(250);
        if (_parseRequest(_currentClient)) {"""
if read_new not in text:
    if legacy_read in text:
        text = text.replace(legacy_read, read_new, 1)
    elif text.count(read_old) == 1:
        text = text.replace(read_old, read_new, 1)
    else:
        raise RuntimeError("Unexpected WebServer::handleClient source (timeout)")

# Diagnostic counters are shared with HeatControl main.cpp; they are only
# written by the Arduino loop task and read after handleClient() returns.
globals_anchor = "void WebServer::handleClient() {"
globals_new = """// HeatControl HTTP phase instrumentation (milliseconds).
volatile uint32_t heatcontrolHttpParseMs = 0;
volatile uint32_t heatcontrolHttpHandleMs = 0;
volatile uint32_t heatcontrolHttpParseSeq = 0;
volatile uint32_t heatcontrolHttpHandleSeq = 0;
char heatcontrolHttpLastUri[96] = {0};

void WebServer::handleClient() {"""
if globals_new not in text:
    if text.count(globals_anchor) != 1:
        raise RuntimeError("Unexpected WebServer::handleClient source (globals)")
    text = text.replace(globals_anchor, globals_new, 1)

parse_old = """        if (_parseRequest(_currentClient)) {
          // because HTTP_MAX_SEND_WAIT"""
parse_new = """        const uint32_t hcParseStart = millis();
        const bool hcParsed = _parseRequest(_currentClient);
        heatcontrolHttpParseMs = millis() - hcParseStart;
        ++heatcontrolHttpParseSeq;
        if (hcParsed) {
          // because HTTP_MAX_SEND_WAIT"""
if parse_new not in text:
    if text.count(parse_old) != 1:
        raise RuntimeError("Unexpected WebServer::handleClient source (parse)")
    text = text.replace(parse_old, parse_new, 1)

handle_old = """          _handleRequest();

// Fix for issue with Chrome based browsers:"""
handle_new = """          // Capture the URI before WebServer clears its request state.
          const String hcUri = uri();
          hcUri.substring(0, sizeof(heatcontrolHttpLastUri) - 1)
              .toCharArray(heatcontrolHttpLastUri, sizeof(heatcontrolHttpLastUri));
          const uint32_t hcHandleStart = millis();
          _handleRequest();
          heatcontrolHttpHandleMs = millis() - hcHandleStart;
          ++heatcontrolHttpHandleSeq;

// Fix for issue with Chrome based browsers:"""
if handle_new not in text:
    if text.count(handle_old) != 1:
        raise RuntimeError("Unexpected WebServer::handleClient source (handle)")
    text = text.replace(handle_old, handle_new, 1)

if source.read_text(encoding="utf-8") != text:
    source.write_text(text, encoding="utf-8")
    print("[HeatControl] WebServer timeout and phase instrumentation patched")
else:
    print("[HeatControl] WebServer timeout and phase instrumentation already active")
