//-------------------------------------------------------------------------------
//  SeinfeldTV - Main Firmware
//
//  Custom TinyTV 2 firmware that turns the device into a dedicated
//  Seinfeld (or any TV show) player with season/episode browsing,
//  metadata display, thumbnails, and TV static transitions.
//
//  Based on TinyCircuits TinyTVs Firmware (GPL v3).
//
//  Board Package: Raspberry Pi Pico/RP2040 by Earle F. Philhower, III
//  Board: Raspberry Pi Pico
//  CPU Speed: 200MHz
//  USB Stack: Adafruit TinyUSB
//-------------------------------------------------------------------------------

// Uncomment to compile debug version
//#define DEBUGAPP (true)

// This include order matters
#include <SPI.h>
#include "src/TinyScreen/TinyScreen.h"
#include <SdFat.h>
#include <JPEGDEC.h>
#include "globals.h"
#include "versions.h"

SdFat32 sd;
File32 infile;
File32 dir;
JPEGDEC jpeg;

// TinyTV 2 hardware configuration
#include "TinyTV2.h"

#ifdef ARDUINO_ARCH_RP2040
#include <Adafruit_TinyUSB.h>
Adafruit_USBD_MSC usb_msc;
Adafruit_USBD_CDC cdc;
#include "USB_MSC.h"
#define has_USB_MSC 1
#endif
#include "USB_CDC.h"

#include "videoBuffer.h"
#include "settings.h"
#include "src/IRremote/src/TinyIRReceiver.hpp"

// ─── Local Playback Vars (from upstream, used during PLAYBACK state) ────────
int nextVideoError = 0;
int prevVideoError = 0;
bool showNoVideoError = false;
uint64_t TVscreenOffModeStartTime = 0;
bool skipNextFrame = false;
unsigned long totalTime = 0;
uint64_t powerDownTimer = 0;
bool TVscreenOffMode = false;
uint64_t settingsNeedSaved = 0;
uint64_t framerateHelper = 0;
bool live = false;
int staticTimeMS = 300;
char splashVidFileName[20] = "";
bool splashPlaybackMode = false;

// ─── Forward Declarations ───────────────────────────────────────────────────
void enterState(AppState state);
void loopBoot();
void loopShowBrowser();
void loopSeasonBrowser();
void loopEpisodeBrowser();
void loopPlayback();
void loopTransition();
void startEpisodePlayback();

// ─── Compatibility shim for USB_CDC.h ───────────────────────────────────────
// USB_CDC.h calls initVideoPlayback() on remote settings reset.
// We redirect it to reload our show metadata and re-enter the browser.
void initVideoPlayback(bool loadSettingsFile) {
  if (loadSettingsFile) {
    loadSettings();
  }
  loadVideoList(splashVidFileName);
  scanAvailableShows();
  loadShowSettings();
  // Validate saved show against available shows
  bool showFound = false;
  for (int i = 0; i < appCtx.availableShowCount; i++) {
    if (strcmp(appCtx.currentShowDir, appCtx.availableShows[i]) == 0) {
      appCtx.showNavIndex = i;
      showFound = true;
      break;
    }
  }
  if (!showFound && appCtx.availableShowCount > 0) {
    appCtx.showNavIndex = 0;
    strncpy(appCtx.currentShowDir, appCtx.availableShows[0], SHOW_DIR_LEN - 1);
    appCtx.currentShowDir[SHOW_DIR_LEN - 1] = '\0';
  }
  if (appCtx.availableShowCount > 0) {
    loadShowMetadata();
  }
  setVolume(volumeSetting);
  enterState(STATE_SHOW_BROWSER);
}

// ─── Setup ──────────────────────────────────────────────────────────────────

void setup() {
#ifdef has_USB_MSC
  Serial.end();
  cdc.begin(0);
  USBMSCInit();
  yield();
  delay(100);
  yield();
#endif

  clearAudioBuffer();
  initializeInfrared();
  initializeDisplay();
  initalizePins();

  if (!initializeSDcard()) {
    displayCardNotFound();
    while (1) {
      uint16_t totalJPEGBytesUnused;
      incomingCDCHandler(getFreeJPEGBuffer(), 0, &live, &totalJPEGBytesUnused);
      if (powerButtonPressed()) hardwarePowerOff();
    }
  }

#ifdef has_USB_MSC
  USBMSCReady();
#endif

  if (!initializeFS()) {
    displayFileSystemError();
    while (1) {
      uint16_t totalJPEGBytesUnused;
      incomingCDCHandler(getFreeJPEGBuffer(), 0, &live, &totalJPEGBytesUnused);
      if (powerButtonPressed()) hardwarePowerOff();
    }
  }

  // Initialize app context
  memset(&appCtx, 0, sizeof(AppContext));
  appCtx.currentState = STATE_BOOT;
  appCtx.currentSeason = 1;
  appCtx.currentEpisode = 1;

  // Scan for available shows (directories with show.sdb)
  scanAvailableShows();

  if (appCtx.availableShowCount == 0) {
    dbgPrint("No shows found - waiting for USB upload");
    displayNoVideosFound();
    while (1) {
#ifdef has_USB_MSC
      // Allow USB mass storage so user can upload files
      if (USBJustConnected()) {
        USBMSCStart();
        for (int i = 0; i < 50; i++) { delay(1); yield(); }
        displayUSBMSCmessage();
      }
      if (handleUSBMSC(powerButtonPressed())) {
        uint16_t totalJPEGBytesUnused;
        incomingCDCHandler(NULL, 0, &live, &totalJPEGBytesUnused);
        continue;
      }
      if (USBMSCJustStopped()) {
        // User ejected - try scanning for shows again
        for (int i = 0; i < 50; i++) { delay(1); yield(); }
        clearPowerButtonPressInt();
        scanAvailableShows();
        if (appCtx.availableShowCount > 0) break;  // Success! Continue to main loop
        displayNoVideosFound();
      }
#endif
      if (powerButtonPressed()) hardwarePowerOff();
      delay(100);
    }
  }

  // Load saved settings (last watched show, position, volume)
  loadShowSettings();

  // Validate saved show against available shows
  bool showFound = false;
  for (int i = 0; i < appCtx.availableShowCount; i++) {
    if (strcmp(appCtx.currentShowDir, appCtx.availableShows[i]) == 0) {
      appCtx.showNavIndex = i;
      showFound = true;
      break;
    }
  }
  if (!showFound) {
    appCtx.showNavIndex = 0;
    strncpy(appCtx.currentShowDir, appCtx.availableShows[0], SHOW_DIR_LEN - 1);
    appCtx.currentShowDir[SHOW_DIR_LEN - 1] = '\0';
    appCtx.currentSeason = 1;
    appCtx.currentEpisode = 1;
  }

  // Load show metadata for the selected show
  loadShowMetadata();
  setVolume(volumeSetting);

  // Load video list for playback engine compatibility
  loadVideoList(splashVidFileName);

  // Play boot clip if available
  File32 bootCheck;
  if (bootCheck.open("/boot.avi", O_RDONLY)) {
    bootCheck.close();
    dbgPrint("Playing boot clip");
    splashPlaybackMode = true;
    startVideo("boot.avi", 0);
    setAudioSampleRate(getVideoAudioRate());
    clearAudioBuffer();

    // Play boot clip until it ends
    appCtx.currentState = STATE_BOOT;
  } else {
    // No boot clip, go straight to show browser
    enterState(STATE_SHOW_BROWSER);
  }
}

// ─── State Entry ────────────────────────────────────────────────────────────

void enterState(AppState state) {
  dbgPrint("Entering state: " + String(state));
  appCtx.currentState = state;

  switch (state) {
    case STATE_SHOW_BROWSER:
      loadShowMetadata();
      drawShowBrowser();
      break;

    case STATE_SEASON_BROWSER:
      loadSeasonMetadata(appCtx.currentSeason);
      drawSeasonBrowser();
      break;

    case STATE_EPISODE_BROWSER:
      loadSeasonMetadata(appCtx.currentSeason);
      loadEpisodeMetadata(appCtx.currentSeason, appCtx.currentEpisode);
      drawEpisodeBrowser();
      break;

    case STATE_PLAYBACK:
      startEpisodePlayback();
      break;

    case STATE_TRANSITION:
      appCtx.transitionStart = millis();
      if (doStaticEffects) {
        drawStaticFor(appCtx.transitionDurationMS);
        playStaticFor(appCtx.transitionDurationMS);
      }
      break;

    case STATE_POWER_OFF:
      if (appCtx.settingsDirty) saveShowSettings();
      clearDisplay();
      startTubeOffEffect();
      while (tubeOffEffect() > 3);
      displayOff();
      TVscreenOffMode = true;
      TVscreenOffModeStartTime = millis();
      break;

    default:
      break;
  }
}

// ─── Episode Playback ───────────────────────────────────────────────────────

void startEpisodePlayback() {
  char aviPath[64];
  getEpisodeAVIPath(aviPath, sizeof(aviPath),
                    appCtx.currentSeason, appCtx.currentEpisode);

  dbgPrint("Starting playback: " + String(aviPath));

  if (startVideo(aviPath, 0)) {
    dbgPrint("Failed to start video: " + String(aviPath));
    // Try to advance to next episode
    advanceToNextEpisode();
    return;
  }

  setAudioSampleRate(getVideoAudioRate());
  clearAudioBuffer();
  splashPlaybackMode = false;
  showNoVideoError = false;

  // Mark settings dirty for resume
  appCtx.settingsDirty = true;
  appCtx.settingsLastChange = millis();
}

void advanceToNextEpisode() {
  if (appCtx.seasonMetaLoaded &&
      appCtx.currentEpisode < appCtx.seasonMeta.episodeCount) {
    // Next episode in season
    appCtx.currentEpisode++;
  } else if (appCtx.seasonNavIndex < appCtx.availableSeasonCount - 1) {
    // Next available season
    appCtx.seasonNavIndex++;
    appCtx.currentSeason = appCtx.availableSeasons[appCtx.seasonNavIndex];
    appCtx.currentEpisode = 1;
    loadSeasonMetadata(appCtx.currentSeason);
  } else {
    // End of show - return to show browser
    appCtx.currentSeason = 1;
    appCtx.currentEpisode = 1;
    appCtx.nextState = STATE_SHOW_BROWSER;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    return;
  }

  appCtx.nextState = STATE_PLAYBACK;
  appCtx.currentState = STATE_TRANSITION;
  appCtx.transitionStart = millis();
  appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
}

// ─── Main Loop ──────────────────────────────────────────────────────────────

void loop() {
  // Handle USB Mass Storage mode (from upstream)
#ifdef has_USB_MSC
  if (USBJustConnected() && !live) {
    setAudioSampleRate(100);
    USBMSCStart();
    for (int i = 0; i < 50; i++) { delay(1); yield(); }
    if (TVscreenOffMode) {
      TVscreenOffMode = false;
      displayOn();
    }
    displayUSBMSCmessage();
  }
  if (handleUSBMSC(powerButtonPressed())) {
    uint16_t totalJPEGBytesUnused;
    if (getFreeJPEGBuffer()) {
      if (incomingCDCHandler(getFreeJPEGBuffer(), VIDEOBUF_SIZE, &live, &totalJPEGBytesUnused)) {
        handleUSBMSC(true);
      }
    } else {
      incomingCDCHandler(NULL, 0, &live, &totalJPEGBytesUnused);
    }
    return;
  }
  if (USBMSCJustStopped()) {
    dbgPrint("MSC Stopped");
    for (int i = 0; i < 50; i++) { delay(1); yield(); }
    clearPowerButtonPressInt();
    // Reload metadata after USB mode (user may have changed files)
    scanAvailableShows();
    loadShowSettings();
    // Validate saved show
    bool usbShowFound = false;
    for (int i = 0; i < appCtx.availableShowCount; i++) {
      if (strcmp(appCtx.currentShowDir, appCtx.availableShows[i]) == 0) {
        appCtx.showNavIndex = i;
        usbShowFound = true;
        break;
      }
    }
    if (!usbShowFound && appCtx.availableShowCount > 0) {
      appCtx.showNavIndex = 0;
      strncpy(appCtx.currentShowDir, appCtx.availableShows[0], SHOW_DIR_LEN - 1);
      appCtx.currentShowDir[SHOW_DIR_LEN - 1] = '\0';
    }
    if (appCtx.availableShowCount > 0) {
      loadShowMetadata();
    }
    enterState(STATE_SHOW_BROWSER);
    return;
  }
#endif

  // CDC handler for incoming data
  if (getFreeJPEGBuffer()) {
    uint16_t totalJPEGBytes = 0;
    if (incomingCDCHandler(getFreeJPEGBuffer(), VIDEOBUF_SIZE, &live, &totalJPEGBytes)) {
      JPEGBufferFilled(totalJPEGBytes);
    }
  }

  // ─── Power Off Handling ──────────────────────────────────────────────
  if (TVscreenOffMode) {
    // Check for wake-up events
    updateRawButtonStates(&appCtx.rawInput);
    IRInputRaw(&appCtx.rawInput);

    if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
      appCtx.rawInput.power = false;
      appCtx.rawInput.irPower = false;
      TVscreenOffMode = false;
      displayOn();
      enterState(STATE_SHOW_BROWSER);
      return;
    }

    if (millis() - TVscreenOffModeStartTime > 1000UL * powerTimeoutSecs) {
      hardwarePowerOff();
    }
    return;
  }

  // ─── State Machine Dispatch ──────────────────────────────────────────
  switch (appCtx.currentState) {
    case STATE_BOOT:
      loopBoot();
      break;
    case STATE_SHOW_BROWSER:
      loopShowBrowser();
      break;
    case STATE_SEASON_BROWSER:
      loopSeasonBrowser();
      break;
    case STATE_EPISODE_BROWSER:
      loopEpisodeBrowser();
      break;
    case STATE_PLAYBACK:
      loopPlayback();
      break;
    case STATE_TRANSITION:
      loopTransition();
      break;
    case STATE_POWER_OFF:
      // Handled by TVscreenOffMode check above
      break;
  }

  // ─── Settings Auto-Save ──────────────────────────────────────────────
  if (appCtx.settingsDirty &&
      millis() - appCtx.settingsLastChange > 2000) {
    saveShowSettings();
  }
}

// ─── State Loop Functions ───────────────────────────────────────────────────

void loopBoot() {
  // Playing boot.avi clip
  // Use upstream AVI streaming logic
  bool streamError = false;
  if (isAVIStreamAvailable()) {
    uint32_t len = nextChunkLength();
    if (len > 0) {
      if (isNextChunkAudio()) {
        uint8_t audioBuffer[512];
        int bytes = readNextChunk(audioBuffer, sizeof(audioBuffer));
        addToAudioBuffer(audioBuffer, bytes);
      } else if (isNextChunkVideo()) {
        if (frameWaitDurationElapsed() && getFreeJPEGBuffer()) {
          readNextChunk(getFreeJPEGBuffer(), VIDEOBUF_SIZE);
          JPEGBufferFilled(len);
        }
      } else if (isNextChunkIndex()) {
        if (jumpToNextMoviList()) streamError = true;
      } else {
        streamError = true;
      }
    } else {
      skipChunk();
      if (nextChunkLength() == 0) streamError = true;
    }
  } else {
    streamError = true;
  }

  if (streamError) {
    // Boot clip finished or errored, go to show browser
    enterState(STATE_SHOW_BROWSER);
  }

  // Allow skipping boot clip with any input
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);
  RawInputFlags& raw = appCtx.rawInput;
  if (raw.encoderCW || raw.encoderCCW || raw.encoder2CW || raw.encoder2CCW ||
      raw.irChannelUp || raw.irChannelDn || raw.power || raw.irPower) {
    memset(&appCtx.rawInput, 0, sizeof(RawInputFlags));
    enterState(STATE_SHOW_BROWSER);
  }
}

void loopShowBrowser() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  // Power button
  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleShowBrowserInput();
}

void loopSeasonBrowser() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  // Power button
  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleSeasonBrowserInput();
}

void loopEpisodeBrowser() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  // Power button
  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleEpisodeBrowserInput();
}

void loopPlayback() {
  // Collect input - during playback we use upstream-compatible input mapping
  IRInput(&inputFlags);
  updateButtonStates(&inputFlags);

  // Power button
  if (inputFlags.power) {
    inputFlags.power = false;
    if (doStaticEffects) {
      drawStaticFor(staticTimeMS);
      playStaticFor(staticTimeMS);
    }
    enterState(STATE_POWER_OFF);
    return;
  }

  // Volume controls (encoder2 = volume knob during playback)
  if (inputFlags.volUp) {
    inputFlags.volUp = false;
    settingsNeedSaved = millis();
    volumeUp();
    drawVolumeFor(1000);
  }
  if (inputFlags.volDown) {
    inputFlags.volDown = false;
    settingsNeedSaved = millis();
    volumeDown();
    drawVolumeFor(1000);
  }

  // Mute
  if (inputFlags.mute) {
    inputFlags.mute = false;
    setMute(!isMute());
  }

  // Channel up = skip to next episode (with static)
  if (inputFlags.channelUp) {
    inputFlags.channelUp = false;
    advanceToNextEpisode();
    return;
  }

  // Channel down = return to episode browser
  if (inputFlags.channelDown) {
    inputFlags.channelDown = false;
    appCtx.nextState = STATE_EPISODE_BROWSER;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    return;
  }

  // ─── AVI Streaming (from upstream) ─────────────────────────────────
  bool streamError = false;
  if (isAVIStreamAvailable()) {
    uint32_t len = nextChunkLength();
    if (len > 0) {
      if (isNextChunkAudio()) {
        uint8_t audioBuffer[512];
        int bytes = readNextChunk(audioBuffer, sizeof(audioBuffer));
        addToAudioBuffer(audioBuffer, bytes);
      } else if (isNextChunkVideo()) {
        if (skipNextFrame) {
          skipChunk();
          skipNextFrame = false;
        } else if (frameWaitDurationElapsed() && getFreeJPEGBuffer()) {
          readNextChunk(getFreeJPEGBuffer(), VIDEOBUF_SIZE);
          JPEGBufferFilled(len);
        }
      } else if (isNextChunkIndex()) {
        if (jumpToNextMoviList()) streamError = true;
      } else {
        streamError = true;
      }
    } else {
      skipChunk();
      if (nextChunkLength() == 0) streamError = true;
    }
  } else {
    streamError = true;
  }

  if (streamError) {
    // Episode ended (or error) - advance to next
    advanceToNextEpisode();
    return;
  }

  // Frame skip logic (from upstream)
  if (getVideoAudioRate() && audioSamplesInBuffer() < 200) {
    skipNextFrame = true;
  } else {
    skipNextFrame = false;
  }

  // Settings auto-save (upstream pattern)
  if (settingsNeedSaved && millis() - settingsNeedSaved > 2000) {
    saveSettings();
    settingsNeedSaved = 0;
  }
}

void loopTransition() {
  // Static effect is already playing (started in enterState)
  if (millis() - appCtx.transitionStart >= (uint32_t)appCtx.transitionDurationMS) {
    enterState(appCtx.nextState);
  }
}

// ─── Frame Rate Helper (from upstream) ──────────────────────────────────────

bool frameWaitDurationElapsed() {
  if (live) return true;
  if ((int64_t(micros() - framerateHelper) < (targetFrameTime - 5000))) {
    delay(1);
    yield();
    return false;
  }
  if (audioSamplesInBuffer() > 1000) {
    delay(1);
    yield();
    return false;
  }
  framerateHelper = micros();
  return true;
}

// ─── Core 1 - JPEG Decode (from upstream) ───────────────────────────────────

void setup1() {
}

void loop1() {
  // Only decode during playback or boot
  if (appCtx.currentState != STATE_PLAYBACK &&
      appCtx.currentState != STATE_BOOT) {
    return;
  }
  if (TVscreenOffMode) return;

  if (!getFilledJPEGBuffer()) return;

  if (!jpeg.openRAM(getFilledJPEGBuffer(), getJPEGBufferLength(), JPEGDraw)) {
    if (getJPEGBufferLength() != 240) {
      dbgPrint("Could not open frame from RAM! Error: " + String(jpeg.getLastError()));
    }
  }
  newJPEGFrameSize(jpeg.getWidth(), jpeg.getHeight());
  jpeg.setPixelType(RGB565_BIG_ENDIAN);
  jpeg.setMaxOutputSize(2048);
  jpeg.decode(0, 0, 0);

  JPEGBufferDecoded();
}
