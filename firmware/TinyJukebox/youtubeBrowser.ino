//-------------------------------------------------------------------------------
//  TinyJukebox - YouTube Browser
//
//  Two-level browser: Playlist -> Video list.
//  Scans /YouTube/ for playlist directories containing playlist.sdb.
//  Each playlist has Y##.avi / Y##.sdb video files.
//  Supports auto-advance through videos in a playlist.
//  Vol knob navigates, CH forward selects/plays, CH back goes up a level.
//-------------------------------------------------------------------------------

#include "uiHelpers.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Playlist Scanning ─────────────────────────────────────────────────────
// Scan /YouTube/ for subdirectories containing playlist.sdb.

void scanYouTubePlaylists() {
  appCtx.availableItemCount = 0;

  File32 root;
  if (!root.open(YOUTUBE_DIR, O_RDONLY)) {
    dbgPrint("Failed to open " YOUTUBE_DIR);
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
    snprintf(sdbPath, sizeof(sdbPath), "%s/%s/playlist.sdb", YOUTUBE_DIR, dirName);
    File32 sdbCheck;
    if (sdbCheck.open(sdbPath, O_RDONLY)) {
      sdbCheck.close();
      if (appCtx.availableItemCount < MAX_ITEMS) {
        strncpy(appCtx.availableItems[appCtx.availableItemCount], dirName, ITEM_DIR_LEN - 1);
        appCtx.availableItems[appCtx.availableItemCount][ITEM_DIR_LEN - 1] = '\0';
        appCtx.availableItemCount++;
        dbgPrint("Found YouTube playlist: " + String(dirName));
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
    loadYouTubePlaylistMetadata();
  }

  dbgPrint("YouTube playlists found: " + String(appCtx.availableItemCount));
}

// ─── Metadata Loading ───────────────────────────────────────────────────────

bool loadYouTubePlaylistMetadata() {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/playlist.sdb", YOUTUBE_DIR, appCtx.currentItemDir);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.metadataLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(YouTubePlaylistMetadata)) {
    dbgPrint("playlist.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.metadataLoaded = false;
    return false;
  }

  f.read(&appCtx.meta1.youtubePlaylistMeta, sizeof(YouTubePlaylistMetadata));
  f.close();

  if (strncmp(appCtx.meta1.youtubePlaylistMeta.magic, YOUTUBE_PLAYLIST_MAGIC, 4) != 0) {
    dbgPrint("playlist.sdb bad magic");
    appCtx.metadataLoaded = false;
    return false;
  }

  appCtx.metadataLoaded = true;
  appCtx.subItemCount = appCtx.meta1.youtubePlaylistMeta.videoCount;
  dbgPrint("Loaded YouTube playlist: " + String(appCtx.meta1.youtubePlaylistMeta.name) +
           " (" + String(appCtx.subItemCount) + " videos)");
  return true;
}

bool loadYouTubeVideoMetadata(int videoNum) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/Y%02d.sdb",
           YOUTUBE_DIR, appCtx.currentItemDir, videoNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(YouTubeVideoMetadata)) {
    dbgPrint("Y##.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  f.read(&appCtx.meta3.youtubeVideoMeta, sizeof(YouTubeVideoMetadata));
  f.close();

  if (strncmp(appCtx.meta3.youtubeVideoMeta.magic, YOUTUBE_VIDEO_MAGIC, 4) != 0) {
    dbgPrint("Y##.sdb bad magic");
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  appCtx.episodeMetaLoaded = true;
  dbgPrint("Loaded YouTube video Y" + String(videoNum) + ": " +
           String(appCtx.meta3.youtubeVideoMeta.title));
  return true;
}

// ─── Draw Playlist Browser ─────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "YouTube" header (white, centered)
//   Row 16:     Divider
//   Row 20-86:  Thumbnail (108x67) centered
//   Row 90:     Playlist name (yellow, centered)
//   Row 106:    Uploader + video count (gray)
//   Row 122:    Hint text

void drawYouTubePlaylistBrowser() {
  clearFrameBuf();

  // Header
  const char* header = "YouTube";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_WHITE);

  // Divider
  drawHLine(10, 16, VIDEO_W - 20, COL_GRAY_DK);

  // Thumbnail
  int thumbX = (VIDEO_W - THUMB_W) / 2;
  int thumbY = 20;
  char thumbPath[64];
  snprintf(thumbPath, sizeof(thumbPath), "%s/%s/playlist.raw",
           YOUTUBE_DIR, appCtx.availableItems[appCtx.itemNavIndex]);
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

  // Playlist name (scrolls if too long)
  if (appCtx.metadataLoaded) {
    const char* name = appCtx.meta1.youtubePlaylistMeta.name;
    drawScrollText(name, 2, 92, VIDEO_W - 4, COL_YELLOW,
                   &appCtx.scrollState.slots[0]);

    // Uploader + video count
    char infoStr[48];
    snprintf(infoStr, sizeof(infoStr), "%s - %d videos",
             appCtx.meta1.youtubePlaylistMeta.uploader,
             appCtx.meta1.youtubePlaylistMeta.videoCount);
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

// ─── Draw Video Browser ─────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-10:   Playlist name (gray, left)
//   Row 12:     Divider
//   Row 14-80:  Thumbnail left + video info right
//   Row 88:     Divider
//   Row 93:     Hint text
//   Row 108+:   Description

void drawYouTubeVideoBrowser() {
  clearFrameBuf();

  // Playlist label
  if (appCtx.metadataLoaded) {
    drawText(appCtx.meta1.youtubePlaylistMeta.name, 4, 4, COL_GRAY_MED);
  }

  // Divider
  drawHLine(2, 16, VIDEO_W - 4, COL_GRAY_DK);

  // Video info (right side) - drawn BEFORE thumbnail so thumbnail
  // overwrites any scrolling text that bleeds left into the thumbnail area
  int infoX = 114;

  // Video number
  char numStr[16];
  snprintf(numStr, sizeof(numStr), "Video %d/%d",
           appCtx.currentSubItem, appCtx.subItemCount);
  drawText(numStr, infoX, 22, COL_WHITE);

  // Video title (scrolls if too long)
  if (appCtx.episodeMetaLoaded) {
    drawScrollText(appCtx.meta3.youtubeVideoMeta.title, infoX, 38,
                   VIDEO_W - infoX - 2, COL_YELLOW,
                   &appCtx.scrollState.slots[0]);

    // Uploader
    if (appCtx.meta3.youtubeVideoMeta.uploader[0]) {
      drawText(appCtx.meta3.youtubeVideoMeta.uploader, infoX, 54, COL_GRAY_LT);
    }
  }

  // Thumbnail (left side) - drawn AFTER text so it overwrites any
  // scrolling text bleed into the thumbnail region
  char thumbPath[64];
  snprintf(thumbPath, sizeof(thumbPath), "%s/%s/Y%02d.raw",
           YOUTUBE_DIR, appCtx.currentItemDir, appCtx.currentSubItem);
  if (!displayThumbnail(thumbPath, 2, 19)) {
    fillRect(2, 19, THUMB_W, THUMB_H, COL_GRAY_DK);
  }

  // Lower divider
  drawHLine(2, 88, VIDEO_W - 4, COL_GRAY_DK);

  // Description (scrolls if too long)
  if (appCtx.episodeMetaLoaded && appCtx.meta3.youtubeVideoMeta.description[0]) {
    drawScrollText(appCtx.meta3.youtubeVideoMeta.description, 4, 93,
                   VIDEO_W - 8, COL_GRAY_LT,
                   &appCtx.scrollState.slots[1]);
  }

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Playlist Input Handling ────────────────────────────────────────────────

void handleYouTubePlaylistInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate playlists
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.itemNavIndex < appCtx.availableItemCount - 1) {
      appCtx.itemNavIndex++;
      strncpy(appCtx.currentItemDir, appCtx.availableItems[appCtx.itemNavIndex], ITEM_DIR_LEN - 1);
      appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadYouTubePlaylistMetadata();
      resetScrollState(&appCtx.scrollState);
      drawYouTubePlaylistBrowser();
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
      loadYouTubePlaylistMetadata();
      resetScrollState(&appCtx.scrollState);
      drawYouTubePlaylistBrowser();
    }
  }

  // Select playlist -> enter video browser
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;
    if (appCtx.subItemCount > 0) {
      appCtx.currentSubItem = 1;
      appCtx.subItemNavIndex = 0;
      loadYouTubeVideoMetadata(appCtx.currentSubItem);
      appCtx.nextState = STATE_YOUTUBE_VIDEO_BROWSER;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    } else {
      dbgPrint("No videos in playlist: " + String(appCtx.currentItemDir));
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

// ─── Video Browser Input Handling ───────────────────────────────────────────

void handleYouTubeVideoBrowserInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate videos
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.currentSubItem < appCtx.subItemCount) {
      appCtx.currentSubItem++;
      appCtx.episodeMetaLoaded = false;
      loadYouTubeVideoMetadata(appCtx.currentSubItem);
      resetScrollState(&appCtx.scrollState);
      drawYouTubeVideoBrowser();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.currentSubItem > 1) {
      appCtx.currentSubItem--;
      appCtx.episodeMetaLoaded = false;
      loadYouTubeVideoMetadata(appCtx.currentSubItem);
      resetScrollState(&appCtx.scrollState);
      drawYouTubeVideoBrowser();
    }
  }

  // Play video
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;

    char aviPath[64];
    snprintf(aviPath, sizeof(aviPath), "%s/%s/Y%02d.avi",
             YOUTUBE_DIR, appCtx.currentItemDir, appCtx.currentSubItem);
    dbgPrint("Playing YouTube video: " + String(aviPath));
    startGenericPlayback(aviPath);
  }

  // Back to playlist browser
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_YOUTUBE_PLAYLIST;
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
// Called when a YouTube video finishes playing to advance to the next one.

void advanceToNextYouTubeVideo() {
  if (appCtx.currentSubItem < appCtx.subItemCount) {
    appCtx.currentSubItem++;
    loadYouTubeVideoMetadata(appCtx.currentSubItem);

    char aviPath[64];
    snprintf(aviPath, sizeof(aviPath), "%s/%s/Y%02d.avi",
             YOUTUBE_DIR, appCtx.currentItemDir, appCtx.currentSubItem);
    dbgPrint("Auto-advancing to: " + String(aviPath));

    appCtx.nextState = STATE_PLAYBACK;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;

    startGenericPlayback(aviPath);
  } else {
    // End of playlist - return to video browser
    dbgPrint("End of YouTube playlist, returning to browser");
    appCtx.nextState = STATE_YOUTUBE_VIDEO_BROWSER;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }
}
