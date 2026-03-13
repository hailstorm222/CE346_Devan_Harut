# Sensor usage (air sampler)

How each sensor is wired, read, and used so behavior stays consistent with the host (`run_sampler.py`).

## TOF (VL53L0X) – distance → effect depth

- **Hardware**: I2C on Qwiic bus (P19/P20), address `0x29`.
- **Driver**: `vl53l0x_min.c` – single-shot ranging; result from registers 0x1E–0x1F (bytes 10–11 of 12-byte read from 0x14). **Byte order**: MSB = `buf[10]`, LSB = `buf[11]` ⇒ `raw = (buf[10]<<8)|buf[11]`. Do not swap; 8190 = no target (reported as 2000 mm then clamped).
- **Main loop**: If present, each loop calls `vl53l0x_read_distance_mm()` (blocks ~30 ms), clamps to **50–400 mm**, smooths with `TOF_SMOOTH_SHIFT` (2), emits `TOF <mm>` when change ≥ `TOF_REPORT_STEP_MM` (15) or on first valid. Also sent in HEARTBEAT.
- **Host**: `controller_mapping._map_tof_to_depth()` clips to 50–400 and maps to depth (close = 1.0, far = 0.0) for `set_effect_depth()`.

## Touch (4 pads, P1–P4)

- **Hardware**: `hw_config.h`: `TOUCH_PIN_0..3` = EDGE_P1..P4; `NUM_TOUCH_PADS` = 4.
- **Driver**: `touch_sensor.c` – GPIO input, pull-down, active-high; `touch_sensor_read_array()` fills a `bool[]`.
- **Main loop**: Every loop: `touch_sensor_read_array()`, then edge/hold logic: **PAD_DOWN** on press, **PAD_HOLD** after 250 ms hold, **PAD_UP** on release. States also in HEARTBEAT and OLED.
- **Host**: PAD_DOWN → `trigger_slice(pad_id)`, PAD_HOLD → `loop_slice(pad_id, True)`, PAD_UP → `loop_slice(pad_id, False)`.

## Rotary encoder (P8, P9, P12)

- **Hardware**: `hw_config.h`: ENC_PIN_A = P8, ENC_PIN_B = P9, ENC_PIN_SW = P12 (button). 24 detents.
- **Driver**: `rotary_encoder.c` – **rotation** is interrupt-driven (GPIOTE on A/B, quadrature table). **Button** is polled in `rotary_encoder_poll()`.
- **Main loop**: `rotary_encoder_poll()` then `rotary_encoder_get_position()`; delta drives **ENC_DELTA** and effect index. Button change → **ENC_BTN** and effect enabled toggle.
- **Host**: ENC_DELTA → change effect (filter/delay/reverb/stutter); ENC_BTN 1 → toggle effect enabled.

## OLED (I2C 0x3D / 0x3C)

- **Usage**: Status only: effect name + ON/OFF, TOF mm or "NO DATA", touch 1–4 and encoder button, "USB CTRL READY". No control protocol events.

## Consistency checklist

- TOF: 50–400 mm in firmware and host; same clamp and depth mapping.
- Touch: pad IDs 0–3; PAD_DOWN / PAD_HOLD / PAD_UP; 250 ms for hold.
- Encoder: ENC_DELTA (signed), ENC_BTN 0/1; 4 effects, 24 detents, wrap.
