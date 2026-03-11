from __future__ import annotations

import argparse
import time

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
    engine.load_sample(args.sample)
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
