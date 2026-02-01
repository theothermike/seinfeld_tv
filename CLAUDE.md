# SeinfeldTV Development Guide

## Project Structure

- `converter/` - Python package for converting TV shows into SD card format
- `firmware/SeinfeldTV/` - Arduino firmware for TinyTV 2 hardware (RP2040)
- `firmware_upstream/` - Original TinyCircuits firmware (reference only)

## Python / Converter

### Environment

- Python venv at `.venv/` (Python 3.14)
- Activate: `.venv/bin/python` or `.venv/bin/pip`
- Do NOT use system `python` or `python3` — they don't have project deps

### Running Tests

```bash
.venv/bin/python -m pytest converter/tests/ -v
```

### Key Dependencies

- PIL/Pillow (thumbnail generation)
- ffmpeg (system binary, for video conversion and frame extraction)
- requests (metadata fetching)

## Firmware

### Toolchain

- arduino-cli at `/home/mike/.local/bin/arduino-cli`
- Board package: `rp2040:rp2040` v5.5.0 (Earle Philhower's arduino-pico)

### Compile

```bash
/home/mike/.local/bin/arduino-cli compile \
  --fqbn rp2040:rp2040:rpipico \
  --board-options "opt=Small,usbstack=tinyusb,freq=200" \
  /home/mike/dev/seinfeld_tv/firmware/SeinfeldTV/SeinfeldTV.ino
```

### Upload

The RP2040 needs to be in UF2 bootloader mode to flash. Three steps:

1. **Enter bootloader mode** — trigger 1200-baud reset on the serial port (requires sudo):
   ```bash
   sudo stty -F /dev/ttyACM0 1200
   ```
   The serial port will disappear. Wait a few seconds for the RPI-RP2 drive to appear.

2. **Mount the bootloader drive** (if not automounted):
   ```bash
   udisksctl mount -b /dev/sda1
   ```
   This mounts to `/media/mike/RPI-RP2`.

3. **Copy the UF2 file** — the device reboots automatically after copy:
   ```bash
   cp ~/.cache/arduino/sketches/1E7FA6852524608C517BCB1B47E06FED/SeinfeldTV.ino.uf2 /media/mike/RPI-RP2/
   ```

The compiled UF2 is cached at `~/.cache/arduino/sketches/1E7FA6852524608C517BCB1B47E06FED/SeinfeldTV.ino.uf2` after a successful compile.

Note: `arduino-cli upload -p /dev/ttyACM0` does NOT work for this board in bootloader mode since there's no serial port. Always use the UF2 copy method above.

### Hardware Target

- TinyTV 2 (Raspberry Pi Pico / RP2040)
- CPU Speed: 200MHz
- USB Stack: Adafruit TinyUSB
- Display: 210x135 RGB565

### Firmware Architecture

- `SeinfeldTV.ino` - Main state machine, setup, loop dispatchers
- `appState.h` - State enum, metadata structs, AppContext
- `showMetadata.ino` - SD card metadata loading, show/season scanning, settings, path helpers
- `showBrowser.ino` - UI rendering (show/season/episode browsers) and input handling
- `display.ino` - Low-level display/DMA, GraphicsBuffer2 setup
- `drawEffects.ino` - Static effect, tube-off animation
- `SD_AVI.ino` - AVI streaming/playback engine
- `audio.ino` - Audio buffer management
- `infrared.ino` - IR remote input

### SD Card Layout (Multi-Show)

```
/settings.txt           # Global settings (last_show, last_season, last_episode, volume)
/boot.avi               # Optional theme clip
/ShowName/              # Sanitized show directory
  show.sdb              # 128-byte show metadata
  show.raw              # 108x67 RGB565 title card thumbnail
  S01/
    season.sdb / thumb.raw
    E01.avi / E01.sdb / E01.raw
    E02.avi / E02.sdb / E02.raw
```

## Scripts

- `scripts/convert.sh` - Wrapper for converter: `./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/`
- `scripts/deploy.sh` - Deploy to TinyTV via USB MSC: `./scripts/deploy.sh --skip-convert`
- `scripts/flash.sh` - Compile + flash firmware: `./scripts/flash.sh [--compile-only] [--upload-only]`

## Conventions

- All .sdb files are binary structs (little-endian, packed) matching `appState.h` definitions
- All .raw files are 108x67 RGB565 big-endian thumbnails (14,472 bytes)
- Path buffers in firmware should be 64 bytes to accommodate show directory prefix
- Show directory names: alphanumeric + underscore, max 31 chars
