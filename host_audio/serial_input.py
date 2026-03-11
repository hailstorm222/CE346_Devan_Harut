from __future__ import annotations

from dataclasses import dataclass, field
import time
from typing import Any

import serial
from serial import SerialException


@dataclass
class Event:
    kind: str
    value: Any = None
    payload: dict[str, Any] = field(default_factory=dict)
    raw: str = ""


class SerialController:
    def __init__(self,
                 port: str,
                 baudrate: int = 38400,
                 timeout: float = 0.05,
                 reconnect_interval: float = 2.0) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.reconnect_interval = reconnect_interval
        self._serial: serial.Serial | None = None
        self._last_reconnect_attempt = 0.0
        self._connect()

    def _connect(self) -> None:
        now = time.monotonic()
        if now - self._last_reconnect_attempt < self.reconnect_interval:
            return

        self._last_reconnect_attempt = now
        try:
            self._serial = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
            self._serial.reset_input_buffer()
            print(f"[serial] connected to {self.port} @ {self.baudrate}")
        except SerialException as exc:
            self._serial = None
            print(f"[serial] waiting for controller on {self.port}: {exc}")

    def close(self) -> None:
        if self._serial is not None:
            self._serial.close()
            self._serial = None

    def read_event(self) -> Event | None:
        if self._serial is None:
            self._connect()
            return None

        try:
            raw = self._serial.readline()
        except SerialException as exc:
            print(f"[serial] disconnected: {exc}")
            self.close()
            return None

        if not raw:
            return None

        line = raw.decode("utf-8", errors="replace").strip()
        if not line:
            return None

        return self._parse_line(line)

    def _parse_line(self, line: str) -> Event | None:
        parts = line.split()
        if not parts:
            return None

        kind = parts[0]

        try:
            if kind in {"PAD_DOWN", "PAD_UP", "PAD_HOLD"} and len(parts) >= 2:
                return Event(kind=kind, value=int(parts[1]), raw=line)

            if kind == "ENC_DELTA" and len(parts) >= 2:
                return Event(kind=kind, value=int(parts[1]), raw=line)

            if kind == "ENC_BTN" and len(parts) >= 2:
                return Event(kind=kind, value=int(parts[1]), raw=line)

            if kind == "TOF" and len(parts) >= 2:
                return Event(kind=kind, value=int(parts[1]), raw=line)

            if kind == "HEARTBEAT":
                payload: dict[str, Any] = {}
                for field_text in parts[1:]:
                    key, _, value = field_text.partition("=")
                    if not key:
                        continue
                    payload[key] = value
                return Event(kind=kind, payload=payload, raw=line)
        except ValueError:
            if self._looks_garbled(line):
                return None
            print(f"[serial] dropped malformed line: {line}")
            return None

        if self._looks_garbled(line):
            return None

        if kind in {
            "CE346",
            "OLED_READY",
            "TOF_READY",
            "TOF_ID",
            "ENC_READY",
            "TOUCH_READY",
            "WARN",
            "ERROR",
        }:
            print(f"[serial] status: {line}")
            return None

        print(f"[serial] ignored line: {line}")
        return None

    @staticmethod
    def _looks_garbled(line: str) -> bool:
        if "�" in line:
            return True

        printable = sum(1 for ch in line if 32 <= ord(ch) <= 126)
        return printable < max(1, len(line) // 2)
