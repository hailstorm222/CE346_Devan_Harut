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
        self._slice_bounds: list[tuple[int, int]] = []
        self._voices: list[Voice] = []
        self._effect_names = ("filter", "delay", "reverb", "stutter")
        self._effect_name = self._effect_names[0]
        self._effect_depth = 0.0
        self._effect_enabled = True

        self._filter_state = np.zeros(channels, dtype=np.float32)
        self._delay_buffer = np.zeros((sample_rate, channels), dtype=np.float32)
        self._delay_index = 0
        self._reverb_buffer = np.zeros((sample_rate // 2, channels), dtype=np.float32)
        self._reverb_index = 0
        self._stutter_capture = np.zeros((block_size, channels), dtype=np.float32)
        self._stutter_length = max(1, block_size // 4)
        self._stutter_index = 0

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

        total_frames = data.shape[0]
        slice_size = max(1, total_frames // 4)
        bounds: list[tuple[int, int]] = []
        for slice_id in range(4):
            start = slice_id * slice_size
            end = total_frames if slice_id == 3 else min(total_frames, start + slice_size)
            bounds.append((start, max(start + 1, end)))

        with self._lock:
            self._sample = data
            self._slice_bounds = bounds
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
            start, end = self._slice_bounds[slice_id]
            self._voices.append(Voice(slice_id=slice_id,
                                      start=start,
                                      end=end,
                                      position=start,
                                      looping=False))

    def loop_slice(self, slice_id: int, enabled: bool) -> None:
        with self._lock:
            if enabled:
                if any(voice.slice_id == slice_id and voice.looping for voice in self._voices):
                    return
                start, end = self._slice_bounds[slice_id]
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

    def set_effect(self, effect_id: str) -> None:
        if effect_id not in self._effect_names:
            raise ValueError(f"Unsupported effect: {effect_id}")
        with self._lock:
            self._effect_name = effect_id

    def set_effect_depth(self, value_0_to_1: float) -> None:
        with self._lock:
            self._effect_depth = float(np.clip(value_0_to_1, 0.0, 1.0))

    def set_effect_enabled(self, enabled: bool) -> None:
        with self._lock:
            self._effect_enabled = enabled

    def _reset_effect_buffers(self) -> None:
        self._filter_state.fill(0.0)
        self._delay_buffer.fill(0.0)
        self._delay_index = 0
        self._reverb_buffer.fill(0.0)
        self._reverb_index = 0
        self._stutter_capture.fill(0.0)
        self._stutter_index = 0

    def _audio_callback(self, outdata, frames, time_info, status) -> None:
        del time_info
        if status:
            print(f"[audio] stream status: {status}")

        with self._lock:
            mix = np.zeros((frames, self.channels), dtype=np.float32)
            active_voices: list[Voice] = []

            for voice in self._voices:
                frame_index = 0
                while frame_index < frames:
                    if voice.position >= voice.end:
                        if voice.looping:
                            voice.position = voice.start
                        else:
                            break

                    chunk = min(frames - frame_index, voice.end - voice.position)
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
            effected = self._apply_effect(mix)

        outdata[:] = np.clip(effected, -1.0, 1.0)

    def _apply_effect(self, mix: np.ndarray) -> np.ndarray:
        if (not self._effect_enabled) or (self._effect_depth <= 0.001):
            return mix

        if self._effect_name == "filter":
            return self._apply_filter(mix)
        if self._effect_name == "delay":
            return self._apply_delay(mix)
        if self._effect_name == "reverb":
            return self._apply_reverb(mix)
        if self._effect_name == "stutter":
            return self._apply_stutter(mix)
        return mix

    def _apply_filter(self, mix: np.ndarray) -> np.ndarray:
        alpha = 0.5 - (0.42 * self._effect_depth)
        alpha = float(np.clip(alpha, 0.04, 0.5))
        out = np.empty_like(mix)
        state = self._filter_state.copy()

        for i in range(mix.shape[0]):
            state = state + alpha * (mix[i] - state)
            out[i] = state

        self._filter_state = state
        dry = 1.0 - (0.35 * self._effect_depth)
        wet = 0.3 + (0.7 * self._effect_depth)
        return (dry * mix) + (wet * out)

    def _apply_delay(self, mix: np.ndarray) -> np.ndarray:
        wet = 0.15 + (0.55 * self._effect_depth)
        feedback = 0.15 + (0.45 * self._effect_depth)
        delay_samples = int(self.sample_rate * (0.08 + (0.24 * self._effect_depth)))
        out = np.copy(mix)

        for i in range(mix.shape[0]):
            read_index = (self._delay_index - delay_samples) % self._delay_buffer.shape[0]
            delayed = self._delay_buffer[read_index]
            out[i] = (1.0 - wet) * mix[i] + wet * delayed
            self._delay_buffer[self._delay_index] = mix[i] + feedback * delayed
            self._delay_index = (self._delay_index + 1) % self._delay_buffer.shape[0]

        return out

    def _apply_reverb(self, mix: np.ndarray) -> np.ndarray:
        wet = 0.1 + (0.35 * self._effect_depth)
        feedback = 0.2 + (0.25 * self._effect_depth)
        tap_a = int(self.sample_rate * 0.031)
        tap_b = int(self.sample_rate * 0.047)
        tap_c = int(self.sample_rate * 0.071)
        out = np.copy(mix)

        for i in range(mix.shape[0]):
            delayed_a = self._reverb_buffer[(self._reverb_index - tap_a) % self._reverb_buffer.shape[0]]
            delayed_b = self._reverb_buffer[(self._reverb_index - tap_b) % self._reverb_buffer.shape[0]]
            delayed_c = self._reverb_buffer[(self._reverb_index - tap_c) % self._reverb_buffer.shape[0]]
            reverb_mix = (0.5 * delayed_a) + (0.3 * delayed_b) + (0.2 * delayed_c)
            out[i] = (1.0 - wet) * mix[i] + wet * reverb_mix
            self._reverb_buffer[self._reverb_index] = mix[i] + feedback * reverb_mix
            self._reverb_index = (self._reverb_index + 1) % self._reverb_buffer.shape[0]

        return out

    def _apply_stutter(self, mix: np.ndarray) -> np.ndarray:
        capture_length = int(self.block_size * (0.25 + (0.75 * self._effect_depth)))
        capture_length = max(1, min(capture_length, mix.shape[0]))

        if np.max(np.abs(mix)) > 0.0005:
            self._stutter_capture[:capture_length] = mix[:capture_length]
            self._stutter_length = capture_length
            self._stutter_index = 0

        out = np.empty_like(mix)
        for i in range(mix.shape[0]):
            out[i] = self._stutter_capture[self._stutter_index]
            self._stutter_index = (self._stutter_index + 1) % self._stutter_length

        dry = 0.35
        wet = 0.65
        return (dry * mix) + (wet * out)
