#include <Arduino_GFX_Library.h>
#include <TouchScreen.h>
#include "RTC.h"

// ==========================================
//        USER CUSTOMIZATION SETTINGS
// ==========================================
// --- FEATURE TOGGLES ---
#define SHOW_SECONDS       false   // true = "12:30:45 PM", false = "12:30 PM"
#define SHOW_HI_LO         true    // true = Show High/Low temps, false = Hide them

// --- TEXT SIZES ---
#define SIZE_TIME          5
#define SIZE_DATE          2
#define SIZE_TEMP          4
#define SIZE_COND          2

// --- COLOR PALETTE (Hex RGB565) ---
#define COLOR_BG           0x0000  // Dark slate background
#define COLOR_CARD         0x2104  // Dark gray container
#define COLOR_TEXT_MAIN    0xFFFF  // Pure White
#define COLOR_TEXT_DIM     0x8410  // Soft Gray
#define COLOR_ACCENT_CYAN  0x07FF  // Neon Blue
#define COLOR_ACCENT_AMBER 0xFD20  // Warm Orange
#define COLOR_ACCENT_GREEN 0x07E0  // Bright Green
#define COLOR_ALERT_RED    0xF800  // Disconnect Red
// ==========================================

Arduino_DataBus *bus = new Arduino_UNOPAR8();  
Arduino_GFX *gfx = new Arduino_ILI9486(bus, DF_GFX_RST); 
TouchScreen ts = TouchScreen(6, A1, A2, 7, 280);

// System State
bool isConnected = false;
unsigned long lastSyncTime = 0;
int currentPage = 0; 

// Live variables (Under the hood)
String live_timeStr, live_dateStr, live_tempStr, live_condStr, live_hiStr, live_loStr;
String live_wspeedStr = "0", live_wdegStr = "0", live_dewStr, live_humStr, live_pressStr;
String live_f1Time, live_f1Temp, live_f2Time, live_f2Temp;

// Rendered variables (What is physically on the screen)
String rendered_timeStr, rendered_dateStr, rendered_tempStr, rendered_condStr, rendered_hiLo;
String rendered_wspeed, rendered_wdeg, rendered_hum, rendered_dew, rendered_press;
String rendered_f1, rendered_status;
int rendered_heading = -999;

bool readingPacket = false;
String incomingData = "";

const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* monthNames[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int getDayOfWeek(int y, int m, int d) {
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// --- DELTA RENDER ENGINE ---
void updateCenteredText(String &rendered, String live, int y, int h, int size, uint16_t color) {
  if (rendered == live) return; // Completely prevents flicker
  
  gfx->fillRect(0, y, 320, h, COLOR_BG); 
  int width = live.length() * 6 * size;
  gfx->setTextColor(color);
  gfx->setTextSize(size);
  gfx->setCursor((320 - width) / 2, y);
  gfx->print(live);
  rendered = live;
}

void drawCardValue(int x, int y, String &rendered, String live, uint16_t color) {
  if (rendered == live) return;
  gfx->fillRect(x, y, 135, 20, COLOR_CARD); 
  gfx->setTextColor(color);
  gfx->setTextSize(2);
  gfx->setCursor(x, y);
  gfx->print(live);
  rendered = live;
}

void drawCompassNeedle(int cx, int cy, int radius, int liveHeading) {
  if (rendered_heading == liveHeading) return; 
  
  gfx->fillCircle(cx, cy, radius - 2, COLOR_BG); 
  float rad = liveHeading * 3.14159 / 180.0;
  
  int tipX = cx + (radius - 10) * sin(rad);
  int tipY = cy - (radius - 10) * cos(rad);
  
  float radL = (liveHeading - 90) * 3.14159 / 180.0;
  float radR = (liveHeading + 90) * 3.14159 / 180.0;
  
  int leftX = cx + 6 * sin(radL);
  int leftY = cy - 6 * cos(radL);
  int rightX = cx + 6 * sin(radR);
  int rightY = cy - 6 * cos(radR);
  
  gfx->fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, COLOR_ACCENT_AMBER);
  
  int tailX = cx - 12 * sin(rad);
  int tailY = cy + 12 * cos(rad);
  gfx->fillTriangle(tailX, tailY, leftX, leftY, rightX, rightY, COLOR_TEXT_DIM);
  
  gfx->fillCircle(cx, cy, 3, COLOR_TEXT_MAIN); 
  rendered_heading = liveHeading;
}

void drawPagination() {
  gfx->fillRect(0, 225, 320, 15, COLOR_BG); 
  for (int i = 0; i < 3; i++) {
    gfx->fillCircle(130 + (i * 30), 232, (i == currentPage) ? 5 : 3, (i == currentPage) ? COLOR_TEXT_MAIN : COLOR_CARD);
  }
}

void drawPage(bool forceClear) {
  if (forceClear) {
     gfx->fillScreen(COLOR_BG);
     drawPagination();
     
     if (currentPage > 0) gfx->fillTriangle(12, 120, 28, 105, 28, 135, COLOR_TEXT_DIM); 
     if (currentPage < 2) gfx->fillTriangle(308, 120, 292, 105, 292, 135, COLOR_TEXT_DIM); 
     
     // Wipe memory so the UI loop is forced to redraw the current live values
     rendered_timeStr = ""; rendered_dateStr = ""; rendered_tempStr = "";
     rendered_condStr = ""; rendered_hiLo = "";
     rendered_wspeed = ""; rendered_wdeg = ""; rendered_heading = -999;
     rendered_hum = ""; rendered_dew = ""; rendered_press = "";
     rendered_f1 = ""; rendered_status = "";

     // Draw static page shells
     if (currentPage == 1) {
        gfx->drawCircle(160, 115, 60, COLOR_TEXT_DIM);
        gfx->setTextColor(COLOR_TEXT_DIM); gfx->setTextSize(1);
        gfx->setCursor(157, 45); gfx->print("N"); gfx->setCursor(157, 178); gfx->print("S");
        gfx->setCursor(225, 112); gfx->print("E"); gfx->setCursor(88, 112); gfx->print("W");
     }
     else if (currentPage == 2) {
        gfx->fillRoundRect(10, 40, 145, 75, 8, COLOR_CARD);
        gfx->setTextColor(COLOR_TEXT_DIM); gfx->setTextSize(1); gfx->setCursor(20, 50); gfx->print("Humidity");
        
        gfx->fillRoundRect(165, 40, 145, 75, 8, COLOR_CARD);
        gfx->setCursor(175, 50); gfx->print("Dew Point");
        
        gfx->fillRoundRect(10, 125, 300, 90, 8, COLOR_CARD);
        gfx->setCursor(20, 135); gfx->print("Atmo & Forecast");
     }
  }
}

void updateLiveUI() {
  if (currentPage == 0) {
     String status = isConnected ? "SYNCED" : "OFFLINE";
     if (rendered_status != status) {
        gfx->fillRect(250, 0, 70, 20, COLOR_BG);
        gfx->setTextColor(isConnected ? COLOR_ACCENT_GREEN : COLOR_ALERT_RED);
        gfx->setTextSize(1); gfx->setCursor(260, 10); gfx->print(status);
        rendered_status = status;
     }

     updateCenteredText(rendered_timeStr, live_timeStr, 40, 40, SIZE_TIME, COLOR_TEXT_MAIN); 
     updateCenteredText(rendered_dateStr, live_dateStr, 90, 20, SIZE_DATE, COLOR_TEXT_DIM);
     updateCenteredText(rendered_tempStr, live_tempStr, 130, 40, SIZE_TEMP, COLOR_ACCENT_AMBER);
     updateCenteredText(rendered_condStr, live_condStr, 180, 20, SIZE_COND, COLOR_ACCENT_CYAN);
     
     if (SHOW_HI_LO) {
        String hiLo = "H: " + live_hiStr + "  L: " + live_loStr;
        updateCenteredText(rendered_hiLo, hiLo, 205, 20, SIZE_DATE, COLOR_TEXT_MAIN);
     }
  }
  else if (currentPage == 1) {
     updateCenteredText(rendered_dateStr, "Wind & Heading", 10, 20, SIZE_DATE, COLOR_TEXT_DIM);
     drawCompassNeedle(160, 115, 50, live_wdegStr.toInt());
     updateCenteredText(rendered_wspeed, "Speed: " + live_wspeedStr, 190, 20, SIZE_DATE, COLOR_TEXT_MAIN);
     updateCenteredText(rendered_wdeg, "Heading: " + live_wdegStr + " deg", 210, 20, SIZE_DATE, COLOR_ACCENT_CYAN);
  }
  else if (currentPage == 2) {
     updateCenteredText(rendered_dateStr, "Environment", 10, 20, SIZE_DATE, COLOR_TEXT_DIM);
     drawCardValue(20, 75, rendered_hum, live_humStr, COLOR_ACCENT_CYAN);
     drawCardValue(175, 75, rendered_dew, live_dewStr, COLOR_ACCENT_GREEN);
     
     if (rendered_press != live_pressStr) {
         gfx->fillRect(20, 155, 280, 15, COLOR_CARD);
         gfx->setTextColor(COLOR_ACCENT_AMBER); gfx->setTextSize(1);
         gfx->setCursor(20, 155); gfx->print("Pressure: " + live_pressStr);
         rendered_press = live_pressStr;
     }
     if (rendered_f1 != live_f1Time) {
         gfx->fillRect(20, 175, 280, 35, COLOR_CARD);
         gfx->setTextColor(COLOR_TEXT_MAIN); gfx->setCursor(20, 175);
         gfx->print("Next: " + live_f1Time + " -> " + live_f1Temp);
         gfx->setCursor(20, 195); gfx->print("Then: " + live_f2Time + " -> " + live_f2Temp);
         rendered_f1 = live_f1Time;
     }
  }
}

void updateRTC() {
  RTCTime currentTime;
  RTC.getTime(currentTime);

  int h24 = currentTime.getHour();
  int m = currentTime.getMinutes();
  int s = currentTime.getSeconds();
  bool pm = (h24 >= 12);
  int h12 = h24 % 12;
  if (h12 == 0) h12 = 12;

  char timeBuf[16];
  if (SHOW_SECONDS) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d %s", h12, m, s, pm ? "PM" : "AM");
  else snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", h12, m, pm ? "PM" : "AM");
  live_timeStr = String(timeBuf);

  int day = currentTime.getDayOfMonth();
  int mon = Month2int(currentTime.getMonth());
  int yr = currentTime.getYear();

  if (mon >= 1 && mon <= 12 && yr >= 2024) {
    int dow = getDayOfWeek(yr, mon, day);
    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d", dayNames[dow], monthNames[mon], day);
    live_dateStr = String(dateBuf);
  }
}

void checkTouchButtons() {
  TSPoint p = ts.getPoint();
  pinMode(6, OUTPUT); pinMode(A2, OUTPUT); pinMode(A1, OUTPUT); pinMode(7, OUTPUT);

  if (p.z > 50 && p.z < 1000) {
     int x = map(p.x, 928, 215, 0, 320);
     if (x < 50 && currentPage > 0) {
         currentPage--; drawPage(true); delay(300);
     } else if (x > 270 && currentPage < 2) {
         currentPage++; drawPage(true); delay(300);
     }
  }
}

void setup() {
  Serial.begin(9600);
  RTC.begin(); 
  
  pinMode(A4, OUTPUT);
  digitalWrite(A4, LOW); delay(1); digitalWrite(A4, HIGH); delay(1);

  gfx->begin();
  gfx->setRotation(1);
  
  updateRTC();
  drawPage(true);
}

void loop() {
  checkTouchButtons();

  if (!isConnected && Serial) {
    static unsigned long lastReadyShout = 0;
    if (millis() - lastReadyShout > 300) {
      Serial.println("ARDUINO_READY");
      lastReadyShout = millis();
    }
  }

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '<') {
      readingPacket = true;
      incomingData = "";
    } 
    else if (c == '>') {
      readingPacket = false;
      
      // Invisible heartbeat to maintain Sync status
      if (incomingData == "H") {
        lastSyncTime = millis();
        if (!isConnected) { isConnected = true; drawPage(true); }
        continue; 
      }

      int lastIndex = 0;
      String parsed[14];
      for (int i = 0; i < 13; i++) {
        int nextIndex = incomingData.indexOf('|', lastIndex);
        if (nextIndex == -1) break; 
        parsed[i] = incomingData.substring(lastIndex, nextIndex);
        lastIndex = nextIndex + 1;
      }
      parsed[13] = incomingData.substring(lastIndex);
      
      unsigned long epoch = strtoul(parsed[0].c_str(), NULL, 10);
      if (epoch > 1000000000) {
        RTCTime timeToSet(epoch);
        RTC.setTime(timeToSet);
      }
      
      live_tempStr = parsed[1]; live_condStr = parsed[2];
      live_hiStr = parsed[3]; live_loStr = parsed[4];
      live_wspeedStr = parsed[5]; live_wdegStr = parsed[6];
      live_dewStr = parsed[7]; live_humStr = parsed[8]; live_pressStr = parsed[9];
      live_f1Time = parsed[10]; live_f1Temp = parsed[11];
      live_f2Time = parsed[12]; live_f2Temp = parsed[13];
      
      lastSyncTime = millis();
      if (!isConnected) { isConnected = true; drawPage(true); }
    } 
    else if (readingPacket) {
      incomingData += c;
    }
  }

  if (isConnected && (millis() - lastSyncTime > 4000)) {
    isConnected = false;
    live_tempStr = "--"; live_condStr = "WAITING"; 
    live_hiStr = "--"; live_loStr = "--";
  }

  updateRTC();
  updateLiveUI(); // Automatically draws only what changed!
}