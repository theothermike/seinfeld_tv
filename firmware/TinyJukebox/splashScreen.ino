//-------------------------------------------------------------------------------
//  TinyJukebox - Splash Screen
//
//  Renders "TinyJukebox" branding centered on screen during startup.
//  Displayed for SPLASH_DURATION_MS before transitioning to media selector.
//-------------------------------------------------------------------------------

#include "uiHelpers.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Splash Screen ──────────────────────────────────────────────────────────

void drawSplashScreen() {
  clearFrameBuf();

  // Title: "TinyJukebox" centered vertically and horizontally
  const char* title = "TinyJukebox";
  int titleW = textWidth(title);
  int titleX = (VIDEO_W - titleW) / 2;
  int titleY = 40;
  drawText(title, titleX, titleY, COL_YELLOW);

  // Decorative lines above and below the title
  int lineX = titleX - 8;
  int lineW = titleW + 16;
  if (lineX < 4) { lineX = 4; lineW = VIDEO_W - 8; }
  drawHLine(lineX, titleY - 6, lineW, COL_GRAY_MED);
  drawHLine(lineX, titleY + 18, lineW, COL_GRAY_MED);

  // Subtitle
  const char* subtitle = "Multi-Media Player";
  int subW = textWidth(subtitle);
  int subX = (VIDEO_W - subW) / 2;
  drawText(subtitle, subX, 70, COL_GRAY_LT);

  // Version / attribution
  const char* version = "v1.0";
  int verW = textWidth(version);
  drawText(version, (VIDEO_W - verW) / 2, 100, COL_GRAY_DK);

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);

  dbgPrint("Splash screen drawn");
}
