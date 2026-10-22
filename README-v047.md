# HeatControl v0.4.7 – GPIO-Diagnose

Basis: v0.4.6. GPIO12/13 werden erst nach Display und ADS1115 initialisiert. Beide LOW-aktiven Relaisausgänge bleiben unabhängig von Simulation und `RELAY ARM` deaktiviert.

Weboberfläche zeigt v0.4.7 und Diagnose; zusätzlich GET `/api/relay` mit GPIO-Pegeln.

**Sicherheitswarnung:** Auch spätes Initialisieren kann einen Einschaltimpuls vor der Initialisierung nicht verhindern. Relaismodul während des Flashens und der ersten Tests spannungsfrei lassen; COM/NO/NC vollständig unbeschaltet. Kein Anschluss an Heizungs- oder Netzstromkreise. Für echten Betrieb unabhängige hardwareseitige Freigabe erforderlich.

USB: `pio run -e esp32-4827S043R -t upload`
OTA (nur wenn normale HeatControl-Firmware läuft): `pio run -e esp32-4827S043R-ota -t upload`

Test: Webinterface `/api/relay` zeigt `initialized:true`, `armed:false`, `outputsEnabled:false`, `gpio12:1`, `gpio13:1`. `RELAY ARM` wird absichtlich abgewiesen.

**Nicht auf Hardware kompiliert/getestet.**
