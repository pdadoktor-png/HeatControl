# Permanent, idempotent workaround for esp32_smartdisplay RGB panels.
# Runs on every PlatformIO build; never edit .pio by hand.
from pathlib import Path
Import("env")

libdeps = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
header = libdeps / env["PIOENV"] / "esp32_smartdisplay" / "include" / "lvgl_panel_common.h"
old = "ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));"
new = """do {
        esp_err_t heatcontrol_disp_err = esp_lcd_panel_disp_on_off(panel_handle, true);
        if (heatcontrol_disp_err != ESP_OK && heatcontrol_disp_err != ESP_ERR_NOT_SUPPORTED) {
            ESP_ERROR_CHECK(heatcontrol_disp_err);
        }
    } while (0);"""

def apply_patch(*args, **kwargs):
    if not header.is_file():
        print("[HeatControl] Smartdisplay header not installed yet:", header)
        return
    content = header.read_text(encoding="utf-8")
    if new in content:
        print("[HeatControl] Display patch already active")
        return
    if old not in content:
        raise RuntimeError("[HeatControl] Smartdisplay header changed; verify driver before flashing: " + str(header))
    header.write_text(content.replace(old, new, 1), encoding="utf-8")
    print("[HeatControl] Display ESP_ERR_NOT_SUPPORTED patch applied")

apply_patch()
# Also run immediately before C++ compilation if libdeps were populated later.
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", apply_patch)
