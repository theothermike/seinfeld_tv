//-------------------------------------------------------------------------------
//  TinyJukebox - Music Video Browser
//
//  Two-level browser: Collection -> Video list.
//  Scans /MusicVideos/ for collection directories containing collection.sdb.
//  Each collection has V##.avi / V##.sdb video files.
//  Supports auto-advance through videos in a collection.
//  Vol knob navigates, CH forward selects/plays, CH back goes up a level.
//-------------------------------------------------------------------------------

#include "uiHelpers.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Collection Scanning ────────────────────────────────────────────────────
// Scan /MusicVideos/ for subdirectories containing collection.sdb.

void scanMVCollections() {
  appCtx.availableItemCount = 0;

  File32 root;
  if (!root.open(MUSIC_VIDEOS_DIR, O_RDONLY)) {
    dbgPrint("Failed to open " MUSIC_VIDEOS_DIR);
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
    snprintf(sdbPath, sizeof(sdbPath), "%s/%s/collection.sdb", MUSIC_VIDEOS_DIR, dirName);
    File32 sdbCheck;
    if (sdbCheck.open(sdbPath, O_RDONLY)) {
      sdbCheck.close();
      if (appCtx.availableItemCount < MAX_ITEMS) {
        strncpy(appCtx.availableItems[appCtx.availableItemCount], dirName, ITEM_DIR_LEN - 1);
        appCtx.availableItems[appCtx.availableItemCount][ITEM_DIR_LEN - 1] = '\0';
        appCtx.availableItemCount++;
        dbgPrint("Found MV collection: " + String(dirName));
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
    loadCollectionMetadata();
  }

  dbgPrint("MV collections found: " + String(appCtx.availableItemCount));
}

// ─── Metadata Loading ───────────────────────────────────────────────────────

bool loadCollectionMetadata() {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/collection.sdb", MUSIC_VIDEOS_DIR, appCtx.currentItemDir);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.metadataLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(CollectionMetadata)) {
    dbgPrint("collection.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.metadataLoaded = false;
    return false;
  }

  f.read(&appCtx.meta1.collectionMeta, sizeof(CollectionMetadata));
  f.close();

  if (strncmp(appCtx.meta1.collectionMeta.magic, COLLECTION_MAGIC, 4) != 0) {
    dbgPrint("collection.sdb bad magic");
    appCtx.metadataLoaded = false;
    return false;
  }

  appCtx.metadataLoaded = true;
  appCtx.subItemCount = appCtx.meta1.collectionMeta.videoCount;
  dbgPrint("Loaded collection: " + String(appCtx.meta1.collectionMeta.name) +
           " (" + String(appCtx.subItemCount) + " videos)");
  return true;
}

bool loadVideoMetadata(int videoNum) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/V%02d.sdb",
           MUSIC_VIDEOS_DIR, appCtx.currentItemDir, videoNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(VideoMetadata)) {
    dbgPrint("V##.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  f.read(&appCtx.meta3.videoMeta, sizeof(VideoMetadata));
  f.close();

  if (strncmp(appCtx.meta3.videoMeta.magic, VIDEO_MAGIC, 4) != 0) {
    dbgPrint("V##.sdb bad magic");
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  appCtx.episodeMetaLoaded = true;
  dbgPrint("Loaded video V" + String(videoNum) + ": " +
           String(appCtx.meta3.videoMeta.title));
  return true;
}

// ─── Draw Collection Browser ────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "Music Videos" header (white, centered)
//   Row 16:     Divider
//   Row 20-86:  Thumbnail (108x67) centered
//   Row 90:     Collection name (yellow, centered)
//   Row 106:    Year + video count (gray)
//   Row 122:    Hint text

void drawMVCollectionBrowser() {
  clearFrameBuf();

  // Header
  const char* header = "Music Videos";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_WHITE);

  // Divider
  drawHLine(10, 16, VIDEO_W - 20, COL_GRAY_DK);

  // Thumbnail
  int thumbX = (VIDEO_W - THUMB_W) / 2;
  int thumbY = 20;
  char thumbPath[64];
  snprintf(thumbPath, sizeof(thumbPath), "%s/%s/collection.raw",
           MUSIC_VIDEOS_DIR, appCtx.availableItems[appCtx.itemNavIndex]);
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

  // Collection name
  if (appCtx.metadataLoaded) {
    const char* name = appCtx.meta1.collectionMeta.name;
    int nameW = textWidth(name);
    int nameX = (VIDEO_W - nameW) / 2;
    if (nameX < 2) nameX = 2;
    drawText(name, nameX, 92, COL_YELLOW);

    // Year + video count
    char infoStr[48];
    snprintf(infoStr, sizeof(infoStr), "%s - %d videos",
             appCtx.meta1.collectionMeta.year,
             appCtx.meta1.collectionMeta.videoCount);
    int infoW = textWidth(infoStr);
    drawText(infoStr, (VIDEO_W - infoW) / 2, 106, COL_GRAY_MED);
  } else {
    const char* dirName = appCtx.availableItems[appCtx.itemNavIndex];
    int nameW = textWidth(dirName);
    drawText(dirName, (VIDEO_W - nameW) / 2, 92, COL_YELLOW);
  }

  // Hint
  const char* hint = "VOL:nav  CH:select";
  int hintW = textWidth(hint);
  drawText(hint, (VIDEO_W - hintW) / 2, 122, COL_GRAY_DK);

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Draw Video Browser ─────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-10:   Collection name (gray, left)
//   Row 12:     Divider
//   Row 14-80:  Thumbnail left + video info right
//   Row 88:     Divider
//   Row 93:     Hint text
//   Row 108+:   Description

void drawMVVideoBrowser() {
  clearFrameBuf();

  // Collection label
  if (appCtx.metadataLoaded) {
    drawText(appCtx.meta1.collectionMeta.name, 4, 4, COL_GRAY_MED);
  }

  // Divider
  drawHLine(2, 16, VIDEO_W - 4, COL_GRAY_DK);

  // Thumbnail (left side)
  char thumbPath[64];
  snprintf(thumbPath, sizeof(thumbPath), "%s/%s/V%02d.raw",
           MUSIC_VIDEOS_DIR, appCtx.currentItemDir, appCtx.currentSubItem);
  if (!displayThumbnail(thumbPath, 2, 19)) {
    fillRect(2, 19, THUMB_W, THUMB_H, COL_GRAY_DK);
  }

  // Video info (right side)
  int infoX = 114;

  // Video number
  char numStr[16];
  snprintf(numStr, sizeof(numStr), "Video %d/%d",
           appCtx.currentSubItem, appCtx.subItemCount);
  drawText(numStr, infoX, 22, COL_WHITE);

  // Video title
  if (appCtx.episodeMetaLoaded) {
    char titleBuf[49];
    strncpy(titleBuf, appCtx.meta3.videoMeta.title, 48);
    titleBuf[48] = '\0';

    // Truncate for display area
    char line1[13], line2[13];
    memset(line1, 0, sizeof(line1));
    memset(line2, 0, sizeof(line2));

    if (strlen(titleBuf) <= 12) {
      strncpy(line1, titleBuf, 12);
    } else {
      int breakAt = 12;
      while (breakAt > 0 && titleBuf[breakAt] != ' ') breakAt--;
      if (breakAt == 0) breakAt = 12;
      strncpy(line1, titleBuf, breakAt);
      strncpy(line2, titleBuf + breakAt + (titleBuf[breakAt] == ' ' ? 1 : 0), 12);
    }

    drawText(line1, infoX, 38, COL_YELLOW);
    if (line2[0]) {
      drawText(line2, infoX, 52, COL_YELLOW);
    }

    // Artist
    if (appCtx.meta3.videoMeta.artist[0]) {
      drawText(appCtx.meta3.videoMeta.artist, infoX, 68, COL_GRAY_LT);
    }
  }

  // Lower divider
  drawHLine(2, 88, VIDEO_W - 4, COL_GRAY_DK);

  // Hint
  drawText("CH:play  VOL:nav", 4, 93, COL_GRAY_DK);

  // Description
  if (appCtx.episodeMetaLoaded && appCtx.meta3.videoMeta.description[0]) {
    char descBuf[57];
    strncpy(descBuf, appCtx.meta3.videoMeta.description, 56);
    descBuf[56] = '\0';

    char dLine1[27], dLine2[27];
    memset(dLine1, 0, sizeof(dLine1));
    memset(dLine2, 0, sizeof(dLine2));

    if (strlen(descBuf) <= 26) {
      strncpy(dLine1, descBuf, 26);
    } else {
      int breakAt = 26;
      while (breakAt > 0 && descBuf[breakAt] != ' ') breakAt--;
      if (breakAt == 0) breakAt = 26;
      strncpy(dLine1, descBuf, breakAt);
      strncpy(dLine2, descBuf + breakAt + (descBuf[breakAt] == ' ' ? 1 : 0), 26);
    }

    drawText(dLine1, 4, 108, COL_GRAY_LT);
    if (dLine2[0]) {
      drawText(dLine2, 4, 122, COL_GRAY_LT);
    }
  }

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Collection Input Handling ──────────────────────────────────────────────

void handleMVCollectionInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate collections
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.itemNavIndex < appCtx.availableItemCount - 1) {
      appCtx.itemNavIndex++;
      strncpy(appCtx.currentItemDir, appCtx.availableItems[appCtx.itemNavIndex], ITEM_DIR_LEN - 1);
      appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadCollectionMetadata();
      drawMVCollectionBrowser();
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
      loadCollectionMetadata();
      drawMVCollectionBrowser();
    }
  }

  // Select collection -> enter video browser
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;
    if (appCtx.subItemCount > 0) {
      appCtx.currentSubItem = 1;
      appCtx.subItemNavIndex = 0;
      loadVideoMetadata(appCtx.currentSubItem);
      appCtx.nextState = STATE_MV_VIDEO_BROWSER;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    } else {
      dbgPrint("No videos in collection: " + String(appCtx.currentItemDir));
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

void handleMVVideoBrowserInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate videos
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.currentSubItem < appCtx.subItemCount) {
      appCtx.currentSubItem++;
      appCtx.episodeMetaLoaded = false;
      loadVideoMetadata(appCtx.currentSubItem);
      drawMVVideoBrowser();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.currentSubItem > 1) {
      appCtx.currentSubItem--;
      appCtx.episodeMetaLoaded = false;
      loadVideoMetadata(appCtx.currentSubItem);
      drawMVVideoBrowser();
    }
  }

  // Play video
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;

    char aviPath[64];
    snprintf(aviPath, sizeof(aviPath), "%s/%s/V%02d.avi",
             MUSIC_VIDEOS_DIR, appCtx.currentItemDir, appCtx.currentSubItem);
    dbgPrint("Playing music video: " + String(aviPath));
    startGenericPlayback(aviPath);
  }

  // Back to collection browser
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_MV_COLLECTION;
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
// Called when a music video finishes playing to advance to the next one.

void advanceToNextMusicVideo() {
  if (appCtx.currentSubItem < appCtx.subItemCount) {
    appCtx.currentSubItem++;
    loadVideoMetadata(appCtx.currentSubItem);

    char aviPath[64];
    snprintf(aviPath, sizeof(aviPath), "%s/%s/V%02d.avi",
             MUSIC_VIDEOS_DIR, appCtx.currentItemDir, appCtx.currentSubItem);
    dbgPrint("Auto-advancing to: " + String(aviPath));

    appCtx.nextState = STATE_PLAYBACK;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;

    // startGenericPlayback will be called when we enter STATE_PLAYBACK
    // But we need to actually start it since generic playback is started before entering state
    startGenericPlayback(aviPath);
  } else {
    // End of collection - return to video browser
    dbgPrint("End of MV collection, returning to browser");
    appCtx.nextState = STATE_MV_VIDEO_BROWSER;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }
}
