# HeatControl v0.4.8 – Niedervolt-Relaispruefstand

Basierend auf v0.4.7. GPIO12 (Brenner) und GPIO13 (Pumpe) werden erst nach Display und ADS1115 als Ausgaenge initialisiert. Beide LOW-aktiven Relais folgen automatisch den virtuellen Anforderungen. Keine ARM-Freigabe. Webversion und /api/relay auf 0.4.8 aktualisiert. OTA bleibt konfiguriert.

**Nur Niedervolt-Pruefstand:** COM/NO/NC unbeschaltet lassen. NICHT mit Brenner, Wilo-Pumpe oder 230 V verbinden. Diese Firmware ist keine sichere Heizungssteuerung. Ein Boot-Glitch, Absturz oder Hardwarefehler kann ein Relais unbeabsichtigt aktivieren; eine unabhängige Hardware-Sperre ist fuer Realbetrieb notwendig.

OTA: `pio run -e esp32-4827S043R-ota -t upload` (wenn die aktuelle Firmware OTA unter 192.168.0.102 bereitstellt). USB: `pio run -e esp32-4827S043R -t upload`.

Diagnose: `http://192.168.0.102/api/relay`. Bei OTA-Start werden die Relais deaktiviert. Nach OTA-Fehler bleibt die Abschaltung bis zum nächsten Regelzyklus nicht garantiert; realer Heizungsanschluss bleibt untersagt.
