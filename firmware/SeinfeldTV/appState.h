//-------------------------------------------------------------------------------
//  SeinfeldTV - Application State & Data Structures
//
//  State machine, context struct, and binary metadata structs that match
//  the Python converter's .sdb binary format.
//-------------------------------------------------------------------------------

#ifndef APPSTATE_H
#define APPSTATE_H

#include <stdint.h>

// ─── State Machine ───────────────────────────────────────────────────────────

enum AppState {
  STATE_BOOT,
  STATE_SHOW_BROWSER,
  STATE_SEASON_BROWSER,
  STATE_EPISODE_BROWSER,
  STATE_PLAYBACK,
  STATE_TRANSITION,
  STATE_POWER_OFF
};

// ─── Binary Metadata Structs (match converter/binary_writer.py) ──────────────
// All multi-byte integers are little-endian (RP2040 native).
// All strings are null-padded to their fixed width.

// show.sdb - 128 bytes
struct ShowMetadata {
  char     magic[4];        // "SFTV"
  uint8_t  version;         // FORMAT_VERSION (1)
  uint8_t  seasonCount;
  uint16_t totalEpisodes;   // LE
  char     name[48];        // null-padded
  char     year[8];         // null-padded
  uint8_t  reserved[64];
} __attribute__((packed));

// season.sdb - 64 bytes
struct SeasonMetadata {
  char    magic[4];          // "SFSN"
  uint8_t seasonNumber;
  uint8_t episodeCount;
  uint8_t reserved1[2];
  char    year[8];           // null-padded
  char    title[24];         // null-padded
  uint8_t reserved2[24];
} __attribute__((packed));

// E##.sdb - 128 bytes
struct EpisodeMetadata {
  char     magic[4];         // "SFEP"
  uint8_t  seasonNumber;
  uint8_t  episodeNumber;
  uint16_t runtimeMinutes;   // LE
  char     title[48];        // null-padded
  char     airDate[12];      // null-padded
  char     description[56];  // null-padded
  uint8_t  reserved[4];
} __attribute__((packed));

// ─── Input Flags (extended from upstream) ────────────────────────────────────
// We extend the upstream inputFlagStruct with browser-specific flags.
// Encoder 1 (channel knob) and Encoder 2 (volume knob) are remapped
// depending on state.

// Raw hardware events (set by ISRs / IR receiver)
struct RawInputFlags {
  bool encoderCW   = false;   // Channel knob clockwise
  bool encoderCCW  = false;   // Channel knob counter-clockwise
  bool encoder2CW  = false;   // Volume knob clockwise
  bool encoder2CCW = false;   // Volume knob counter-clockwise
  bool power       = false;   // Power button
  bool irChannelUp = false;
  bool irChannelDn = false;
  bool irVolUp     = false;
  bool irVolDn     = false;
  bool irMute      = false;
  bool irPower     = false;
};

// ─── Multi-Show Constants ────────────────────────────────────────────────────

#define MAX_SHOWS    10
#define SHOW_DIR_LEN 32   // Max directory name length including null terminator

// ─── Application Context ─────────────────────────────────────────────────────

struct AppContext {
  AppState currentState;
  AppState nextState;       // Used by TRANSITION state to know where to go

  // Show navigation
  char availableShows[MAX_SHOWS][SHOW_DIR_LEN]; // Directory names on SD card
  int availableShowCount;   // How many shows found
  int showNavIndex;         // Index into availableShows[]
  char currentShowDir[SHOW_DIR_LEN]; // Active show directory name

  // Season/episode navigation
  int currentSeason;        // Actual season number (e.g. 2 for S02)
  int currentEpisode;       // 1-based
  int seasonNavIndex;       // Index into availableSeasons[]

  // Available seasons on SD card (scanned per show)
  int availableSeasons[30]; // Actual season numbers found on card
  int availableSeasonCount; // How many seasons found

  // Loaded metadata (only one of each at a time to save RAM)
  ShowMetadata    showMeta;
  SeasonMetadata  seasonMeta;
  EpisodeMetadata episodeMeta;

  // UI state
  bool metadataLoaded;       // True after show.sdb parsed successfully
  bool seasonMetaLoaded;     // True after current season.sdb parsed
  bool episodeMetaLoaded;    // True after current episode .sdb parsed

  // Transition
  uint32_t transitionStart;  // millis() when transition began
  int transitionDurationMS;  // How long to show static (ms)

  // Settings persistence
  int  savedSeason;
  int  savedEpisode;
  int  savedVolume;
  bool settingsDirty;
  uint32_t settingsLastChange; // millis() of last change

  // Raw input
  RawInputFlags rawInput;
};

// ─── Constants ───────────────────────────────────────────────────────────────

#define SHOW_MAGIC    "SFTV"
#define SEASON_MAGIC  "SFSN"
#define EPISODE_MAGIC "SFEP"

#define TRANSITION_STATIC_MS  300

// Thumbnail dimensions
#define THUMB_W 108
#define THUMB_H  67
// RGB565, 2 bytes per pixel
#define THUMB_ROW_BYTES (THUMB_W * 2)

// Display area (from TinyTV2.h: VIDEO_W=210, VIDEO_H=135)
// We draw our UI into the same frameBuf used by video playback.

#endif // APPSTATE_H
