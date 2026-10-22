# HeatControl v0.5.0 – PCF8574-I2C-Vorbereitung

Basis: die funktionierende v0.4.9 mit Touch-Kalibrierung. Keine ESP32-Relais-GPIOs werden verändert.

## Schon heute testen (PCF8574 noch nicht vorhanden)

1. Firmware via `pio run -e esp32-4827S043R-ota -t upload` übertragen.
2. Im Browser `http://192.168.0.102/api/relay` aufrufen: `present:false`, `outputsEnabled:false`.
3. Touch-Kalibrierung, Touchbedienung und ADS1115 testen.
4. Wenn der PCF8574 morgen angeschlossen ist, sollte `present:true` erscheinen.

## Geplante Verdrahtung (nur spannungsfrei!)

- PCF8574 VCC = ESP32 3,3 V, GND = ESP32 GND, SDA = GPIO17, SCL = GPIO18.
- PCF8574 ohne A-Suffix: A0/A1/A2 nach GND -> Adresse 0x20. PCF8574A hat anderen Adressbereich (0x38–0x3F).
- ADS1115 bleibt bei 0x48. Auf I2C-Bus darf keine 5-V-Pull-up-Spannung liegen.
- P0 = Brenner-Test, P1 = Pumpen-Test. **Relaisplatine vorerst NICHT anschließen.** PCF8574 HIGH ist schwach getrieben; elektrische Eignung für das konkrete LOW-aktive 5-V-Modul und definierter Power-On-OFF-Zustand sind erst zu prüfen.

## Sicherheit / Aktivierung

Die Build-Option `HEATCONTROL_PCF8574_OUTPUTS_ENABLED=0` bleibt absichtlich deaktiviert. Die Firmware erkennt das I2C-Gerät und setzt nach Erkennung alle P0–P7 auf HIGH. Eine I2C-Antwort beweist weder den Chiptyp noch die tatsächliche Relaisstellung. Für spätere Niedervolt-Tests kann der Schalter nach elektrischer Prüfung auf 1 gesetzt werden. Keine Verbindung zur Wolf-Heizung / Netzspannung.

## Diagnose

- `/api/relay`: `present`, `outputsEnabled`, `burnerDemand`, `pumpDemand`, `errors`, `success`.
- Seriell: `RELAY STATUS` und `RELAY OFF`.
- Falls ADS1115 erneut I2C initialisiert, werden PCF-Zustände beim nächsten Update erneuert.
- Falls Touch oder ADS nach dem Flash ausfallen: keine Relaisplatine anschließen; Firmware v0.4.9 zurückflashen.

OTA-Passwort und WLAN-Konfiguration aus der bestehenden Projektversion übernommen.
