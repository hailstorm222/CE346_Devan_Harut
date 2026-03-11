# Host Audio Runner

Install host dependencies:

```bash
python3 -m pip install -r requirements-host.txt
```

Run the Python audio engine:

```bash
python3 run_sampler.py --port /dev/tty.usbmodemXXXX --sample path/to/sample.wav
```

Notes:
- The sample file should be 44.1 kHz for the current implementation.
- The micro:bit serial link in this repo uses `38400` baud by default.
- The firmware sends plain-text USB serial messages such as `PAD_DOWN 0`, `ENC_DELTA 1`, and `TOF 120`.
- Touch pads trigger slices immediately, `PAD_HOLD` enables looping, and `PAD_UP` disables looping.
