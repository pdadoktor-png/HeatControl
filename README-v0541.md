# HeatControl v0.5.4.1 – LVGL-Warnungen

Basis: v0.5.4. Nur vier veraltete LVGL-Aufrufe in der Touch-Kalibrierung ersetzt:

- `lv_obj_clear_flag(..., LV_OBJ_FLAG_SCROLLABLE)` → `lv_obj_set_scrollable(..., false)`
- `lv_obj_clear_flag(..., LV_OBJ_FLAG_HIDDEN)` → `lv_obj_set_hidden(..., false)`
- `lv_obj_add_flag(..., LV_OBJ_FLAG_HIDDEN)` → `lv_obj_set_hidden(..., true)` (2 Stellen)

Funktionalität der Kalibrierung bleibt gleich. Keine Hardwaretests oder vollständige Kompilierung durchgeführt.
