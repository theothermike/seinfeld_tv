#!/usr/bin/env bash
#
# flash.sh - Compile and upload firmware to TinyTV 2
#
# Usage:
#   ./scripts/flash.sh                # Compile + flash
#   ./scripts/flash.sh --compile-only # Just compile
#   ./scripts/flash.sh --upload-only  # Flash existing UF2
#
set -euo pipefail

ARDUINO_CLI="/home/mike/.local/bin/arduino-cli"
FQBN="rp2040:rp2040:rpipico"
BOARD_OPTIONS="opt=Small,usbstack=tinyusb,freq=200"
SKETCH="/home/mike/dev/seinfeld_tv/firmware/TinyJukebox/TinyJukebox.ino"
UF2_PATH="$HOME/.cache/arduino/sketches/1E7FA6852524608C517BCB1B47E06FED/TinyJukebox.ino.uf2"
SERIAL_PORT="/dev/ttyACM0"
BOOT_MOUNT="/media/mike/RPI-RP2"
BOOT_DEVICE="/dev/sda1"

COMPILE=true
UPLOAD=true

while [[ $# -gt 0 ]]; do
    case "$1" in
        --compile-only)
            UPLOAD=false
            shift
            ;;
        --upload-only)
            COMPILE=false
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--compile-only] [--upload-only]"
            exit 1
            ;;
    esac
done

# Step 1: Compile
if [[ "$COMPILE" == true ]]; then
    echo "=== Compiling firmware ==="
    "$ARDUINO_CLI" compile \
        --fqbn "$FQBN" \
        --board-options "$BOARD_OPTIONS" \
        "$SKETCH"
    echo ""
    echo "Compiled successfully. UF2 at: $UF2_PATH"
    echo ""
fi

if [[ "$UPLOAD" == false ]]; then
    exit 0
fi

# Step 2: Verify UF2 exists
if [[ ! -f "$UF2_PATH" ]]; then
    echo "Error: UF2 file not found at $UF2_PATH"
    echo "Run with --compile-only first, or without --upload-only."
    exit 1
fi

# Step 3: Enter bootloader mode
echo "=== Entering bootloader mode ==="
if [[ -e "$SERIAL_PORT" ]]; then
    echo "Triggering 1200-baud reset on $SERIAL_PORT (requires sudo)..."
    sudo stty -F "$SERIAL_PORT" 1200
    echo "Waiting for bootloader drive..."
else
    echo "Serial port $SERIAL_PORT not found."
    echo "Device may already be in bootloader mode, checking..."
fi

# Step 4: Wait for RPI-RP2 drive
WAIT_SECS=15
for i in $(seq 1 "$WAIT_SECS"); do
    if [[ -b "$BOOT_DEVICE" ]]; then
        break
    fi
    if [[ $i -eq $WAIT_SECS ]]; then
        echo "Error: Bootloader drive ($BOOT_DEVICE) did not appear after ${WAIT_SECS}s"
        echo "Try manually entering bootloader mode: hold BOOTSEL while plugging in USB"
        exit 1
    fi
    sleep 1
done

# Step 5: Mount bootloader drive
if ! mountpoint -q "$BOOT_MOUNT" 2>/dev/null; then
    echo "Mounting bootloader drive..."
    udisksctl mount -b "$BOOT_DEVICE"
    sleep 1
fi

if [[ ! -d "$BOOT_MOUNT" ]]; then
    echo "Error: Boot mount point $BOOT_MOUNT not available"
    exit 1
fi

# Step 6: Copy UF2
echo "=== Flashing firmware ==="
echo "Copying UF2 to $BOOT_MOUNT..."
cp "$UF2_PATH" "$BOOT_MOUNT/"
sync

echo ""
echo "Firmware flashed! Device will reboot automatically."
