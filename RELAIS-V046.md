# HeatControl v0.4.6 – Niedervolt-Relaisprüfstand

Basierend auf dem vom Benutzer bereitgestellten Projekt v0.4.5 FIX2.

- Normale HeatControl-Simulation, Display, Webinterface, ADS1115, OTA bleiben enthalten.
- GPIO12 (Relais 1) und GPIO13 (Relais 2) LOW-aktiv.
- Beim Start beide AUS. Die Relais sind **nicht dauerhaft** mit der Simulation verbunden.
- Serieller Monitor (115200 Baud): `RELAY STATUS`, `RELAY ARM`, `RELAY OFF`.
- `RELAY ARM` spiegelt die virtuellen Brenner-/Pumpenanforderungen für maximal 30 Sekunden auf die beiden Relais. Danach AUS. Kein Web-ARM, kein Speichern der Freigabe.
- Vor dem Test alle NO/COM/NC-Kontakte unbeschaltet lassen; **keine Verbindung zu Brenner, Pumpe oder 230 V**.
- Der ESP32 ist kein zertifizierter Heizungsregler; Neustart, Brownout, Firmwarehänger und Fehlzustände sind noch nicht sicherheitsgerichtet abgefangen. Ein Test mit Relais beweist keine Eignung zur Steuerung der Heizung.

Build: `pio run -e esp32-4827S043R` ; Upload: `pio run -e esp32-4827S043R -t upload`

Der separate `relay-test` bleibt verfügbar. Nicht mit diesem verwechseln.

Hinweis: OTA-Zugangsdaten aus dem ursprünglichen Projekt wurden übernommen; ZIP vertraulich behandeln.
