# SeinfeldTV

A multi-show video player for the [TinyTV 2](https://tinycircuits.com/products/tinytv-2) hardware. Converts TV episodes into a compact AVI format and plays them on the tiny 210x135 screen with audio, show browsing UI, and IR remote control.

## Requirements

- **Python 3.14+** with venv
- **ffmpeg** (system binary)
- **Pillow** (`pip install Pillow`)
- **arduino-cli** (for firmware compilation/flashing)
- **RP2040 board package** (`rp2040:rp2040` v5.5.0)

## Quick Start

### 1. Set up Python environment

```bash
python3.14 -m venv .venv
.venv/bin/pip install -e .
```

### 2. Convert episodes

```bash
./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/
```

### 3. Flash firmware

```bash
./scripts/flash.sh
```

### 4. Deploy to device

Put the TinyTV in USB Mass Storage mode, then:

```bash
./scripts/deploy.sh --skip-convert
```

## Usage

### Converting Shows

```bash
# Convert all episodes of a show
./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/

# Convert a specific season
./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/ --season 3

# Convert specific episodes with TMDB artwork
./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/ \
    --season 4 --episodes 1-5 --tmdb-key YOUR_API_KEY

# Custom quality and frame rate
./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/ --quality 4 --fps 20

# Add a boot clip (5-10 second theme music)
./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/ --boot-clip ~/theme.mp4
```

Output goes to `sdcard_test/` by default. Override with `--output-dir`.

### Multi-Show Setup

Run the converter multiple times with different shows. Each show gets its own directory on the SD card:

```bash
./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/
./scripts/convert.sh --show "The Office" --input ~/tv/TheOffice/
./scripts/convert.sh --show "Friends" --input ~/tv/Friends/
```

### Firmware

```bash
# Compile and flash
./scripts/flash.sh

# Compile only (no flash)
./scripts/flash.sh --compile-only

# Flash previously compiled firmware
./scripts/flash.sh --upload-only
```

### Deploying to Device

```bash
# Deploy existing converted files
./scripts/deploy.sh --skip-convert

# Convert and deploy in one step
./scripts/deploy.sh --show "Seinfeld" --input ~/tv/Seinfeld/

# Deploy only a single show (faster)
./scripts/deploy.sh --skip-convert --show-only Seinfeld
```

The device must be in USB Mass Storage mode and mounted at `/media/mike/disk`.

## SD Card Layout

```
/settings.txt           # Global settings (last_show, last_season, last_episode, volume)
/boot.avi               # Optional theme clip
/ShowName/              # Sanitized show directory
  show.sdb              # Show metadata (128 bytes, binary)
  show.raw              # Title card thumbnail (108x67 RGB565)
  S01/
    season.sdb / thumb.raw
    E01.avi / E01.sdb / E01.raw
    E02.avi / E02.sdb / E02.raw
```

## Development

### Project Structure

- `converter/` - Python package for converting TV shows into SD card format
- `firmware/SeinfeldTV/` - Arduino firmware for TinyTV 2 (RP2040)
- `scripts/` - Wrapper scripts for convert, deploy, and flash workflows

### Running Tests

```bash
.venv/bin/python -m pytest converter/tests/ -v
```
