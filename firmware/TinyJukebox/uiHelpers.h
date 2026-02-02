//-------------------------------------------------------------------------------
//  TinyJukebox - UI Drawing Helpers & Color Constants
//
//  Shared color definitions and framebuffer drawing primitives used by
//  all browser and menu screens. Included by showBrowser.ino and other
//  UI .ino files.
//-------------------------------------------------------------------------------

#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#include <stdint.h>
#include <string.h>

// ─── Color Constants (RGB565 for display) ───────────────────────────────────

#define COL_BLACK     0x0000
#define COL_WHITE     0xFFFF
#define COL_YELLOW    0xFFE0
#define COL_GRAY_DK   0x4208
#define COL_GRAY_MED  0x8410
#define COL_GRAY_LT   0xC618
#define COL_BG        0x0000   // Black background

// ─── Framebuffer Drawing Helpers ────────────────────────────────────────────
// These are defined as inline so they can live in a header included by
// multiple .ino files (which Arduino concatenates into one translation unit).

static inline uint16_t rgb565BE(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  return val;
}

static inline void clearFrameBuf() {
  extern uint16_t frameBuf[];
  memset(frameBuf, 0, VIDEO_W * VIDEO_H * 2);
}

static inline void fillRect(int x, int y, int w, int h, uint16_t color) {
  extern uint16_t frameBuf[];
  for (int row = y; row < y + h && row < VIDEO_H; row++) {
    if (row < 0) continue;
    for (int col = x; col < x + w && col < VIDEO_W; col++) {
      if (col < 0) continue;
      frameBuf[row * VIDEO_W + col] = color;
    }
  }
}

static inline void drawHLine(int x, int y, int w, uint16_t color) {
  extern uint16_t frameBuf[];
  if (y < 0 || y >= VIDEO_H) return;
  for (int col = x; col < x + w && col < VIDEO_W; col++) {
    if (col >= 0) frameBuf[y * VIDEO_W + col] = color;
  }
}

// Draw text using GraphicsBuffer2 into frameBuf
static inline void drawText(const char* text, int x, int y, uint16_t color) {
  extern uint16_t frameBuf[];
  extern GraphicsBuffer2 screenBuffer;
  screenBuffer.setBuffer((uint8_t*)frameBuf);
  screenBuffer.setWidth(VIDEO_W);
  screenBuffer.fontColor(color, COL_BG);
  screenBuffer.setCursor(x, y);
  screenBuffer.print(text);
}

// Measure text width (approximate - 8px per char for liberationSansNarrow_14pt)
static inline int textWidth(const char* text) {
  return strlen(text) * 8;
}

#endif // UI_HELPERS_H
