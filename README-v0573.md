# HeatControl v0.5.7.3 – Pumpenursache und automatische Prüfstand-Freigabe

Ausgangspunkt: v0.5.7.2.

- Web-Übersicht und Pumpenseite zeigen jetzt den Grund für den Pumpenzustand: Raumheizbedarf, Pumpennachlauf (mit Restzeit), Frostschutz, Handbetrieb oder Kein Heizbedarf.
- `/api/status` enthält `pumpReason`.
- Pumpen-Zustandswechsel im Ereignislog enthalten die Ursache.
- Am **getrennten Niedervolt-Prüfstand** wird die PCF8574-Automatik nach erfolgreicher I²C-Initialisierung automatisch freigegeben. Keine zeitliche Begrenzung. P0/P1 folgen nur bei aktiver Simulation und Automatik den Anforderungen.
- Bei Simulationspause bleiben P0/P1 AUS, die Prüfstand-Freigabe bleibt jedoch bestehen. Nach Neustart wird sie erneut hergestellt, sobald der PCF8574 erkannt wird.
- Manuelles AUS, Einzeltest, OTA und I²C-Fehler können die Freigabe weiterhin beenden. Ein Einzeltest erfordert danach erneutes Aktivieren der Automatik über die Testseite.

**WICHTIG:** Ausschließlich an galvanisch vom Heizsystem getrenntem Niedervolt-Prüfstand verwenden. Diese Firmware ist **nicht** für den Anschluss an Wolf Klemmen 17/18 freigegeben. PCF8574-Ausgänge können bei Controller-Ausfall im letzten Zustand bleiben; für den Anlagenbetrieb ist eine unabhängige hardwareseitige Abschaltung notwendig.

Build: `pio run -e esp32-4827S043R-ota`; Upload: `pio run -e esp32-4827S043R-ota -t upload`.

Die Archivstruktur und Syntaxänderungen wurden überprüft; ein vollständiger PlatformIO-Build und Hardwaretest wurden hier nicht durchgeführt.
