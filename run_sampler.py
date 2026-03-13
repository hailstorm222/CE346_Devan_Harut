from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

# Allow running from any directory: add this app folder to path so host_audio is found
_APP_DIR = Path(__file__).resolve().parent
if str(_APP_DIR) not in sys.path:
    sys.path.insert(0, str(_APP_DIR))

from host_audio.audio_engine import AudioEngine
from host_audio.controller_mapping import ControllerMapper
from host_audio.serial_input import SerialController


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Air sampler mixer host app")
    parser.add_argument("--port", required=True, help="USB serial port for the micro:bit")
    parser.add_argument("--sample", required=True, help="Path to a 44.1 kHz audio file")
    parser.add_argument("--baudrate", type=int, default=38400, help="Serial baudrate")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    engine = AudioEngine()
    try:
        engine.load_sample(args.sample)
    except FileNotFoundError:
        print(f"[host] error: sample file not found: {args.sample}")
        sys.exit(1)
    except ValueError as e:
        print(f"[host] error: {e}")
        sys.exit(1)
    engine.start()
    engine.set_effect("filter")

    mapper = ControllerMapper()
    serial_controller = SerialController(args.port, baudrate=args.baudrate)

    print("[host] ready")
    try:
        while True:
            event = serial_controller.read_event()
            if event is None:
                time.sleep(0.005)
                continue
            mapper.handle_event(event, engine)
    except KeyboardInterrupt:
        print("\n[host] stopping")
    finally:
        serial_controller.close()
        engine.stop()


if __name__ == "__main__":
    main()
