# HeatControl v0.5.1 – Heizkurve und Öltank

Basis: v0.5.0 PCF8574-I2C mit Touch-Kalibrierung, ADS1115 und OTA.

## Heizkurve
Webinterface → Heizkurve: SVG-Diagramm mit exakt der im Firmwarecode verwendeten Solltemperaturformel und aktuellem Betriebspunkt. Außentemperatur -20 bis +20 °C, Solltemperatur 25–75 °C. Änderungen über bestehende Eingabefelder speichern.

## Öltank
Webinterface → Öltank: feste Kapazität 4200 Liter, manuell erfasster Bestand und Hinzubuchung einer Lieferung. Speicherung in Preferences/NVS. Kein automatischer Abzug aus simulierten Brennerstunden. Keine Umrechnung der Füllhöhe bei bauchigem Tank ohne Kalibriertabelle. Kein vollständiges Lieferjournal; letzte Menge und Anzahl werden gespeichert.

API: GET /api/tank; POST /api/tank mit action=set|delivery und liters=<Zahl>.

## Sicherheit
PCF8574-Relaisausgänge bleiben standardmäßig deaktiviert (HEATCONTROL_PCF8574_OUTPUTS_ENABLED=0). Keine Verbindung mit Brenner oder Pumpe.

## OTA
`pio run -e esp32-4827S043R-ota -t upload` bei laufender normaler HeatControl-Firmware und erreichbarem Heim-WLAN.

Eine vollständige Hardwareprüfung wurde nicht durchgeführt.
