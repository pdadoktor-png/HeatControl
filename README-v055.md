# HeatControl v0.5.5 – Ereignisprotokoll

Aufbauend auf v0.5.4.1. Neuer Reiter **Ereignisse** und HTTP-Endpunkte `/api/events`, `/api/events.csv`.

Protokolliert werden Neustart, Simulations- und Freigabewechsel sowie virtuelle Brenner- und Pumpenanforderungen mit Grundinformationen. Es handelt sich **nicht** um den Nachweis eines tatsächlichen Brennerbetriebs. Im RAM liegen die letzten 160 Ereignisse; mit LittleFS werden Ereignisse in `/events.csv` gespeichert und bei 120 KB in `/events-old.csv` rotiert. Die CSV enthält beide Dateien. Bei Neustart wird nur das neue RAM-Protokoll angezeigt; die CSV enthält weiterhin ältere Einträge.

**Relaisausgänge bleiben gesperrt**. Die Schaltkontakte 17/18 sind nicht Bestandteil dieser Firmwareänderung. Vor einer realen Anbindung sind Spannung, Kontaktbelastbarkeit, elektrische Trennung und das Fehlerverhalten fachgerecht zu prüfen. Die unveränderte Sicherheitsschaltung bleibt zwingend erforderlich.

Die Software wurde hier nicht auf ESP32-Hardware kompiliert oder getestet.
