# Running the app on the micro:bit v2

## Prerequisites

1. **micro:bit v2** connected to your Mac via **USB** (use the USB port on the micro:bit).
2. **OpenOCD** installed (e.g. `brew install openocd`). The project uses OpenOCD with CMSIS-DAP (the micro:bit v2’s built-in debugger).

## Build and flash

From the **app directory** (this folder):

```bash
cd /path/to/nu-microbit-base/software/apps/CE346_Devan_Harut
make flash
```

- This runs `make all` (builds the `.hex`), then programs it to the board and resets the device.
- The HEX file is: `_build/CE346_Devan_Harut_sdk16_blank.hex`.

## If `make flash` fails

1. **“Error: unable to find a matching CMSIS-DAP device”**  
   - Check the USB cable and port; try another cable/port.  
   - Unplug and replug the micro:bit; wait a few seconds and run `make flash` again.

2. **“openocd: command not found”**  
   - Install OpenOCD: `brew install openocd`.

3. **Permission denied on USB**  
   - On Linux you may need udev rules or to run with `sudo` (not typical on macOS).

## Viewing serial output

After flashing, the app uses **UART** for `printf` (e.g. TOF, encoder, touch). To see it:

- Use a **serial terminal** at **115200 baud** on the micro:bit’s serial port (the same USB connection often exposes a CDC ACM serial port, e.g. `/dev/cu.usbmodem*` on macOS).
- Or use a **USB‑serial adapter** if you’re using the edge connector’s UART pins (TXD/RXD) instead of USB CDC.

## Build only (no flash)

```bash
make
```

## Clean build

```bash
make clean
make flash
```
