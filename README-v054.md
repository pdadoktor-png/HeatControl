# HeatControl v0.5.4 – Optimierung 24 Stunden / 7 Tage

Basis: funktionierende v0.5.3, sonstige Funktionen unveraendert.

Neu: Zeitraumwahl im Webinterface unter Optimierung (24 h oder 7 Tage). API: GET /api/optimization?hours=24 bzw. ?hours=168. Auswertung bleibt rein lesend und simulationsbasiert; keine automatischen Parameter- oder Tankbestandsaenderungen.

Wichtig: Bei Neustarts und fehlender Systemzeit koennen historische Zaehlerdaten unvollstaendig sein. Ein ausreichend langer Beobachtungszeitraum ist keine Garantie fuer statistisch belastbare Ergebnisse.

Installation: pio run -e esp32-4827S043R-ota -t upload

Die Firmware wurde in dieser Umgebung nicht mit PlatformIO kompiliert und nicht auf Hardware getestet. Relais nur am Niedervolt-Pruefstand; nicht an der realen Heizung anschliessen.
