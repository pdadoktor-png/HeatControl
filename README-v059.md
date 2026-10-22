# HeatControl v0.5.9 – Nur Außentemperatur

- Kesselsolltemperatur = clamp(Bezugstemperatur + Steilheit × (Bezugstemperatur − Außentemperatur), 25 °C, 75 °C).
- Brennerregelung mit unveränderter Hysterese und Mindestschaltzeiten.
- Raumtemperatur beeinflusst weder Kesselsoll noch Pumpenanforderung.
- Die Heizkreispumpe läuft bei aktivem witterungsgeführtem Heizbetrieb unterhalb der Sommergrenze; Frostschutz und Nachlauf bleiben erhalten.
- ADS1115-Kanal 1 (Raum) ist optional; Kanäle 0 (Außen) und 2 (Kessel) sind weiterhin erforderlich.
- Komfort-/Absenktemperatur dient weiterhin als Heizkurven-Bezugstemperatur.
- Ausschließlich für den getrennten Niedervolt-Prüfstand; nicht für unbeaufsichtigten Anlagenbetrieb freigegeben.
- Kompilierung und Hardwaretests müssen noch durchgeführt werden.
