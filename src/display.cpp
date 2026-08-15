#include "display.h"
#include "globals.h"

static void drawVersionHeader() {
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.drawString(String("M5ChainOSC v") + APP_VERSION,
                        M5.Display.width() / 2, 2);
}

static const int MAIN_FEEDBACK_Y = 52;

static void drawMainFeedbackArea(bool clearArea) {
  const int screenW = M5.Display.width();
  const int screenH = M5.Display.height();
  const int cols    = 21;

  if (clearArea) {
    M5.Display.fillRect(0, MAIN_FEEDBACK_Y, screenW,
                        screenH - MAIN_FEEDBACK_Y, TFT_BLACK);
  }

  if (hasOscFeedback) {
    M5.Display.drawFastHLine(2, MAIN_FEEDBACK_Y, screenW - 4, TFT_DARKGREY);

    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setCursor(2, 56);
    String n = lastOscName;
    if ((int)n.length() > cols) n = n.substring(0, cols);
    M5.Display.println(n.length() ? n : "OSC");

    const int addrStartY = 70;
    const int lineH      = 12;
    const int valY       = screenH - 14;
    const int maxAddrLines = max(1, (valY - 2 - addrStartY) / lineH);

    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setCursor(2, addrStartY);
    M5.Display.print("A:");
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);

    String addr = lastOscAddr;
    int firstCols = max(8, cols - 2);
    int pos = 0;
    int line = 0;
    while (pos < (int)addr.length() && line < maxAddrLines) {
      int take = (line == 0) ? firstCols : cols;
      int end  = min(pos + take, (int)addr.length());
      String part = addr.substring(pos, end);
      if (line == 0) {
        M5.Display.setCursor(14, addrStartY);
        M5.Display.println(part);
      } else {
        M5.Display.setCursor(2, addrStartY + line * lineH);
        M5.Display.println(part);
      }
      pos = end;
      line++;
    }

    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setCursor(2, valY);
    M5.Display.print("V:");
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    String v = lastOscVal;
    if ((int)v.length() > cols - 2) v = v.substring(0, cols - 2);
    M5.Display.print(v);
  } else {
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setCursor(2, 56);
    M5.Display.println(deviceCount > 0 ? "Waiting..." : "No Device");
  }
}

void applyDisplayRotation() {
  if (displayRotation > 3) displayRotation = 0;
  M5.Display.setRotation(displayRotation);
}

void showMessage(const char* a, const char* b) {
  M5.Display.fillScreen(TFT_BLACK);
  drawVersionHeader();
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  int cx = M5.Display.width() / 2;
  int cy = M5.Display.height() / 2;
  if (b[0] == '\0') {
    M5.Display.drawString(a, cx, cy);
  } else {
    M5.Display.drawString(a, cx, cy - 16);
    M5.Display.drawString(b, cx, cy + 16);
  }
}

void showWifiConnected(const String& ipAddress) {
  M5.Display.fillScreen(TFT_BLACK);
  drawVersionHeader();
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  int cx = M5.Display.width() / 2;
  int cy = M5.Display.height() / 2;
  M5.Display.drawString("WiFi OK", cx, cy - 9);
  M5.Display.drawString(ipAddress, cx, cy + 9);
}

void drawMainScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setTextSize(1);

  const int cols    = 21;

  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(2, 2);
  M5.Display.println(String("M5ChainOSC v") + APP_VERSION);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(2, 16);
  String ipLine = "IP:" + ipStr;
  if ((int)ipLine.length() > cols) ipLine = ipLine.substring(0, cols);
  M5.Display.println(ipLine);

  M5.Display.setCursor(2, 28);
  String hostLine = "H:" + hostStr;
  if ((int)hostLine.length() > cols) hostLine = hostLine.substring(0, cols);
  M5.Display.println(hostLine);

  M5.Display.setCursor(2, 40);
  M5.Display.printf("Dev:%d", deviceCount);

  drawMainFeedbackArea(false);
}

void showOscFeedback(const String& name, const String& address, const String& value) {
  lastOscName  = name;
  lastOscAddr  = address;
  lastOscVal   = value;
  hasOscFeedback = true;
  drawMainFeedbackArea(true);
}

void showResetProgress(unsigned long heldMs) {
  if (heldMs > RESET_HOLD_MS) heldMs = RESET_HOLD_MS;
  int percent = (int)((heldMs * 100) / RESET_HOLD_MS);
  if (percent == lastResetDrawPercent && heldMs < RESET_HOLD_MS) return;
  lastResetDrawPercent = percent;

  int cx = M5.Display.width() / 2;
  int cy = M5.Display.height() / 2 - 6;
  M5.Display.fillScreen(TFT_BLACK);
  drawVersionHeader();
  M5.Display.drawCircle(cx, cy, 36, TFT_DARKGREY);
  M5.Display.drawCircle(cx, cy, 28, TFT_DARKGREY);
  float deg = (heldMs * 360.0f) / (float)RESET_HOLD_MS;
  if (deg < 1) deg = 1;
  M5.Display.fillArc(cx, cy, 28, 36, -90, (int)(-90 + deg), TFT_RED);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("RESET", cx, cy - 8);
  char buf[16];
  sprintf(buf, "%d%%", percent);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.drawString(buf, cx, cy + 8);
}
