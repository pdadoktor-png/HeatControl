# HeatControl v0.5.7.1 – unbegrenzte Prüfstand-Automatik

Auf Grundlage v0.5.7. Die automatische Relaisfreigabe bleibt nach manueller Aktivierung ohne Zeitlimit eingeschaltet, solange Simulation und Automatik aktiv sind und keine I2C-/OTA-Störung vorliegt. Sie wird bei manuellem AUS, Simulationsstopp, Automatik AUS, I2C-Fehler, OTA oder Neustart gesperrt. Einzeltests weiterhin 2 Sekunden. Nur getrennter Niedervolt-Prüfstand; keine Verbindung zur Heizung.

Die API `/api/relay` meldet `benchRemainingSeconds: -1` als Kennzeichnung für unbegrenzte Freigabe.

Kompilieren: `pio run -e esp32-4827S043R-ota`
OTA: `pio run -e esp32-4827S043R-ota -t upload`

Nicht hardwaregetestet oder vollständig kompiliert.
