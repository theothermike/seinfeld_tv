//-------------------------------------------------------------------------------
//  TinyJukebox - Global State
//
//  Extended from upstream globals.h to include AppContext and keep
//  backward compatibility with upstream .ino files that reference inputFlags.
//-------------------------------------------------------------------------------

#include "appState.h"

// Upstream-compatible input flags (used by SD_AVI.ino, display.ino, etc.)
typedef struct inputFlagStruct {
  bool channelUp = false;
  bool channelDown = false;
  bool volUp = false;
  bool volDown = false;
  bool mute = false;
  bool power = false;
  bool channelSet = false;
  bool volumeSet = false;
  bool settingsChanged = false;
} inputFlagStruct;

// PLAYBACK PARAMETERS
uint64_t targetFrameTime;

int volumeSetting = 3;

inputFlagStruct inputFlags;

// ─── TinyJukebox App Context (global) ────────────────────────────────────────
AppContext appCtx;
