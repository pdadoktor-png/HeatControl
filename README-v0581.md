# HeatControl v0.5.8.1 – lesbarer Quellcode

Diese Version basiert auf v0.5.8. Mehrere C++-Anweisungen, die auf derselben Quelltextzeile durch Semikolons getrennt waren, wurden soweit syntaktisch eindeutig in einzelne Zeilen zerlegt. Zeichenketten, Kommentare und `for`-Kopfzeilen werden dabei berücksichtigt.

**Wichtig:** Dies ist eine konservative Quelltext-Bereinigung und keine vollständige automatische clang-format-Formatierung. Einzeilige Kontrollblöcke und verschachtelte Konstrukte können weiterhin manuelle Bereinigung benötigen. Die Kompilierung wurde hier nicht geprüft.

Für die abschließende einheitliche Einrückung auf dem Mac:

```bash
brew install clang-format
find src -name '*.cpp' -exec clang-format -i -style=file {} +
pio run -e esp32-4827S043R-ota
```

Die Regelungslogik wurde nicht absichtlich geändert. Bitte vor einem OTA-Upload kompilieren und am Prüfstand testen.
