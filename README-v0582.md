# HeatControl v0.5.8.2 – Kontrollblöcke

Dieses Paket basiert auf v0.5.8.1 und enthält eine korrigierte `.clang-format`-Datei mit `InsertBraces: true` (clang-format 23). Die C++-Quelldateien im ZIP sind **noch nicht automatisch umgeschrieben**; dies erfolgt auf dem Mac mit dem beigefügten Skript. So werden Kontrollanweisungen durch den C++-Formatter und nicht durch fehleranfällige Text-Ersetzungen umgebaut.

```sh
./scripts/format_code.sh
pio run -e esp32-4827S043R-ota
```

`InsertBraces` ist in clang-format experimentell; bitte die Änderungen vor dem Flashen mit `git diff` prüfen und die Firmware auf dem Prüfstand testen. Keine beabsichtigte Änderung der Regelungslogik. Versionsanzeige in der Firmware bleibt vorerst v0.5.8.1, bis die Umformatierung erfolgreich geprüft wurde.
