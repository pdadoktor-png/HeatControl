# HeatControl – C++-Quellcode formatieren

Dieses Archiv enthält den unveränderten, funktionierenden Stand v0.5.7.3 und eine verbindliche `.clang-format`-Konfiguration. Die C++-Quellen sind **noch nicht automatisch formatiert**, weil clang-format in der Erstellungsumgebung nicht verfügbar war. So werden keine ungetesteten automatischen Code-Umbauten ausgeliefert.

Auf dem Mac im Projektordner:

```bash
brew install clang-format
./scripts/format_code.sh
pio run -e esp32-4827S043R-ota
```

In VS Code: Erweiterung C/C++ installieren, clang-format als Formatter wählen und 'Format Document' ausführen. Die Datei `.clang-format` steuert den Stil. Empfehlung: Änderungen vor und nach der Formatierung mit Git vergleichen.

Formatierungsregeln: vier Leerzeichen, keine einzeiligen if-/for-Blöcke, keine einzeiligen Funktionsdefinitionen, maximal 100 Zeichen pro Zeile soweit möglich, keine automatische Umordnung von Includes.

Hinweis: clang-format fügt bei `if (x) action();` nicht grundsätzlich fehlende geschweifte Klammern hinzu. Solche Änderungen benötigen eine gesonderte, geprüfte Quellcodebereinigung.
