/*
 * Task Timer — ST7920 Ender 3 Display (EXP3 SPI)
 * 
 * Wiring (EXP3 → Arduino Uno):
 *   Brown  (5V)            → 5V
 *   Black  (GND)           → GND
 *   Grey   (Chip Select)   → D10
 *   Blue   (Data/MOSI)     → D11
 *   Green  (Clock)         → D13
 *   Yellow (Knob Rot 1)    → D2
 *   Orange (Knob Rot 2)    → D3
 *   Purple (Knob Button)   → D4
 *   Red    (Beeper)        → D5
 * 
 * Libraries needed:
 *   - U8g2 (install via Arduino Library Manager)
 */

#include <U8g2lib.h>

// --- Pin Definitions ---
#define CLK_PIN     13
#define MOSI_PIN    11
#define CS_PIN      10
#define ENC_A       2
#define ENC_B       3
#define ENC_BTN     4
#define BEEPER_PIN  5

// --- Display ---
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, CLK_PIN, MOSI_PIN, CS_PIN);

// --- Timer States ---
enum State { SETTING, RUNNING, PAUSED, DONE };
State state = SETTING;

// --- Timer Variables ---
int setMinutes = 45;
int setSeconds = 0;
long remainingMs = 0;
long lastTick = 0;

// --- Encoder Variables ---
volatile int encDelta = 0;
int lastEncA = HIGH;

// --- Button Variables ---
bool lastBtnState = HIGH;
unsigned long lastBtnTime = 0;
#define DEBOUNCE_MS 50

// --- Beeper ---
bool beeped = false;

// =====================
//   ENCODER INTERRUPT
// =====================
void encoderISR() {
  int a = digitalRead(ENC_A);
  int b = digitalRead(ENC_B);
  if (a != lastEncA) {
    if (b != a) encDelta++;
    else         encDelta--;
    lastEncA = a;
  }
}

// =====================
//   BEEPER HELPERS
// =====================
void beep(int freq, int duration) {
  tone(BEEPER_PIN, freq, duration);
}

void beepDone() {
  // Three short beeps
  for (int i = 0; i < 3; i++) {
    tone(BEEPER_PIN, 1000, 150);
    delay(300);
  }
}

void beepClick() {
  tone(BEEPER_PIN, 1800, 30);
}

// =====================
//   DRAW FUNCTIONS
// =====================
void drawSetting() {
  u8g2.clearBuffer();

  // Title
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 12, "SET TIMER");
  u8g2.drawHLine(0, 15, 128);

  // Big time display
  char buf[10];
  sprintf(buf, "%02d:%02d", setMinutes, setSeconds);
  u8g2.setFont(u8g2_font_logisoso28_tf);
  u8g2.drawStr(24, 52, buf);

  // Instructions
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(4, 63, "Turn=adjust  Click=start");

  u8g2.sendBuffer();
}

void drawRunning(long ms, bool paused) {
  u8g2.clearBuffer();

  int totalSecs = ms / 1000;
  int mins = totalSecs / 60;
  int secs = totalSecs % 60;

  // Title
  u8g2.setFont(u8g2_font_6x10_tf);
  if (paused) {
    u8g2.drawStr(38, 12, "PAUSED");
  } else {
    u8g2.drawStr(34, 12, "RUNNING");
  }
  u8g2.drawHLine(0, 15, 128);

  // Big time
  char buf[10];
  sprintf(buf, "%02d:%02d", mins, secs);
  u8g2.setFont(u8g2_font_logisoso28_tf);
  u8g2.drawStr(24, 52, buf);

  // Progress bar
  int totalSet = (setMinutes * 60 + setSeconds) * 1000;
  int barWidth = 0;
  if (totalSet > 0) {
    barWidth = map(ms, 0, totalSet, 0, 120);
    barWidth = constrain(barWidth, 0, 120);
  }
  u8g2.drawFrame(4, 55, 120, 8);
  u8g2.drawBox(4, 55, barWidth, 8);

  // Instructions
  u8g2.setFont(u8g2_font_5x7_tf);
  if (paused) {
    u8g2.drawStr(10, 63, "Click=resume  Hold=reset");
  }

  u8g2.sendBuffer();
}

void drawDone() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(34, 12, "DONE!");
  u8g2.drawHLine(0, 15, 128);

  u8g2.setFont(u8g2_font_logisoso28_tf);
  u8g2.drawStr(30, 50, "00:00");

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(16, 63, "Click to set new timer");

  u8g2.sendBuffer();
}

// =====================
//   BUTTON READ
// =====================
bool buttonPressed() {
  bool reading = digitalRead(ENC_BTN);
  if (reading == LOW && lastBtnState == HIGH) {
    if (millis() - lastBtnTime > DEBOUNCE_MS) {
      lastBtnTime = millis();
      lastBtnState = reading;
      return true;
    }
  }
  lastBtnState = reading;
  return false;
}

// =====================
//   SETUP
// =====================
void setup() {
  pinMode(ENC_A,    INPUT_PULLUP);
  pinMode(ENC_B,    INPUT_PULLUP);
  pinMode(ENC_BTN,  INPUT_PULLUP);
  pinMode(BEEPER_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);

  u8g2.begin();
  u8g2.setContrast(200);

  beep(1200, 100);  // startup beep
  drawSetting();
}

// =====================
//   LOOP
// =====================
void loop() {
  bool clicked = buttonPressed();

  // --- SETTING STATE ---
  if (state == SETTING) {
    if (encDelta != 0) {
      int totalSecs = setMinutes * 60 + setSeconds;
      totalSecs += encDelta * 300;  // 5 minute increments
      totalSecs = constrain(totalSecs, 300, 5999); // 5min min, ~99min max
      setMinutes = totalSecs / 60;
      setSeconds = totalSecs % 60;
      encDelta = 0;
      drawSetting();
    }
    if (clicked) {
      beepClick();
      remainingMs = (long)(setMinutes * 60 + setSeconds) * 1000L;
      lastTick = millis();
      beeped = false;
      state = RUNNING;
      drawRunning(remainingMs, false);
    }
  }

  // --- RUNNING STATE ---
  else if (state == RUNNING) {
    long now = millis();
    long elapsed = now - lastTick;
    lastTick = now;
    remainingMs -= elapsed;

    if (remainingMs <= 0) {
      remainingMs = 0;
      state = DONE;
      if (!beeped) {
        beepDone();
        beeped = true;
      }
      drawDone();
      return;
    }

    if (clicked) {
      beepClick();
      state = PAUSED;
    }

    drawRunning(remainingMs, false);
  }

  // --- PAUSED STATE ---
  else if (state == PAUSED) {
    // Check for long press to reset (hold ~1 second)
    if (digitalRead(ENC_BTN) == LOW) {
      unsigned long holdStart = millis();
      while (digitalRead(ENC_BTN) == LOW) {
        if (millis() - holdStart > 1000) {
          // Reset to setting
          beepClick();
          state = SETTING;
          encDelta = 0;
          drawSetting();
          return;
        }
      }
      // Short press = resume
      beepClick();
      lastTick = millis();
      state = RUNNING;
      return;
    }
    drawRunning(remainingMs, true);
  }

  // --- DONE STATE ---
  else if (state == DONE) {
    if (clicked) {
      beepClick();
      state = SETTING;
      encDelta = 0;
      drawSetting();
    }
  }

  encDelta = 0;
  delay(50);
}
