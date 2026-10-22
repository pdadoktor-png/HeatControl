# HeatControl v0.4.9 – Touch-Kalibrierung

Basis: v0.4.8. GPIO12/13 werden durch die Relaisfunktionen nicht mehr konfiguriert; Relaisausgaenge deaktiviert. Display, Sensoren, Heizkurve, Webinterface und OTA bleiben enthalten.

## Installation
`pio run -e esp32-4827S043R-ota -t upload` (nur wenn OTA aktuell laeuft), sonst USB.

## Kalibrierung
1. Relaisplatine von GPIO12/13 und 5V trennen.
2. `http://192.168.0.102/touch/calibrate` im Browser oeffnen (oder das Touch-Menue `Uebersicht > Touch kalibrieren`).
3. `Kalibrierung starten` druecken. Auf dem Display die drei Pluszeichen jeweils einmal antippen, Finger dazwischen anheben.
4. Nach dem dritten Punkt wird die affine Korrektur in NVS gespeichert.
5. `Kalibrierung zuruecksetzen` setzt die Korrektur zurueck; danach erneut kalibrieren.

Die Korrektur verwendet die vom Treiber gelieferten (nicht die rohen ADC-) Koordinaten und wirkt vor LVGL-Hit-Testing. Die Loesung ist ein ungetesteter Prototyp: bei verdrehter oder stark verzerrter Touchabbildung kann eine Neukalibrierung noetig sein. Die drei Punkte muessen physisch korrekt beruehrt werden. Keine echte Heizung anschliessen.
