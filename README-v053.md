# HeatControl v0.5.3 – Betriebsanalyse und Optimierung

Basis: vom Nutzer getestete v0.5.2.1. Keine Änderungen an Display, Touch-Kalibrierung, ADS1115, PCF8574-Ausgangssperre oder Heizungsregelung.

## Neu
- Webreiter **Optimierung** und API `GET /api/optimization`.
- Statistische Analyse der letzten 24 Stunden: virtuelle Brennerstarts pro Stunde, Brenner-Einschaltdauer, Pumpenlaufzeit, mittlere Temperaturen und rechnerischer Ölverbrauch.
- Hinweise auf hohe Taktrate bzw. hohen Brenneranteil, **ohne** selbständig Reglerparameter zu verändern.
- Analyse nur bei mindestens vier Samples und mindestens 15 Minuten Daten.

## Grenzen
- Alle Betriebsdaten sind Simulation. Kein realer Ölverbrauch, kein Tankabzug.
- Die Zähler werden nicht persistent geführt; Neustarts können die Auswertung einschränken.
- Statistik basiert auf 5-Minuten-Abtastung; schnelle Schaltvorgänge sind nicht einzeln protokolliert.
- Keine reale Heizung anschließen. PCF8574-Ausgänge bleiben deaktiviert.

## OTA
`pio run -e esp32-4827S043R-ota -t upload`

**Nicht hardwarekompiliert / nicht auf ESP32 getestet.**
