from __future__ import annotations

from dataclasses import dataclass
import threading

import numpy as np
import sounddevice as sd
import soundfile as sf


@dataclass
class Voice:
    slice_id: int
    start: int
    end: int
    position: int
    looping: bool


class AudioEngine:
    def __init__(self,
                 sample_rate: int = 44100,
                 block_size: int = 512,
                 channels: int = 2) -> None:
        self.sample_rate = sample_rate
        self.block_size = block_size
        self.channels = channels

        self._lock = threading.Lock()
        self._sample = np.zeros((1, channels), dtype=np.float32)
        # Per-pad clip bounds: (start_sample, end_sample)
        self._clip_bounds: list[tuple[int, int]] = []
        self._voices: list[Voice] = []
        # Effect chain and per-effect state for layering
        self._effect_names = ("filter", "delay", "reverb", "bitcrush")
        self._active_effect = self._effect_names[0]
        self._effect_states: dict[str, dict[str, float | bool]] = {
            name: {"depth": 0.0, "enabled": False}
            for name in self._effect_names
        }

        self._filter_state = np.zeros(channels, dtype=np.float32)
        self._delay_buffer = np.zeros((sample_rate, channels), dtype=np.float32)
        self._delay_index = 0
        self._reverb_buffer = np.zeros((sample_rate // 2, channels), dtype=np.float32)
        self._reverb_index = 0
        # Bitcrush is mostly stateless; no large buffers required.

        self._stream = sd.OutputStream(
            samplerate=self.sample_rate,
            blocksize=self.block_size,
            channels=self.channels,
            dtype="float32",
            callback=self._audio_callback,
        )

    def load_sample(self, path: str) -> None:
        data, file_sample_rate = sf.read(path, dtype="float32", always_2d=True)
        if file_sample_rate != self.sample_rate:
            raise ValueError(
                f"Expected {self.sample_rate} Hz sample, got {file_sample_rate} Hz"
            )

        if data.shape[1] < self.channels:
            data = np.repeat(data[:, :1], self.channels, axis=1)
        elif data.shape[1] > self.channels:
            data = data[:, :self.channels]

        # Default per-pad clips: pick 4 random ~1 second slices from the file.
        total_frames = data.shape[0]
        bounds: list[tuple[int, int]] = []
        clip_len = self.sample_rate  # ~1 second

        if total_frames <= clip_len * 2:
            # If the file is too short, fall back to 4 equal segments.
            slice_size = max(1, total_frames // 4)
            for pad_id in range(4):
                start = pad_id * slice_size
                end = total_frames if pad_id == 3 else min(total_frames, start + slice_size)
                bounds.append((start, max(start + 1, end)))
        else:
            max_start = max(0, total_frames - clip_len)
            for _ in range(4):
                start = int(np.random.randint(0, max_start + 1))
                end = start + clip_len
                bounds.append((start, end))

        with self._lock:
            self._sample = data
            self._clip_bounds = bounds
            self._voices.clear()
            self._reset_effect_buffers()

    def start(self) -> None:
        if not self._stream.active:
            self._stream.start()

    def stop(self) -> None:
        if self._stream.active:
            self._stream.stop()
        self._stream.close()

    def trigger_slice(self, slice_id: int) -> None:
        with self._lock:
            start, end = self._clip_bounds[slice_id]
            self._voices.append(Voice(slice_id=slice_id,
                                      start=start,
                                      end=end,
                                      position=start,
                                      looping=False))

    def loop_slice(self, slice_id: int, enabled: bool) -> None:
        with self._lock:
            if enabled:
                # If a looping voice already exists for this pad, keep it.
                if any(voice.slice_id == slice_id and voice.looping for voice in self._voices):
                    return
                start, end = self._clip_bounds[slice_id]
                self._voices.append(Voice(slice_id=slice_id,
                                          start=start,
                                          end=end,
                                          position=start,
                                          looping=True))
                return

            self._voices = [
                voice for voice in self._voices
                if not (voice.slice_id == slice_id and voice.looping)
            ]

    def get_total_frames(self) -> int:
        with self._lock:
            return int(self._sample.shape[0])

    def get_clip_bounds(self, pad_id: int) -> tuple[int, int] | None:
        with self._lock:
            if 0 <= pad_id < len(self._clip_bounds):
                return tuple(self._clip_bounds[pad_id])
            return None

    def set_clip(self, pad_id: int, start_sample: int, length_samples: int) -> None:
        """Set the clip for a pad by start sample and length. Clamps to file bounds."""
        with self._lock:
            if pad_id < 0 or pad_id >= len(self._clip_bounds):
                return
            total = int(self._sample.shape[0])
            start = max(0, min(start_sample, total - 1))
            length = max(1, min(length_samples, total - start))
            end = start + length
            self._clip_bounds[pad_id] = (start, end)

    def set_effect(self, effect_id: str) -> None:
        if effect_id not in self._effect_names:
            raise ValueError(f"Unsupported effect: {effect_id}")
        with self._lock:
            self._active_effect = effect_id

    def set_effect_depth(self, value_0_to_1: float) -> None:
        with self._lock:
            depth = float(np.clip(value_0_to_1, 0.0, 1.0))
            state = self._effect_states[self._active_effect]
            state["depth"] = depth

    def set_effect_enabled(self, enabled: bool) -> None:
        with self._lock:
            state = self._effect_states[self._active_effect]
            state["enabled"] = bool(enabled)

    def get_playing_pads(self) -> tuple[bool, bool, bool, bool]:
        """Return which pads currently have an active voice (playing or looping)."""
        with self._lock:
            out = [False, False, False, False]
            for v in self._voices:
                if 0 <= v.slice_id < 4:
                    out[v.slice_id] = True
            return tuple(out)

    def get_effect_states(self) -> dict[str, dict[str, float | bool]]:
        """Return a copy of effect name -> {depth, enabled} for display."""
        with self._lock:
            return {
                name: {"depth": s["depth"], "enabled": s["enabled"]}
                for name, s in self._effect_states.items()
            }

    def _reset_effect_buffers(self) -> None:
        self._filter_state.fill(0.0)
        self._delay_buffer.fill(0.0)
        self._delay_index = 0
        self._reverb_buffer.fill(0.0)
        self._reverb_index = 0

    def _audio_callback(self, outdata, frames, time_info, status) -> None:
        del time_info
        if status:
            print(f"[audio] stream status: {status}")

        with self._lock:
            mix = np.zeros((frames, self.channels), dtype=np.float32)
            active_voices: list[Voice] = []

            for voice in self._voices:
                frame_index = 0
                clip_len = voice.end - voice.start
                # Crossfade at loop boundary (e.g. ~15 ms) for smoother looping; skip if clip too short.
                crossfade_len = min(
                    int(self.sample_rate * 0.015),
                    max(0, clip_len // 2 - 1),
                ) if voice.looping and clip_len >= 32 else 0

                while frame_index < frames:
                    if voice.position >= voice.end:
                        if voice.looping:
                            voice.position = voice.start
                        else:
                            break

                    if (
                        voice.looping
                        and crossfade_len > 0
                        and voice.position >= voice.end - crossfade_len
                    ):
                        # Output crossfade: blend tail of clip with head for seamless loop.
                        tail_start = voice.end - crossfade_len
                        tail_offset = voice.position - tail_start
                        n = min(frames - frame_index, voice.end - voice.position)
                        if n <= 0:
                            voice.position = voice.start
                            continue
                        for i in range(n):
                            t = (i + 1) / (n + 1)
                            mix[frame_index + i] += (
                                (1.0 - t) * self._sample[voice.position + i]
                                + t * self._sample[voice.start + tail_offset + i]
                            )
                        voice.position = voice.start + tail_offset + n
                        frame_index += n
                        continue

                    chunk = min(frames - frame_index, voice.end - voice.position)
                    if crossfade_len > 0 and voice.looping and voice.position + chunk > voice.end - crossfade_len:
                        chunk = (voice.end - crossfade_len) - voice.position
                        if chunk <= 0:
                            continue
                    if chunk <= 0:
                        break

                    mix[frame_index:frame_index + chunk] += self._sample[
                        voice.position:voice.position + chunk
                    ]
                    voice.position += chunk
                    frame_index += chunk

                if voice.looping or voice.position < voice.end:
                    active_voices.append(voice)

            self._voices = active_voices
            effected = self._apply_effect_chain(mix)

        outdata[:] = np.clip(effected, -1.0, 1.0)

    def _apply_effect_chain(self, mix: np.ndarray) -> np.ndarray:
        """Apply all enabled effects in a fixed chain for layering."""
        out = mix
        for name in self._effect_names:
            state = self._effect_states[name]
            depth = float(state["depth"])
            if (not state["enabled"]) or depth <= 0.001:
                continue
            if name == "filter":
                out = self._apply_filter(out, depth)
            elif name == "delay":
                out = self._apply_delay(out, depth)
            elif name == "reverb":
                out = self._apply_reverb(out, depth)
            elif name == "bitcrush":
                out = self._apply_bitcrush(out, depth)
        return out

    def _apply_filter(self, mix: np.ndarray, depth: float) -> np.ndarray:
        """Overdrive/saturation filter - more noticeable than subtle low-pass."""
        # Map depth 0..1 to drive amount: gentle saturation to heavy distortion
        drive = 1.0 + 8.0 * depth  # 1x to 9x gain before clipping
        # Soft-clip using tanh for warm overdrive
        driven = np.tanh(mix * drive)
        # Blend dry/wet: more depth = more saturated sound
        wet = 0.3 + 0.7 * depth
        return (1.0 - wet) * mix + wet * driven

    def _apply_delay(self, mix: np.ndarray, depth: float) -> np.ndarray:
        """Vectorized delay effect for better performance."""
        wet = 0.15 + 0.55 * depth
        feedback = 0.15 + 0.45 * depth
        delay_samples = int(self.sample_rate * (0.08 + 0.24 * depth))
        n_frames = mix.shape[0]
        buf_len = self._delay_buffer.shape[0]

        # Build index arrays for reading from delay buffer
        start_idx = self._delay_index
        read_indices = (np.arange(n_frames) + start_idx - delay_samples) % buf_len
        write_indices = (np.arange(n_frames) + start_idx) % buf_len

        # Read delayed samples
        delayed = self._delay_buffer[read_indices]

        # Compute output
        out = (1.0 - wet) * mix + wet * delayed

        # Write to delay buffer (mix + feedback * delayed)
        self._delay_buffer[write_indices] = mix + feedback * delayed

        # Update index
        self._delay_index = (start_idx + n_frames) % buf_len
        return out

    def _apply_reverb(self, mix: np.ndarray, depth: float) -> np.ndarray:
        """Vectorized reverb effect for better performance."""
        wet = 0.1 + 0.35 * depth
        feedback = 0.2 + 0.25 * depth
        tap_a = int(self.sample_rate * 0.031)
        tap_b = int(self.sample_rate * 0.047)
        tap_c = int(self.sample_rate * 0.071)
        n_frames = mix.shape[0]
        buf_len = self._reverb_buffer.shape[0]
        start_idx = self._reverb_index

        # Build index arrays for the three delay taps
        base_indices = np.arange(n_frames) + start_idx
        indices_a = (base_indices - tap_a) % buf_len
        indices_b = (base_indices - tap_b) % buf_len
        indices_c = (base_indices - tap_c) % buf_len
        write_indices = base_indices % buf_len

        # Read from reverb buffer
        delayed_a = self._reverb_buffer[indices_a]
        delayed_b = self._reverb_buffer[indices_b]
        delayed_c = self._reverb_buffer[indices_c]

        # Combine taps
        reverb_mix = 0.5 * delayed_a + 0.3 * delayed_b + 0.2 * delayed_c

        # Compute output
        out = (1.0 - wet) * mix + wet * reverb_mix

        # Write to reverb buffer
        self._reverb_buffer[write_indices] = mix + feedback * reverb_mix

        # Update index
        self._reverb_index = (start_idx + n_frames) % buf_len
        return out

    def _apply_bitcrush(self, mix: np.ndarray, depth: float) -> np.ndarray:
        """Simple bitcrusher: reduce amplitude resolution based on depth."""
        # Map depth 0..1 to bit depth ~16 -> ~4
        bits = int(round(16 - 12 * depth))
        bits = max(4, min(16, bits))
        levels = float(2 ** bits)
        # Quantize around zero; this is intentionally lo-fi.
        out = np.round(mix * levels) / levels
        return out
