from __future__ import annotations

from host_audio.audio_engine import AudioEngine
from host_audio.serial_input import Event


class ControllerMapper:
    EFFECTS = ("filter", "delay", "reverb", "stutter")

    def __init__(self) -> None:
        self.effect_index = 0
        self.effect_enabled = True

    def handle_event(self, event: Event, engine: AudioEngine) -> None:
        if event.kind == "PAD_DOWN":
            engine.trigger_slice(int(event.value))
            print(f"[control] trigger slice {event.value}")
            return

        if event.kind == "PAD_HOLD":
            engine.loop_slice(int(event.value), True)
            print(f"[control] loop slice {event.value} on")
            return

        if event.kind == "PAD_UP":
            engine.loop_slice(int(event.value), False)
            print(f"[control] loop slice {event.value} off")
            return

        if event.kind == "ENC_DELTA":
            delta = int(event.value)
            self.effect_index = (self.effect_index + delta) % len(self.EFFECTS)
            effect_name = self.EFFECTS[self.effect_index]
            engine.set_effect(effect_name)
            print(f"[control] effect -> {effect_name}")
            return

        if event.kind == "ENC_BTN" and int(event.value) == 1:
            self.effect_enabled = not self.effect_enabled
            engine.set_effect_enabled(self.effect_enabled)
            print(f"[control] effect enabled -> {self.effect_enabled}")
            return

        if event.kind == "TOF":
            depth = self._map_tof_to_depth(int(event.value))
            engine.set_effect_depth(depth)
            print(f"[control] depth -> {depth:.2f}")
            return

        if event.kind == "HEARTBEAT":
            print(f"[control] heartbeat {event.payload}")

    @staticmethod
    def _map_tof_to_depth(distance_mm: int) -> float:
        clipped = min(max(distance_mm, 50), 400)
        normalized = (clipped - 50) / 350.0
        return 1.0 - normalized
