# HeatControl v0.5.8 – Quellcode-Struktur

Ausgangspunkt ist v0.5.7.3 mit den bestehenden Formatierungsregeln.

## Bereits umgesetzt

- Das umfangreiche statische Dashboard-HTML ist **unveraendert** nach `include/WebDashboardPage.h` ausgelagert.
- Das `HistorySample`-Datenformat ist **unveraendert** nach `include/HistorySample.h` ausgelagert.
- Die Strukturen `Phase` und `EventEntry` sind mehrzeilig und lesbarer geschrieben.
- `.editorconfig` und `.clang-format` legen die Formatierung fest.
- Versionskennung auf 0.5.8 angepasst.

## Noch offen

Die weitere Aufteilung der 1400-Zeilen-main.cpp in eigenstaendige Module
und das vollstaendige Anwenden von clang-format sind noch offen.
Diese Schritte sollten nur mit einem erfolgreichen Build nach jeder
Auslagerung erfolgen, um keine funktionierende Regelung zu beschaedigen.

## Mac

`brew install clang-format`

`clang-format -i src/main.cpp src/relay_test.cpp include/HistorySample.h include/WebDashboardPage.h`

**Achtung:** Der Formatter wird auf dem grossen HTML-Raw-String nichts
inhaltlich aendern; JavaScript/HTML sollten separat formatiert werden.

`pio run -e esp32-4827S043R-ota`

Die Firmware wurde hier nicht kompiliert; vor OTA erst lokal bauen.
Die PCF8574-Ausgaenge sind weiterhin nur fuer den getrennten Niedervolt-Pruefstand bestimmt.
