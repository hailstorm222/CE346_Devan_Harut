from __future__ import annotations

from host_audio.audio_engine import AudioEngine
from host_audio.serial_input import Event

# Encoder slots: 0–3 = effects, 4 = slice-edit mode.
EFFECTS = ("filter", "delay", "reverb", "bitcrush")
ENCODER_SLOTS = (*EFFECTS, "slice_edit")

# Slice length presets (ms); TOF maps 50–400 mm to these.
SLICE_LENGTH_MS = (250, 500, 750, 1000)


class ControllerMapper:
    EFFECTS = EFFECTS

    def __init__(self) -> None:
        # Encoder slot 0–4: 0–3 = effect, 4 = SLICE (must press button to enter edit).
        self._slot_index = 0
        self._last_effect_slot = 0
        # True only after user presses encoder button while on SLICE slot.
        self._slice_edit_mode = False
        # Per-effect enabled flags for layering.
        self._effect_enabled: dict[str, bool] = {
            name: False for name in EFFECTS
        }
        # Per-pad loop state: True if pad is currently looping (play mode only).
        self._pad_looping: list[bool] = [False, False, False, False]
        # Slice-edit: which pad is being edited (0–3).
        self._edit_pad = 0
        # Slice length preset index per pad (0–3 for 250/500/750/1000 ms).
        self._slice_length_index: list[int] = [1, 1, 1, 1]

    @property
    def effect_index(self) -> int:
        return min(self._slot_index, len(EFFECTS) - 1)

    @property
    def is_slice_edit_mode(self) -> bool:
        return self._slice_edit_mode

    def handle_event(self, event: Event, engine: AudioEngine) -> None:
        if event.kind == "PAD_DOWN":
            self._handle_pad_down(int(event.value), engine)
            return
        if event.kind == "PAD_HOLD":
            self._handle_pad_hold(int(event.value), engine)
            return
        if event.kind == "PAD_UP":
            return
        if event.kind == "ENC_DELTA":
            self._handle_encoder_delta(int(event.value), engine)
            return
        if event.kind == "ENC_BTN" and int(event.value) == 1:
            self._handle_encoder_button(engine)
            return
        if event.kind == "TOF":
            self._handle_tof(int(event.value), engine)
            return
        if event.kind == "HEARTBEAT":
            print(f"[control] heartbeat {event.payload}")

    def _handle_pad_down(self, pad: int, engine: AudioEngine) -> None:
        if pad < 0 or pad > 3:
            return
        if self.is_slice_edit_mode:
            if pad == self._edit_pad:
                engine.trigger_slice(pad)
                print(f"[control] slice preview pad {pad}")
            else:
                self._edit_pad = pad
                print(f"[control] slice edit pad -> {pad}")
            return
        # Play mode
        if self._pad_looping[pad]:
            engine.loop_slice(pad, False)
            self._pad_looping[pad] = False
            print(f"[control] loop slice {pad} off")
        else:
            engine.trigger_slice(pad)
            print(f"[control] trigger slice {pad}")

    def _handle_pad_hold(self, pad: int, engine: AudioEngine) -> None:
        if self.is_slice_edit_mode:
            # In slice-edit, hold can also select the pad (optional).
            if pad != self._edit_pad:
                self._edit_pad = pad
                print(f"[control] slice edit pad -> {pad}")
            return
        if 0 <= pad < len(self._pad_looping) and not self._pad_looping[pad]:
            engine.loop_slice(pad, True)
            self._pad_looping[pad] = True
            print(f"[control] loop slice {pad} on")

    def _handle_encoder_delta(self, delta: int, engine: AudioEngine) -> None:
        if self.is_slice_edit_mode:
            self._slice_edit_move_start(delta, engine)
            return
        # Play mode: cycle effect slot (0–4). Encoder rotation changes slot.
        self._slot_index = (self._slot_index + delta) % len(ENCODER_SLOTS)
        slot_name = ENCODER_SLOTS[self._slot_index]
        if slot_name != "slice_edit":
            self._last_effect_slot = self._slot_index
            engine.set_effect(slot_name)
        print(f"[control] slot -> {slot_name}")

    def _slice_edit_move_start(self, delta: int, engine: AudioEngine) -> None:
        bounds = engine.get_clip_bounds(self._edit_pad)
        if not bounds:
            return
        start, end = bounds
        length = end - start
        total = engine.get_total_frames()
        step = length  # step size = current slice length
        new_start = start + delta * step
        new_start = max(0, min(new_start, total - length))
        engine.set_clip(self._edit_pad, new_start, length)
        print(f"[control] slice pad {self._edit_pad} start -> {new_start} (len={length})")

    def _handle_encoder_button(self, engine: AudioEngine) -> None:
        if self.is_slice_edit_mode:
            # Exit slice-edit: back to last effect slot.
            self._slice_edit_mode = False
            self._slot_index = self._last_effect_slot
            slot_name = ENCODER_SLOTS[self._slot_index]
            engine.set_effect(slot_name)
            print(f"[control] exit slice_edit -> {slot_name}")
            return
        # On SLICE slot (index 4): press enters slice-edit; no effect toggle.
        if ENCODER_SLOTS[self._slot_index] == "slice_edit":
            self._slice_edit_mode = True
            print(f"[control] enter slice_edit")
            return
        # Effect slot: toggle that effect on/off.
        effect_name = ENCODER_SLOTS[self._slot_index]
        current = self._effect_enabled[effect_name]
        new_state = not current
        self._effect_enabled[effect_name] = new_state
        engine.set_effect(effect_name)
        engine.set_effect_enabled(new_state)
        print(f"[control] effect {effect_name} enabled -> {new_state}")

    def _handle_tof(self, distance_mm: int, engine: AudioEngine) -> None:
        if self.is_slice_edit_mode:
            self._slice_edit_set_length(distance_mm, engine)
            return
        effect_name = ENCODER_SLOTS[self._slot_index]
        if effect_name == "slice_edit":
            return
        depth = self._map_tof_to_depth(distance_mm)
        engine.set_effect(effect_name)
        engine.set_effect_depth(depth)
        print(f"[control] depth[{effect_name}] -> {depth:.2f}")

    def _slice_edit_set_length(self, distance_mm: int, engine: AudioEngine) -> None:
        # Map 50–400 mm to preset index 0–3 (250, 500, 750, 1000 ms).
        clipped = min(max(distance_mm, 50), 400)
        t = (clipped - 50) / 350.0
        idx = min(3, int(t * 4)) if t < 1.0 else 3
        if idx == self._slice_length_index[self._edit_pad]:
            return
        self._slice_length_index[self._edit_pad] = idx
        ms = SLICE_LENGTH_MS[idx]
        length_samples = int(engine.sample_rate * ms / 1000.0)
        bounds = engine.get_clip_bounds(self._edit_pad)
        if not bounds:
            return
        start, _ = bounds
        engine.set_clip(self._edit_pad, start, length_samples)
        print(f"[control] slice pad {self._edit_pad} length -> {ms} ms")

    @staticmethod
    def _map_tof_to_depth(distance_mm: int) -> float:
        clipped = min(max(distance_mm, 50), 400)
        normalized = (clipped - 50) / 350.0
        return 1.0 - normalized
