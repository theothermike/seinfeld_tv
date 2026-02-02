//-------------------------------------------------------------------------------
//  TinyJukebox - Music Browser
//
//  Three-level browser: Artist -> Album -> Track.
//  Scans /Music/ for artist directories containing artist.sdb.
//  Each artist has album subdirs (A##/) with album.sdb, and tracks (T##.avi).
//  Supports auto-advance through tracks in an album.
//  Vol knob navigates, CH forward selects/plays, CH back goes up a level.
//-------------------------------------------------------------------------------

#include "uiHelpers.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Artist Scanning ────────────────────────────────────────────────────────
// Scan /Music/ for subdirectories containing artist.sdb.

void scanMusicArtists() {
  appCtx.availableItemCount = 0;

  File32 root;
  if (!root.open(MUSIC_DIR, O_RDONLY)) {
    dbgPrint("Failed to open " MUSIC_DIR);
    return;
  }

  File32 entry;
  while (entry.openNext(&root, O_RDONLY)) {
    if (!entry.isDir()) {
      entry.close();
      continue;
    }

    char dirName[ITEM_DIR_LEN];
    entry.getName(dirName, sizeof(dirName));
    entry.close();

    if (dirName[0] == '.' || dirName[0] == '_') continue;

    char sdbPath[64];
    snprintf(sdbPath, sizeof(sdbPath), "%s/%s/artist.sdb", MUSIC_DIR, dirName);
    File32 sdbCheck;
    if (sdbCheck.open(sdbPath, O_RDONLY)) {
      sdbCheck.close();
      if (appCtx.availableItemCount < MAX_ITEMS) {
        strncpy(appCtx.availableItems[appCtx.availableItemCount], dirName, ITEM_DIR_LEN - 1);
        appCtx.availableItems[appCtx.availableItemCount][ITEM_DIR_LEN - 1] = '\0';
        appCtx.availableItemCount++;
        dbgPrint("Found artist: " + String(dirName));
      }
    }
  }
  root.close();

  // Sort alphabetically
  for (int i = 0; i < appCtx.availableItemCount - 1; i++) {
    for (int j = i + 1; j < appCtx.availableItemCount; j++) {
      if (strcmp(appCtx.availableItems[i], appCtx.availableItems[j]) > 0) {
        char tmp[ITEM_DIR_LEN];
        memcpy(tmp, appCtx.availableItems[i], ITEM_DIR_LEN);
        memcpy(appCtx.availableItems[i], appCtx.availableItems[j], ITEM_DIR_LEN);
        memcpy(appCtx.availableItems[j], tmp, ITEM_DIR_LEN);
      }
    }
  }

  appCtx.itemNavIndex = 0;
  if (appCtx.availableItemCount > 0) {
    strncpy(appCtx.currentItemDir, appCtx.availableItems[0], ITEM_DIR_LEN - 1);
    appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
    loadArtistMetadata();
  }

  dbgPrint("Artists found: " + String(appCtx.availableItemCount));
}

// ─── Metadata Loading ───────────────────────────────────────────────────────

bool loadArtistMetadata() {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/artist.sdb", MUSIC_DIR, appCtx.currentItemDir);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.metadataLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(ArtistMetadata)) {
    dbgPrint("artist.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.metadataLoaded = false;
    return false;
  }

  f.read(&appCtx.meta1.artistMeta, sizeof(ArtistMetadata));
  f.close();

  if (strncmp(appCtx.meta1.artistMeta.magic, ARTIST_MAGIC, 4) != 0) {
    dbgPrint("artist.sdb bad magic");
    appCtx.metadataLoaded = false;
    return false;
  }

  appCtx.metadataLoaded = true;
  dbgPrint("Loaded artist: " + String(appCtx.meta1.artistMeta.name) +
           " (" + String(appCtx.meta1.artistMeta.albumCount) + " albums)");
  return true;
}

bool loadMusicAlbumMetadata(int albumNum) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/A%02d/album.sdb",
           MUSIC_DIR, appCtx.currentItemDir, albumNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.seasonMetaLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(MusicAlbumMetadata)) {
    dbgPrint("album.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.seasonMetaLoaded = false;
    return false;
  }

  f.read(&appCtx.meta2.musicAlbumMeta, sizeof(MusicAlbumMetadata));
  f.close();

  if (strncmp(appCtx.meta2.musicAlbumMeta.magic, MUSIC_ALBUM_MAGIC, 4) != 0) {
    dbgPrint("album.sdb bad magic");
    appCtx.seasonMetaLoaded = false;
    return false;
  }

  appCtx.seasonMetaLoaded = true;
  appCtx.subItemCount = appCtx.meta2.musicAlbumMeta.trackCount;
  dbgPrint("Loaded album A" + String(albumNum) + ": " +
           String(appCtx.meta2.musicAlbumMeta.title) +
           " (" + String(appCtx.subItemCount) + " tracks)");
  return true;
}

bool loadTrackMetadata(int albumNum, int trackNum) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/A%02d/T%02d.sdb",
           MUSIC_DIR, appCtx.currentItemDir, albumNum, trackNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(TrackMetadata)) {
    dbgPrint("T##.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  f.read(&appCtx.meta3.trackMeta, sizeof(TrackMetadata));
  f.close();

  if (strncmp(appCtx.meta3.trackMeta.magic, TRACK_MAGIC, 4) != 0) {
    dbgPrint("T##.sdb bad magic");
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  appCtx.episodeMetaLoaded = true;
  dbgPrint("Loaded track T" + String(trackNum) + ": " +
           String(appCtx.meta3.trackMeta.title));
  return true;
}

// ─── Draw Artist Browser ────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "Music" header (white, centered)
//   Row 16:     Divider
//   Row 20-86:  Thumbnail (108x67) centered
//   Row 90:     Artist name (yellow, centered)
//   Row 106:    Genre + album count (gray)
//   Row 122:    Hint text

void drawMusicArtistBrowser() {
  clearFrameBuf();

  // Header
  const char* header = "Music";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_WHITE);

  // Divider
  drawHLine(10, 16, VIDEO_W - 20, COL_GRAY_DK);

  // Thumbnail
  int thumbX = (VIDEO_W - THUMB_W) / 2;
  int thumbY = 20;
  char thumbPath[64];
  snprintf(thumbPath, sizeof(thumbPath), "%s/%s/artist.raw",
           MUSIC_DIR, appCtx.availableItems[appCtx.itemNavIndex]);
  if (!displayThumbnail(thumbPath, thumbX, thumbY)) {
    fillRect(thumbX, thumbY, THUMB_W, THUMB_H, COL_GRAY_DK);
  }

  // Navigation arrows
  if (appCtx.itemNavIndex > 0) {
    drawText("<", 4, thumbY + THUMB_H / 2 - 6, COL_GRAY_MED);
  }
  if (appCtx.itemNavIndex < appCtx.availableItemCount - 1) {
    drawText(">", VIDEO_W - 12, thumbY + THUMB_H / 2 - 6, COL_GRAY_MED);
  }

  // Artist name (scrolls if too long)
  if (appCtx.metadataLoaded) {
    const char* name = appCtx.meta1.artistMeta.name;
    drawScrollText(name, 2, 92, VIDEO_W - 4, COL_YELLOW,
                   &appCtx.scrollState.slots[0]);

    // Genre + album count
    char infoStr[48];
    if (appCtx.meta1.artistMeta.genre[0]) {
      snprintf(infoStr, sizeof(infoStr), "%s - %d albums",
               appCtx.meta1.artistMeta.genre,
               appCtx.meta1.artistMeta.albumCount);
    } else {
      snprintf(infoStr, sizeof(infoStr), "%d albums",
               appCtx.meta1.artistMeta.albumCount);
    }
    int infoW = textWidth(infoStr);
    drawText(infoStr, (VIDEO_W - infoW) / 2, 106, COL_GRAY_MED);
  } else {
    const char* dirName = appCtx.availableItems[appCtx.itemNavIndex];
    int nameW = textWidth(dirName);
    drawText(dirName, (VIDEO_W - nameW) / 2, 92, COL_YELLOW);
  }

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Draw Album Browser ─────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-10:   Artist name (white, centered)
//   Row 24:     Divider
//   Row 30-60:  "< Album Title >" (yellow, centered)
//   Row 68:     "of N" (gray)
//   Row 86:     Track count + year
//   Row 120:    Hint text

void drawMusicAlbumBrowser() {
  clearFrameBuf();

  // Artist name at top (scrolls if too long)
  if (appCtx.metadataLoaded) {
    drawScrollText(appCtx.meta1.artistMeta.name, 2, 8, VIDEO_W - 4, COL_WHITE,
                   &appCtx.scrollState.slots[0]);
  }

  // Divider
  drawHLine(10, 24, VIDEO_W - 20, COL_GRAY_DK);

  // Album title - centered (scrolls if too long)
  char albumStr[32];
  if (appCtx.seasonMetaLoaded) {
    strncpy(albumStr, appCtx.meta2.musicAlbumMeta.title, 24);
    albumStr[24] = '\0';
  } else {
    snprintf(albumStr, sizeof(albumStr), "Album %d", appCtx.currentSubItem);
  }

  // Navigation arrows
  if (appCtx.subItemNavIndex > 0) {
    drawText("<", 4, 48, COL_GRAY_MED);
  }
  if (appCtx.subItemNavIndex < appCtx.meta1.artistMeta.albumCount - 1) {
    drawText(">", VIDEO_W - 12, 48, COL_GRAY_MED);
  }
  // Album title between arrows
  drawScrollText(albumStr, 16, 48, VIDEO_W - 32, COL_YELLOW,
                 &appCtx.scrollState.slots[1]);

  // "of N"
  char ofStr[16];
  snprintf(ofStr, sizeof(ofStr), "of %d", appCtx.meta1.artistMeta.albumCount);
  int ofW = textWidth(ofStr);
  drawText(ofStr, (VIDEO_W - ofW) / 2, 68, COL_GRAY_MED);

  // Track count + year
  if (appCtx.seasonMetaLoaded) {
    char detailStr[48];
    snprintf(detailStr, sizeof(detailStr), "%d tracks - %s",
             appCtx.meta2.musicAlbumMeta.trackCount,
             appCtx.meta2.musicAlbumMeta.year);
    int detailW = textWidth(detailStr);
    drawText(detailStr, (VIDEO_W - detailW) / 2, 86, COL_GRAY_LT);
  }

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Draw Track Browser ─────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-10:   Album title (gray, left)
//   Row 12:     Divider
//   Row 14-80:  Thumbnail left + track info right
//   Row 88:     Divider
//   Row 93:     Hint text
//   Row 108:    Runtime info

void drawMusicTrackBrowser() {
  clearFrameBuf();

  // Album label
  if (appCtx.seasonMetaLoaded) {
    drawText(appCtx.meta2.musicAlbumMeta.title, 4, 4, COL_GRAY_MED);
  }

  // Divider
  drawHLine(2, 16, VIDEO_W - 4, COL_GRAY_DK);

  // Track info (right side) - drawn BEFORE thumbnail so thumbnail
  // overwrites any scrolling text that bleeds left into the thumbnail area
  int infoX = 114;

  // Track number
  char numStr[16];
  snprintf(numStr, sizeof(numStr), "Track %d/%d",
           appCtx.currentSubItem, appCtx.subItemCount);
  drawText(numStr, infoX, 22, COL_WHITE);

  // Track title (scrolls if too long)
  if (appCtx.episodeMetaLoaded) {
    drawScrollText(appCtx.meta3.trackMeta.title, infoX, 38,
                   VIDEO_W - infoX - 2, COL_YELLOW,
                   &appCtx.scrollState.slots[0]);

    // Runtime
    if (appCtx.meta3.trackMeta.runtimeSeconds > 0) {
      int mins = appCtx.meta3.trackMeta.runtimeSeconds / 60;
      int secs = appCtx.meta3.trackMeta.runtimeSeconds % 60;
      char rtStr[16];
      snprintf(rtStr, sizeof(rtStr), "%d:%02d", mins, secs);
      drawText(rtStr, infoX, 54, COL_GRAY_LT);
    }
  }

  // Thumbnail (left side) - drawn AFTER text so it overwrites any
  // scrolling text bleed into the thumbnail region
  char thumbPath[64];
  snprintf(thumbPath, sizeof(thumbPath), "%s/%s/A%02d/T%02d.raw",
           MUSIC_DIR, appCtx.currentItemDir,
           appCtx.currentSeason, appCtx.currentSubItem);
  if (!displayThumbnail(thumbPath, 2, 19)) {
    fillRect(2, 19, THUMB_W, THUMB_H, COL_GRAY_DK);
  }

  // Lower divider
  drawHLine(2, 88, VIDEO_W - 4, COL_GRAY_DK);

  // Artist name at bottom
  if (appCtx.metadataLoaded) {
    const char* artist = appCtx.meta1.artistMeta.name;
    int artW = textWidth(artist);
    drawText(artist, (VIDEO_W - artW) / 2, 93, COL_GRAY_LT);
  }

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Artist Input Handling ──────────────────────────────────────────────────

void handleMusicArtistInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate artists
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.itemNavIndex < appCtx.availableItemCount - 1) {
      appCtx.itemNavIndex++;
      strncpy(appCtx.currentItemDir, appCtx.availableItems[appCtx.itemNavIndex], ITEM_DIR_LEN - 1);
      appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadArtistMetadata();
      resetScrollState(&appCtx.scrollState);
      drawMusicArtistBrowser();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.itemNavIndex > 0) {
      appCtx.itemNavIndex--;
      strncpy(appCtx.currentItemDir, appCtx.availableItems[appCtx.itemNavIndex], ITEM_DIR_LEN - 1);
      appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadArtistMetadata();
      resetScrollState(&appCtx.scrollState);
      drawMusicArtistBrowser();
    }
  }

  // Select artist -> enter album browser
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;
    if (appCtx.metadataLoaded && appCtx.meta1.artistMeta.albumCount > 0) {
      appCtx.currentSeason = 1;  // Repurpose currentSeason as album number
      appCtx.subItemNavIndex = 0;
      loadMusicAlbumMetadata(appCtx.currentSeason);
      appCtx.nextState = STATE_MUSIC_ALBUM;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    } else {
      dbgPrint("No albums for artist: " + String(appCtx.currentItemDir));
    }
  }

  // Back to media selector
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_MEDIA_SELECTOR;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }

  // Mute
  if (raw.irMute) {
    raw.irMute = false;
    setMute(!isMute());
  }
}

// ─── Album Input Handling ───────────────────────────────────────────────────

void handleMusicAlbumInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate albums
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.subItemNavIndex < appCtx.meta1.artistMeta.albumCount - 1) {
      appCtx.subItemNavIndex++;
      appCtx.currentSeason = appCtx.subItemNavIndex + 1;  // Album number (1-based)
      appCtx.seasonMetaLoaded = false;
      loadMusicAlbumMetadata(appCtx.currentSeason);
      resetScrollState(&appCtx.scrollState);
      drawMusicAlbumBrowser();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.subItemNavIndex > 0) {
      appCtx.subItemNavIndex--;
      appCtx.currentSeason = appCtx.subItemNavIndex + 1;
      appCtx.seasonMetaLoaded = false;
      loadMusicAlbumMetadata(appCtx.currentSeason);
      resetScrollState(&appCtx.scrollState);
      drawMusicAlbumBrowser();
    }
  }

  // Select album -> enter track browser
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;
    if (appCtx.seasonMetaLoaded && appCtx.subItemCount > 0) {
      appCtx.currentSubItem = 1;  // Track number (1-based)
      loadTrackMetadata(appCtx.currentSeason, appCtx.currentSubItem);
      appCtx.nextState = STATE_MUSIC_TRACK;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    } else {
      dbgPrint("No tracks in album");
    }
  }

  // Back to artist browser
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_MUSIC_ARTIST;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }

  // Mute
  if (raw.irMute) {
    raw.irMute = false;
    setMute(!isMute());
  }
}

// ─── Track Input Handling ───────────────────────────────────────────────────

void handleMusicTrackInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate tracks
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.currentSubItem < appCtx.subItemCount) {
      appCtx.currentSubItem++;
      appCtx.episodeMetaLoaded = false;
      loadTrackMetadata(appCtx.currentSeason, appCtx.currentSubItem);
      resetScrollState(&appCtx.scrollState);
      drawMusicTrackBrowser();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.currentSubItem > 1) {
      appCtx.currentSubItem--;
      appCtx.episodeMetaLoaded = false;
      loadTrackMetadata(appCtx.currentSeason, appCtx.currentSubItem);
      resetScrollState(&appCtx.scrollState);
      drawMusicTrackBrowser();
    }
  }

  // Play track
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;

    char aviPath[64];
    snprintf(aviPath, sizeof(aviPath), "%s/%s/A%02d/T%02d.avi",
             MUSIC_DIR, appCtx.currentItemDir,
             appCtx.currentSeason, appCtx.currentSubItem);
    dbgPrint("Playing track: " + String(aviPath));
    startGenericPlayback(aviPath);
  }

  // Back to album browser
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_MUSIC_ALBUM;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }

  // Mute
  if (raw.irMute) {
    raw.irMute = false;
    setMute(!isMute());
  }
}

// ─── Auto-Advance ───────────────────────────────────────────────────────────
// Called when a track finishes playing to advance to the next one.

void advanceToNextTrack() {
  if (appCtx.currentSubItem < appCtx.subItemCount) {
    // Next track in current album
    appCtx.currentSubItem++;
    loadTrackMetadata(appCtx.currentSeason, appCtx.currentSubItem);

    char aviPath[64];
    snprintf(aviPath, sizeof(aviPath), "%s/%s/A%02d/T%02d.avi",
             MUSIC_DIR, appCtx.currentItemDir,
             appCtx.currentSeason, appCtx.currentSubItem);
    dbgPrint("Auto-advancing to track: " + String(aviPath));
    startGenericPlayback(aviPath);
  } else if (appCtx.subItemNavIndex < appCtx.meta1.artistMeta.albumCount - 1) {
    // Next album
    appCtx.subItemNavIndex++;
    appCtx.currentSeason = appCtx.subItemNavIndex + 1;
    loadMusicAlbumMetadata(appCtx.currentSeason);
    appCtx.currentSubItem = 1;
    loadTrackMetadata(appCtx.currentSeason, appCtx.currentSubItem);

    char aviPath[64];
    snprintf(aviPath, sizeof(aviPath), "%s/%s/A%02d/T%02d.avi",
             MUSIC_DIR, appCtx.currentItemDir,
             appCtx.currentSeason, appCtx.currentSubItem);
    dbgPrint("Auto-advancing to next album track: " + String(aviPath));
    startGenericPlayback(aviPath);
  } else {
    // End of all albums - return to track browser
    dbgPrint("End of music, returning to track browser");
    appCtx.nextState = STATE_MUSIC_TRACK;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }
}
