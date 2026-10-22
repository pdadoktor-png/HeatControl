# HeatControl v0.5.7 – Zeitbegrenzte Relais-Automatik (Niedervolt-Pruefstand)

Basis: v0.5.6. Die vorhandene virtuelle Regelung (Hysterese, Mindest-EIN/AUS-Zeit, Pumpennachlauf) wird mit P0/P1 des PCF8574 verbunden.

## Test
1. Nur getrennten Niedervolt-Pruefstand verwenden, **keine Verbindung zur Wolf-Heizung oder Netzspannung**.
2. Simulation starten und Automatik aktivieren.
3. `/relay/test` aufrufen und **Automatik 3 Minuten freigeben**.
4. P0 folgt der simulierten Brenneranforderung, P1 der simulierten Pumpenanforderung.
5. Ablauf, OTA, Simulationsstopp, PCF8574-Fehler und Neustart sperren die Freigabe.
6. `/api/relay` zeigt `benchAuto` und `benchRemainingSeconds`; Ereignislog zeigt Ausgangswechsel.

**Grenze:** Der PCF8574 ist kein sicherheitsgerichteter Ausgang. Bei Absturz oder Leitungsfehler kann der letzte Zustand bestehen bleiben. Diese Firmware ist nicht zum Betrieb eines echten Brenners geeignet.

Eine echte Hardware-Rueckmeldung der Relaiskontakte ist nicht vorhanden; der Status bezeichnet nur den zuletzt geschriebenen PCF8574-Wert.
