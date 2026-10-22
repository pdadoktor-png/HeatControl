# HeatControl v0.5.6 – PCF8574 Niedervolt-Relaispruefstand

Basis: v0.5.5. Touch, ADS1115, Statistik, Heizkurve, Oeltank, Ereignislog bleiben erhalten.

## Test
1. PCF8574 mit 3,3 V versorgen, SDA GPIO17, SCL GPIO18, A0/A1/A2 GND (0x20).
2. Nur getrennten Niedervolt-Pruefstand anschliessen. **Keine Wolf-Heizung, keine Netzspannung.**
3. OTA hochladen; http://192.168.0.102/relay/test aufrufen.
4. P0 oder P1: je ein aktiv-LOW-Testpuls von maximal 2 Sekunden.
5. Diagnose unter /api/relay und Ereignisse im Webinterface pruefen.

## API
- GET /api/relay: Status einschliesslich testActive, testPin, testRemainingMs
- POST /api/relay/test mit pin=0 oder pin=1: 2-s-Puls
- POST /api/relay/off: sofort aus

Die automatische Relaisansteuerung bleibt per Build-Flag deaktiviert. PCF8574 HIGH ist quasi-bidirektional und kein kraeftiger 3,3-V-Ausgang. Elektrische Kompatibilitaet des Relaismoduls muss vor Anschluss geprueft werden. PCF8574 besitzt keine unabhaengige sichere Abschaltung bei Firmwarestillstand.

**Nicht auf einem angeschlossenen Heizkessel testen.**
