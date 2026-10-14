# HeatControl v0.2.3 – GPIO-Zuordnung (Simulation)

Board: ESP32-4827S043R, PlatformIO/VS Code.

## Anschlussplan (vorläufig, anhand Aufdruck auf Foto)

- P4: GND, 3.3V, IO17, IO18
- ADS1115 VDD -> P4 3.3V; GND -> P4 GND; SDA -> IO17; SCL -> IO18; ADDR -> GND (0x48)
- ADS1115 A0 -> NTC Aussen, A1 -> NTC Raum, A2 -> NTC Kessel (jeweils mit geeignetem Spannungsteiler, noch zu dimensionieren).
- P3: IO17, IO18, IO19, IO20: IO17/18 sind dieselben Signale wie P4, IO19/20 fuer USB reservieren.
- P2: IO10-13 SPI: reserviert.
- Brennerrelais: KEIN GPIO festgelegt, Ausgang deaktiviert (-1).

**ACHTUNG:** IO17/18 sind auf P3 als UART1 bezeichnet. Vor Anschluss ADS1115 erst im laufenden Displaybetrieb auf Konflikte testen. Diese Version liest noch keine realen Sensoren und schaltet kein Relais.

## LVGL
Float-Textformatierung auf snprintf umgestellt; LV_COLOR_FORMAT_DEFAULT RGB565 gesetzt.
Die bekannte Treiberkorrektur im esp32_smartdisplay-Paket (ESP_ERR_NOT_SUPPORTED bei esp_lcd_panel_disp_on_off) ist noch nicht dauerhaft integriert. Falls das Paket frisch installiert wird, ist der bereits erprobte Patch erneut erforderlich. Diese Version wurde hier nicht auf dem Board kompiliert oder getestet.
