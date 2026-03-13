# Host Audio Runner

## Setup

1. Install Python host dependencies (from this directory or repo root):

   ```bash
   python3 -m pip install -r requirements-host.txt
   ```

2. Build and flash the micro:bit firmware so the board sends control events over USB serial.

3. Connect the micro:bit over USB and note the serial port:
   - **macOS**: e.g. `/dev/cu.usbmodem101` or `/dev/tty.usbmodem101` (list with `ls /dev/cu.usb*`).
   - **Linux**: e.g. `/dev/ttyACM0`.
   - **Windows**: e.g. `COM3`.

## Run

You can run from **any directory**; the script finds the `host_audio` package automatically.

```bash
python3 run_sampler.py --port /dev/cu.usbmodem101 --sample path/to/sample.wav
```

Use your actual port and a **44.1 kHz** WAV file. Baud rate is 38400 by default (matches the micro:bit).

## Controls (from firmware)

- **Touch pads (P1–P4)**: pad down = trigger slice; hold = loop on; release = loop off.
- **Rotary encoder**: turn = change effect (filter / delay / reverb / stutter); button = toggle effect on/off.
- **TOF (distance)**: 50–400 mm maps to effect depth.

## Troubleshooting

- **"Sample file not found"** – Use an absolute or correct relative path to a WAV file.
- **"Sample rate must be 44100"** – Convert your WAV to 44.1 kHz (e.g. with Audacity or `ffmpeg`).
- **"[serial] waiting for controller..."** – Wrong port, port in use (close other serial terminals/IDEs), or micro:bit not flashed. Unplug/replug and check port with `ls /dev/cu.usb*` (Mac) or Device Manager (Windows).
- **Garbled or no events** – Baud mismatch; keep default 38400 on both host and firmware.
- **ImportError: No module named 'host_audio'** – You should be able to run from any directory after the path fix; if it persists, run from the app directory: `cd nu-microbit-base/software/apps/CE346_Devan_Harut` then run the command above.
