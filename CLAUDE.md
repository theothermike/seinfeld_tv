# TinyJukebox Development Guide

> When working in this codebase, read this file first. It contains project structure, build commands, and critical architectural constraints.

## Project Structure

- `converter/` - Python package for converting media (TV, movies, music videos, music, photos) into SD card format
- `firmware/TinyJukebox/` - Arduino firmware for TinyTV 2 hardware (RP2040)
- `firmware_upstream/` - Original TinyCircuits firmware (reference only)

## Python / Converter

### Environment

- Python venv at `.venv/` (Python 3.14)
- Activate: `.venv/bin/python` or `.venv/bin/pip`
- Do NOT use system `python` or `python3` — they don't have project deps

### Running Tests

```bash
# All tests
.venv/bin/python -m pytest converter/tests/ -v

# Single file
.venv/bin/python -m pytest converter/tests/test_binary_writer.py -v

# Single test
.venv/bin/python -m pytest converter/tests/test_binary_writer.py::TestShowMetadata::test_roundtrip -v
```

### Key Dependencies

- PIL/Pillow (thumbnail generation, photo conversion)
- ffmpeg (system binary, for video/audio conversion and frame extraction)
- requests (metadata fetching from TVMaze/TMDB)
- mutagen (audio file metadata - ID3, Vorbis, MP4, FLAC tags)

### Converter Modules

- `convert.py` - Main CLI entry point with `--type` routing (tv, movie, music-video, music, photo)
- `device_packager.py` - TV show packager (TVMaze metadata, season/episode layout)
- `movie_packager.py` - Movie packager (TMDB metadata, single video)
- `music_video_packager.py` - Music video collection packager
- `music_packager.py` - Music packager (audio tags, album art → AVI with looped art + audio)
- `photo_packager.py` - Photo album packager (resize to 210x135 RGB565)
- `metadata_fetcher.py` - TVMaze + TMDB API client with caching
- `binary_writer.py` - All .sdb binary metadata types (write/read)
- `video_converter.py` - ffmpeg wrapper for video/audio conversion
- `thumbnail_generator.py` - RGB565 thumbnail generation

## Firmware

### Toolchain

- arduino-cli at `/home/mike/.local/bin/arduino-cli`
- Board package: `rp2040:rp2040` v5.5.0 (Earle Philhower's arduino-pico)

### Compile

```bash
/home/mike/.local/bin/arduino-cli compile \
  --fqbn rp2040:rp2040:rpipico \
  --board-options "opt=Small,usbstack=tinyusb,freq=200" \
  /home/mike/dev/tiny_jukebox/firmware/TinyJukebox/TinyJukebox.ino
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
   cp ~/.cache/arduino/sketches/1E7FA6852524608C517BCB1B47E06FED/TinyJukebox.ino.uf2 /media/mike/RPI-RP2/
   ```

Note: `arduino-cli upload -p /dev/ttyACM0` does NOT work for this board in bootloader mode since there's no serial port. Always use the UF2 copy method above.

### Hardware Target

- TinyTV 2 (Raspberry Pi Pico / RP2040)
- CPU Speed: 200MHz
- USB Stack: Adafruit TinyUSB
- Display: 210x135 RGB565

### Firmware Architecture

- `TinyJukebox.ino` - Main state machine, setup, loop dispatchers
- `appState.h` - State enum, MediaType enum, metadata structs (unions), AppContext
- `globals.h` - Global variables and forward declarations
- `showMetadata.ino` - SD card metadata loading, media type scanning, settings, path helpers
- `showBrowser.ino` - TV show/season/episode browser UI and input handling
- `movieBrowser.ino` - Movie browser UI
- `musicVideoBrowser.ino` - Music video collection/video 2-level browser
- `musicBrowser.ino` - Music artist/album/track 3-level browser
- `photoSlideshow.ino` - Photo album browser + timer-based slideshow
- `splashScreen.ino` - "TinyJukebox" branding splash screen
- `mediaSelector.ino` - Top-level media type chooser
- `settingsMenu.ino` - Settings screen (slideshow interval, etc.)
- `uiHelpers.h` - Shared drawing helpers and color constants
- `display.ino` - Low-level display/DMA, GraphicsBuffer2 setup
- `drawEffects.ino` - Static effect, tube-off animation
- `SD_AVI.ino` - AVI streaming/playback engine
- `audio.ino` - Audio buffer management
- `infrared.ino` - IR remote input

### SD Card Layout

```
/settings.txt                    # Global settings
/boot.avi                        # Optional boot clip
/TV/                             # TV Shows
  ShowName/
    show.sdb / show.raw
    S01/
      season.sdb / thumb.raw
      E01.avi / E01.sdb / E01.raw
/Movies/                         # Movies
  MovieName/
    movie.sdb / movie.raw / movie.avi
/MusicVideos/                    # Music Videos
  CollectionName/
    collection.sdb / collection.raw
    V01.avi / V01.sdb / V01.raw
/Music/                          # Music
  ArtistName/
    artist.sdb / artist.raw
    A01/
      album.sdb / album.raw
      T01.avi / T01.sdb
/Photos/                         # Photo Gallery
  AlbumName/
    album.sdb / album.raw
    P01.raw / P01.sdb
```

### Metadata Formats (.sdb files)

All follow conventions: 4-byte magic, little-endian, null-padded strings, fixed sizes.

| Type | Magic | Size | Key Fields |
|------|-------|------|------------|
| show.sdb | `TJSM` | 128 | name(48), year(8), seasonCount, totalEpisodes |
| season.sdb | `TJSS` | 64 | seasonNumber, episodeCount, year(8), name(24) |
| E##.sdb | `TJSE` | 128 | season, episode, title(48), airDate(12), description(56) |
| movie.sdb | `TJMV` | 128 | title(48), year(8), runtimeMinutes, description(56) |
| collection.sdb | `TJVC` | 128 | name(48), year(8), videoCount |
| V##.sdb | `TJVD` | 128 | videoNumber, title(48), artist(12), description(56) |
| artist.sdb | `TJMA` | 128 | name(48), genre(8), albumCount, totalTracks |
| album.sdb (music) | `TJAL` | 64 | title(24), year(8), albumNumber, trackCount |
| T##.sdb | `TJTK` | 64 | trackNumber, title(48), runtimeSeconds |
| album.sdb (photo) | `TJPA` | 64 | title(48), photoCount |
| P##.sdb | `TJPH` | 64 | photoNumber, caption(48), dateTaken(8) |

## Scripts

- `scripts/convert.sh` - Wrapper for converter:
  - `./scripts/convert.sh --type tv --show "Seinfeld" --input ~/tv/Seinfeld/`
  - `./scripts/convert.sh --type movie --title "The Matrix" --input ~/movies/matrix.mkv`
  - `./scripts/convert.sh --type music-video --collection "80s Hits" --input ~/mv/80s/`
  - `./scripts/convert.sh --type music --artist "Pink Floyd" --input ~/music/pinkfloyd/`
  - `./scripts/convert.sh --type photo --album "Vacation" --input ~/photos/vacation/`
- `scripts/deploy.sh` - Deploy to TinyTV via USB MSC: `./scripts/deploy.sh --skip-convert`
- `scripts/flash.sh` - Compile + flash firmware: `./scripts/flash.sh [--compile-only] [--upload-only]`

## Conventions

- All .sdb files are binary structs (little-endian, packed) matching `appState.h` definitions
- Thumbnail .raw files are 108x67 RGB565 big-endian (14,472 bytes)
- Photo .raw files are 210x135 RGB565 big-endian (56,700 bytes) — displayed full-screen
- Path buffers in firmware should be 64 bytes to accommodate directory prefixes
- Directory names: alphanumeric + underscore, max 31 chars
- Music playback: AVI files with looped album art video + PCM audio (ffmpeg)
- Video format: AVI container, MJPEG video 210x135 @ 18fps, PCM unsigned 8-bit mono 11025Hz audio
- **Critical**: `.sdb` struct layouts in `converter/binary_writer.py` must exactly match the C structs in `firmware/TinyJukebox/appState.h` (field order, sizes, magic bytes). Changing one without the other will corrupt metadata reads on-device.
- Converter output defaults to `sdcard_test/` (set in `scripts/convert.sh`); `--output-dir` is required when calling `convert.py` directly
