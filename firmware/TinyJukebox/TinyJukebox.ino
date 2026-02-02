//-------------------------------------------------------------------------------
//  TinyJukebox - Multi-Media Player Firmware
//
//  Custom TinyTV 2 firmware supporting TV Shows, Movies, Music Videos,
//  Music, and Photo Gallery Slideshow with browsing, metadata, thumbnails,
//  and TV static transitions.
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
void loopSplash();
void loopMediaSelector();
void loopShowBrowser();
void loopSeasonBrowser();
void loopEpisodeBrowser();
void loopMovieBrowser();
void loopMVCollection();
void loopMVVideoBrowser();
void loopMusicArtist();
void loopMusicAlbum();
void loopMusicTrack();
void loopPhotoAlbum();
void loopPhotoSlideshow();
void loopSettings();
void loopPlayback();
void loopTransition();
void startEpisodePlayback();
void startGenericPlayback(const char* aviPath);
void advanceToNextEpisode();

// ─── Helper: Enter the default state for the current media type ─────────────
AppState getFirstStateForMediaType(MediaType type) {
  switch (type) {
    case MEDIA_TV:           return STATE_SHOW_BROWSER;
    case MEDIA_MOVIES:       return STATE_MOVIE_BROWSER;
    case MEDIA_MUSIC_VIDEOS: return STATE_MV_COLLECTION;
    case MEDIA_MUSIC:        return STATE_MUSIC_ARTIST;
    case MEDIA_PHOTOS:       return STATE_PHOTO_ALBUM;
    default:                 return STATE_MEDIA_SELECTOR;
  }
}

// ─── Compatibility shim for USB_CDC.h ───────────────────────────────────────
void initVideoPlayback(bool loadSettingsFile) {
  if (loadSettingsFile) {
    loadSettings();
  }
  loadVideoList(splashVidFileName);
  scanMediaTypes();
  scanAvailableShows();
  loadShowSettings();
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
  enterState(STATE_MEDIA_SELECTOR);
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
  appCtx.slideshowIntervalSec = DEFAULT_SLIDESHOW_SEC;

  // Scan for available media types
  scanMediaTypes();

  // Scan for TV shows (if any)
  scanAvailableShows();

  if (appCtx.mediaSelectorCount == 0) {
    dbgPrint("No media found - waiting for USB upload");
    displayNoVideosFound();
    while (1) {
#ifdef has_USB_MSC
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
        for (int i = 0; i < 50; i++) { delay(1); yield(); }
        clearPowerButtonPressInt();
        scanMediaTypes();
        scanAvailableShows();
        if (appCtx.mediaSelectorCount > 0) break;
        displayNoVideosFound();
      }
#endif
      if (powerButtonPressed()) hardwarePowerOff();
      delay(100);
    }
  }

  // Load saved settings
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
    appCtx.currentSeason = 1;
    appCtx.currentEpisode = 1;
  }

  if (appCtx.availableShowCount > 0) {
    loadShowMetadata();
  }
  setVolume(volumeSetting);

  // Load video list for playback engine compatibility
  loadVideoList(splashVidFileName);

  // Play boot clip if available, then splash, then media selector
  File32 bootCheck;
  if (bootCheck.open("/boot.avi", O_RDONLY)) {
    bootCheck.close();
    dbgPrint("Playing boot clip");
    splashPlaybackMode = true;
    startVideo("boot.avi", 0);
    setAudioSampleRate(getVideoAudioRate());
    clearAudioBuffer();
    appCtx.currentState = STATE_BOOT;
  } else {
    // No boot clip, go to splash screen
    enterState(STATE_SPLASH);
  }
}

// ─── State Entry ────────────────────────────────────────────────────────────

void enterState(AppState state) {
  dbgPrint("Entering state: " + String(state));
  appCtx.currentState = state;

  switch (state) {
    case STATE_SPLASH:
      appCtx.splashStartTime = millis();
      drawSplashScreen();
      break;

    case STATE_MEDIA_SELECTOR:
      // If only one media type available, skip selector
      if (appCtx.mediaSelectorCount == 1) {
        for (int i = 0; i < MEDIA_TYPE_COUNT; i++) {
          if (appCtx.mediaTypeAvailable[i]) {
            appCtx.currentMediaType = (MediaType)i;
            enterState(getFirstStateForMediaType(appCtx.currentMediaType));
            return;
          }
        }
      }
      drawMediaSelector();
      break;

    case STATE_SHOW_BROWSER:
      appCtx.currentMediaType = MEDIA_TV;
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

    case STATE_MOVIE_BROWSER:
      appCtx.currentMediaType = MEDIA_MOVIES;
      scanMovies();
      drawMovieBrowser();
      break;

    case STATE_MV_COLLECTION:
      appCtx.currentMediaType = MEDIA_MUSIC_VIDEOS;
      scanMVCollections();
      drawMVCollectionBrowser();
      break;

    case STATE_MV_VIDEO_BROWSER:
      drawMVVideoBrowser();
      break;

    case STATE_MUSIC_ARTIST:
      appCtx.currentMediaType = MEDIA_MUSIC;
      scanMusicArtists();
      drawMusicArtistBrowser();
      break;

    case STATE_MUSIC_ALBUM:
      drawMusicAlbumBrowser();
      break;

    case STATE_MUSIC_TRACK:
      drawMusicTrackBrowser();
      break;

    case STATE_PHOTO_ALBUM:
      appCtx.currentMediaType = MEDIA_PHOTOS;
      scanPhotoAlbums();
      drawPhotoAlbumBrowser();
      break;

    case STATE_PHOTO_SLIDESHOW:
      appCtx.slideshowLastAdvance = millis();
      appCtx.slideshowCurrentPhoto = 1;
      displayPhoto(appCtx.slideshowCurrentPhoto);
      break;

    case STATE_SETTINGS:
      drawSettingsMenu();
      break;

    case STATE_PLAYBACK:
      if (appCtx.currentMediaType == MEDIA_TV) {
        startEpisodePlayback();
      } else {
        // Generic playback started before entering this state
      }
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

// ─── Episode Playback (TV Shows) ────────────────────────────────────────────

void startEpisodePlayback() {
  char aviPath[64];
  getEpisodeAVIPath(aviPath, sizeof(aviPath),
                    appCtx.currentSeason, appCtx.currentEpisode);

  dbgPrint("Starting playback: " + String(aviPath));

  if (startVideo(aviPath, 0)) {
    dbgPrint("Failed to start video: " + String(aviPath));
    advanceToNextEpisode();
    return;
  }

  setAudioSampleRate(getVideoAudioRate());
  clearAudioBuffer();
  splashPlaybackMode = false;
  showNoVideoError = false;

  appCtx.settingsDirty = true;
  appCtx.settingsLastChange = millis();
}

// ─── Generic Playback (Movies, Music Videos, Music) ─────────────────────────

void startGenericPlayback(const char* aviPath) {
  dbgPrint("Starting playback: " + String(aviPath));

  if (startVideo(aviPath, 0)) {
    dbgPrint("Failed to start video: " + String(aviPath));
    return;
  }

  setAudioSampleRate(getVideoAudioRate());
  clearAudioBuffer();
  splashPlaybackMode = false;
  showNoVideoError = false;

  appCtx.currentState = STATE_PLAYBACK;
}

void advanceToNextEpisode() {
  if (appCtx.seasonMetaLoaded &&
      appCtx.currentEpisode < appCtx.meta2.seasonMeta.episodeCount) {
    appCtx.currentEpisode++;
  } else if (appCtx.seasonNavIndex < appCtx.availableSeasonCount - 1) {
    appCtx.seasonNavIndex++;
    appCtx.currentSeason = appCtx.availableSeasons[appCtx.seasonNavIndex];
    appCtx.currentEpisode = 1;
    loadSeasonMetadata(appCtx.currentSeason);
  } else {
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
    // Reload metadata after USB mode
    scanMediaTypes();
    scanAvailableShows();
    loadShowSettings();
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
    enterState(STATE_MEDIA_SELECTOR);
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
    updateRawButtonStates(&appCtx.rawInput);
    IRInputRaw(&appCtx.rawInput);

    if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
      appCtx.rawInput.power = false;
      appCtx.rawInput.irPower = false;
      TVscreenOffMode = false;
      displayOn();
      enterState(STATE_MEDIA_SELECTOR);
      return;
    }

    if (millis() - TVscreenOffModeStartTime > 1000UL * powerTimeoutSecs) {
      hardwarePowerOff();
    }
    return;
  }

  // ─── State Machine Dispatch ──────────────────────────────────────────
  switch (appCtx.currentState) {
    case STATE_BOOT:           loopBoot(); break;
    case STATE_SPLASH:         loopSplash(); break;
    case STATE_MEDIA_SELECTOR: loopMediaSelector(); break;
    case STATE_SHOW_BROWSER:   loopShowBrowser(); break;
    case STATE_SEASON_BROWSER: loopSeasonBrowser(); break;
    case STATE_EPISODE_BROWSER: loopEpisodeBrowser(); break;
    case STATE_MOVIE_BROWSER:  loopMovieBrowser(); break;
    case STATE_MV_COLLECTION:  loopMVCollection(); break;
    case STATE_MV_VIDEO_BROWSER: loopMVVideoBrowser(); break;
    case STATE_MUSIC_ARTIST:   loopMusicArtist(); break;
    case STATE_MUSIC_ALBUM:    loopMusicAlbum(); break;
    case STATE_MUSIC_TRACK:    loopMusicTrack(); break;
    case STATE_PHOTO_ALBUM:    loopPhotoAlbum(); break;
    case STATE_PHOTO_SLIDESHOW: loopPhotoSlideshow(); break;
    case STATE_SETTINGS:       loopSettings(); break;
    case STATE_PLAYBACK:       loopPlayback(); break;
    case STATE_TRANSITION:     loopTransition(); break;
    case STATE_POWER_OFF:      break;
  }

  // ─── Settings Auto-Save ──────────────────────────────────────────────
  if (appCtx.settingsDirty &&
      millis() - appCtx.settingsLastChange > 2000) {
    saveShowSettings();
  }
}

// ─── State Loop Functions ───────────────────────────────────────────────────

void loopBoot() {
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
    enterState(STATE_SPLASH);
  }

  // Allow skipping boot clip with any input
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);
  RawInputFlags& raw = appCtx.rawInput;
  if (raw.encoderCW || raw.encoderCCW || raw.encoder2CW || raw.encoder2CCW ||
      raw.irChannelUp || raw.irChannelDn || raw.power || raw.irPower) {
    memset(&appCtx.rawInput, 0, sizeof(RawInputFlags));
    enterState(STATE_SPLASH);
  }
}

void loopSplash() {
  // Check if splash duration elapsed
  if (millis() - appCtx.splashStartTime >= SPLASH_DURATION_MS) {
    enterState(STATE_MEDIA_SELECTOR);
    return;
  }

  // Allow skipping splash with any input
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);
  RawInputFlags& raw = appCtx.rawInput;
  if (raw.encoderCW || raw.encoderCCW || raw.encoder2CW || raw.encoder2CCW ||
      raw.irChannelUp || raw.irChannelDn || raw.power || raw.irPower) {
    memset(&appCtx.rawInput, 0, sizeof(RawInputFlags));
    enterState(STATE_MEDIA_SELECTOR);
  }
}

void loopMediaSelector() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleMediaSelectorInput();
}

void loopShowBrowser() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

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

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleEpisodeBrowserInput();
}

void loopMovieBrowser() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleMovieBrowserInput();
}

void loopMVCollection() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleMVCollectionInput();
}

void loopMVVideoBrowser() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleMVVideoBrowserInput();
}

void loopMusicArtist() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleMusicArtistInput();
}

void loopMusicAlbum() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleMusicAlbumInput();
}

void loopMusicTrack() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleMusicTrackInput();
}

void loopPhotoAlbum() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handlePhotoAlbumInput();
}

void loopPhotoSlideshow() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handlePhotoSlideshowInput();

  // Auto-advance timer
  if (millis() - appCtx.slideshowLastAdvance >=
      (uint32_t)appCtx.slideshowIntervalSec * 1000) {
    advanceSlideshow();
  }
}

void loopSettings() {
  updateRawButtonStates(&appCtx.rawInput);
  IRInputRaw(&appCtx.rawInput);

  if (appCtx.rawInput.power || appCtx.rawInput.irPower) {
    appCtx.rawInput.power = false;
    appCtx.rawInput.irPower = false;
    enterState(STATE_POWER_OFF);
    return;
  }

  handleSettingsInput();
}

void loopPlayback() {
  // Collect input
  IRInput(&inputFlags);
  updateButtonStates(&inputFlags);

  if (inputFlags.power) {
    inputFlags.power = false;
    if (doStaticEffects) {
      drawStaticFor(staticTimeMS);
      playStaticFor(staticTimeMS);
    }
    enterState(STATE_POWER_OFF);
    return;
  }

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
  if (inputFlags.mute) {
    inputFlags.mute = false;
    setMute(!isMute());
  }

  // Channel up = skip forward
  if (inputFlags.channelUp) {
    inputFlags.channelUp = false;
    if (appCtx.currentMediaType == MEDIA_TV) {
      advanceToNextEpisode();
    } else if (appCtx.currentMediaType == MEDIA_MUSIC_VIDEOS) {
      advanceToNextMusicVideo();
    } else if (appCtx.currentMediaType == MEDIA_MUSIC) {
      advanceToNextTrack();
    } else {
      // Movies: just go back to browser
      appCtx.nextState = STATE_MOVIE_BROWSER;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    }
    return;
  }

  // Channel down = return to browser
  if (inputFlags.channelDown) {
    inputFlags.channelDown = false;
    AppState backState;
    switch (appCtx.currentMediaType) {
      case MEDIA_TV:           backState = STATE_EPISODE_BROWSER; break;
      case MEDIA_MOVIES:       backState = STATE_MOVIE_BROWSER; break;
      case MEDIA_MUSIC_VIDEOS: backState = STATE_MV_VIDEO_BROWSER; break;
      case MEDIA_MUSIC:        backState = STATE_MUSIC_TRACK; break;
      default:                 backState = STATE_MEDIA_SELECTOR; break;
    }
    appCtx.nextState = backState;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    return;
  }

  // ─── AVI Streaming ─────────────────────────────────────────────────
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
    // Video ended - auto-advance based on media type
    if (appCtx.currentMediaType == MEDIA_TV) {
      advanceToNextEpisode();
    } else if (appCtx.currentMediaType == MEDIA_MUSIC_VIDEOS) {
      advanceToNextMusicVideo();
    } else if (appCtx.currentMediaType == MEDIA_MUSIC) {
      advanceToNextTrack();
    } else {
      // Movie ended - back to browser
      appCtx.nextState = STATE_MOVIE_BROWSER;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    }
    return;
  }

  if (getVideoAudioRate() && audioSamplesInBuffer() < 200) {
    skipNextFrame = true;
  } else {
    skipNextFrame = false;
  }

  if (settingsNeedSaved && millis() - settingsNeedSaved > 2000) {
    saveSettings();
    settingsNeedSaved = 0;
  }
}

void loopTransition() {
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
