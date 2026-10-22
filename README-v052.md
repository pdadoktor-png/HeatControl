# HeatControl v0.5.2 – Betriebsstatistik (Simulation)

Baut auf v0.5.1 auf. Touch, ADS1115, PCF8574-Diagnose, OTA, Heizkurve und Öltankverwaltung bleiben erhalten.

## Webinterface

Neue Seite **Statistik**: 24-Stunden- und 7-Tage-Temperaturkurven, virtuelle Brennerstarts, virtuelle Brenner-/Pumpenlaufzeiten, Öl-Schätzwert mit einstellbarem Düsendurchsatz, CSV-Export.

API: `/api/history/status`, `/api/history?hours=24`, `/api/history?hours=168`, `/api/history.csv`, POST `/api/oilrate` mit `rate=2.0`.

Alle fünf Minuten wird ein Datensatz in einem RAM-Ringpuffer (maximal 2016 Einträge) gespeichert. Wenn LittleFS erfolgreich gemountet wird, erfolgt zusätzlich eine persistente Speicherung in `/history.csv`. Bei fehlendem Dateisystem läuft die Statistik nur im RAM weiter. Vorhandene Partitionen werden **nicht** formatiert.

Bei fehlender NTP-Zeit ist `epoch_utc=0`; Daten werden nach einem Neustart ggf. nicht korrekt dem Tages-/Wochenfenster zugeordnet. Die Zähler für simulierte Starts/Laufzeiten beginnen bei jedem Neustart wieder bei null.

Die angezeigten Brenner-/Pumpenstunden sind ausschließlich Simulationswerte und ändern niemals den manuell gepflegten Heizöltankbestand. PCF8574-Ausgänge bleiben standardmäßig gesperrt. Keine Verbindung zu realer Heizung/Netzspannung.

## OTA

`pio run -e esp32-4827S043R-ota -t upload`

## Hinweis

Quellcode und ZIP geprüft; Firmware wurde hier nicht auf der ESP32-Hardware ausgeführt.
