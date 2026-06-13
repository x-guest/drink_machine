#ifndef LOGIC_H
#define LOGIC_H

#include "hardware.h"
#include "strings.h"

//=============================================================================
// ПЕРЕЧИСЛЕНИЯ (STATES)
//=============================================================================

enum MenuState {
  MAIN_SCREEN, ROOT_MENU, SUB_MENU_EDIT, SET_MODE,
  SET_BASE_VOLUME, SET_ROULETTE_MENU, SET_TOAST_TIME,
  CAL_PUMP, POURING, TOAST_SCREEN, PUMP_FLUSH, RESET_PAGE, SLEEP_MODE,
  SET_STALE_MENU, SET_STALE_TIME, FAST_DOSING
};

enum WorkMode { MODE_MANUAL, MODE_AUTOMATIC, MODE_ROULETTE };
enum GlassStatus { EMPTY_SPACE, EMPTY_GLASS, POURING_GLASS, FILLED_GLASS, DISABLED_SPACE };

//=============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (extern)
//=============================================================================
extern MenuState state;
extern WorkMode workMode;
extern GlassStatus glassState[NUM_GLASSES];
extern bool rouletteActiveGlasses[NUM_GLASSES];
extern int menuSelector, dosingGlassSelector, toastSubItem, activeToastID, currentGlass;
extern unsigned long glassPlacementTime[NUM_GLASSES];
extern bool glassTimerArmed[NUM_GLASSES];
extern unsigned long glassStaleTimer[NUM_GLASSES], alertStopTimer[NUM_GLASSES];
extern bool isAlerting[NUM_GLASSES], staleTimerArmed[NUM_GLASSES];
extern unsigned long lastToastTime, lastEncoderMoveTime;
extern unsigned long lastButtonDebounceTime, lastReleaseTime;
extern bool lastButtonState, buttonPressProcessed, waitSecondClick;
extern bool toastAudioPending;
extern unsigned long toastScreenTimer, toastAudioTimer;
extern bool isPress, longPressTriggered;
extern unsigned long pressTime;

//=============================================================================
// КОНСТАНТЫ
//=============================================================================
const unsigned long SLEEP_TIMEOUT_MS = SLEEP_TIMEOUT_MINUTES * 60000UL;

//=============================================================================
// ПРОТОТИПЫ ФУНКЦИЙ
//=============================================================================
void printStr(const char* str, uint8_t y, uint8_t scale, bool isProgmem = true);
void printStrFromRAM(const char* str, uint8_t y, uint8_t scale);
void printGlassesRow(int startIdx, uint8_t y, bool isEditing, int editIdx);
void showSaveAlert();
void performReset();
void runSystemFlush();
void runPumpCalibration();
void showToastOnDisplay();
void handleEncoderTurn(bool isRight);
void handleShortClick();
void handleLongPress();
void handleDoubleBlank();
void checkAndPour();
void updateDisplay();

//=============================================================================
// ФУНКЦИИ ДИСПЛЕЯ (ОПТИМИЗИРОВАННЫЕ)
//=============================================================================

// ПЕЧАТЬ СТРОКИ С ЦЕНТРИРОВАНИЕМ
void printStr(const char* str, uint8_t y, uint8_t scale, bool isProgmem) {
  uint8_t len = 0;
  if (isProgmem) {
    for (const char* p = str; pgm_read_byte(p); p++) {
      if ((pgm_read_byte(p) & 0xC0) != 0x80) len++;
    }
  } else {
    for (const char* p = str; *p; p++) {
      if ((*p & 0xC0) != 0x80) len++;
    }
  }
  uint8_t letterW = (scale == 1) ? 6 : ((scale == 2) ? 12 : 18);
  uint8_t x = (128 - (len * letterW)) / 2;
  if (x > 120) x = 0;
  oled.setCursor(x, y);
  oled.setScale(scale);
  if (isProgmem) {
    char buf[24];
    uint8_t i = 0;
    for (const char* p = str; i < 23 && pgm_read_byte(p); p++) {
      buf[i++] = pgm_read_byte(p);
    }
    buf[i] = '\0';
    oled.print(buf);
  } else {
    oled.print(str);
  }
}

void printStrFromRAM(const char* str, uint8_t y, uint8_t scale) {
  printStr(str, y, scale, false);
}

// ПЕЧАТЬ СТРОКИ С РЮМКАМИ (ОПТИМИЗИРОВАННАЯ - БЕЗ SPRINTF)
void printGlassesRow(int startIdx, uint8_t y, bool isEditing, int editIdx) {
  char buffer[32];
  uint8_t pos = 0;
  for (int i = startIdx; i < startIdx + 3; i++) {
    if (isEditing && editIdx == i) buffer[pos++] = '[';
    else buffer[pos++] = ' ';
    if (set.shotVolumeIndividual[i] == 0) {
      buffer[pos++] = 'B'; buffer[pos++] = 'b'; buffer[pos++] = 'I'; buffer[pos++] = 'K';
    } else {
      int val = set.shotVolumeIndividual[i];
      if (val < 100) buffer[pos++] = ' ';
      if (val < 10) buffer[pos++] = ' ';
      // Ручное преобразование числа в строку (без sprintf)
      if (val >= 100) {
        buffer[pos++] = '0' + (val / 100);
        buffer[pos++] = '0' + ((val / 10) % 10);
        buffer[pos++] = '0' + (val % 10);
      } else if (val >= 10) {
        buffer[pos++] = '0' + (val / 10);
        buffer[pos++] = '0' + (val % 10);
      } else {
        buffer[pos++] = '0' + val;
      }
    }
    if (isEditing && editIdx == i) buffer[pos++] = ']';
    else buffer[pos++] = ' ';
    if (i < startIdx + 2) buffer[pos++] = '|';
  }
  buffer[pos] = '\0';
  uint8_t len = strlen(buffer);
  uint8_t x = (128 - (len * 6)) / 2;
  if (x > 120) x = 0;
  oled.setCursor(x, y);
  oled.print(buffer);
}

void showSaveAlert() {
  oled.clear();
  printStr(T_OK, 3, 2, true);
  unsigned long clearTimer = millis();
  while (millis() - clearTimer < 500) {
    waitSecondClick = false;
    isPress = false;
    longPressTriggered = false;
    pressTime = millis();
    lastReleaseTime = millis();
    delay(5);
  }
  oled.clear();
}

void performReset() {
  oled.clear(); oled.setScale(1);
  printStr(T_DONE, 3, 2, true);
  setDefaultSettings();
  workMode = MODE_MANUAL;
  for (int i = 0; i < NUM_GLASSES; i++) {
    glassState[i] = EMPTY_SPACE;
    set.shotVolumeCustomized[i] = false;
  }
  delay(1200);
  homeStepper();
  FastLED.clear(); FastLED.show();
  oled.init();
  state = ROOT_MENU;
  menuSelector = 10;
  updateDisplay();
}

void runSystemFlush() {
  int chosenGlass = -1;
  for (int g = 0; g < NUM_GLASSES; g++) {
    if (readGlassSensor(g) == LOW) { chosenGlass = g; break; }
  }
  if (chosenGlass == -1) {
    oled.clear();
    printStr(M_PLACE_GLASS, 3, 1, true);
    delay(2000);
    state = ROOT_MENU;
    updateDisplay();
    return;
  }
  while (digitalRead(ENCODER_SW) == LOW) { delay(10); }
  delay(50);
  oled.clear();
  printStr(T_FLUSH, 1, 1, true);
  moveStepperTo(set.shotPos[chosenGlass]); delay(600);
  updateLedWithCheck(chosenGlass, CRGB::White, 255);
  FastLED.show();
  digitalWrite(PUMP_PIN, HIGH);
  bool removed = false;
  unsigned long flushStart = millis();
  while (digitalRead(ENCODER_SW) == HIGH) {
    if (readGlassSensor(chosenGlass) == HIGH) { removed = true; break; }
    if (millis() - flushStart > 30000) break;
    delay(30);
  }
  digitalWrite(PUMP_PIN, LOW);
  if (removed) {
    updateLedWithCheck(chosenGlass, CRGB::Black, 0);
    FastLED.show();
    oled.clear(); printStr(M_REMOVED, 2, 1, true); printStr(M_ABORT, 4, 1, true);
    delay(1500);
  } else {
    updateLedWithCheck(chosenGlass, CRGB::Green, 255);
    FastLED.show(); delay(500);
    updateLedWithCheck(chosenGlass, CRGB::Black, 0);
    FastLED.show();
    oled.clear(); printStr(T_DONE, 3, 2, true); delay(1500);
  }
  moveStepperTo(set.homePos);
  while (digitalRead(ENCODER_SW) == LOW) { delay(10); }
  state = ROOT_MENU;
  updateDisplay();
}

void runPumpCalibration() {
  int chosenGlass = -1;
  for (int g = 0; g < NUM_GLASSES; g++) {
    if (readGlassSensor(g) == LOW) { chosenGlass = g; break; }
  }
  if (chosenGlass == -1) {
    oled.clear();
    printStr(M_PLACE_GLASS, 3, 1, true);
    delay(2000);
    state = ROOT_MENU;
    updateDisplay();
    return;
  }
  oled.clear();
  printStr(M_CALIB_TITLE, 1, 1, true);
  moveStepperTo(set.shotPos[chosenGlass]); delay(600);
  unsigned long startTime = millis();
  digitalWrite(PUMP_PIN, HIGH);
  while (digitalRead(ENCODER_SW) == LOW) {
    if (readGlassSensor(chosenGlass) == HIGH) break;
    long t = millis() - startTime;
    oled.setCursor(31, 2); oled.setScale(3); oled.print(t);
    delay(10);
    if (millis() - startTime > 60000) break;
  }
  digitalWrite(PUMP_PIN, LOW);
  set.mlTimeMs = millis() - startTime;
  saveSettings();
  moveStepperTo(set.homePos);
  oled.clear();
  oled.setCursor(35, 3); oled.setScale(2);
  oled.print(set.mlTimeMs); oled.print(F("мс"));
  delay(2000);
  state = ROOT_MENU;
  updateDisplay();
}

void showToastOnDisplay() {
  oled.clear();
  oled.rect(48, 22, 64, 42, OLED_STROKE);
  oled.line(64, 22, 78, 12);
  oled.line(64, 42, 78, 52);
  oled.line(78, 12, 78, 52);
  oled.circle(78, 32, 8, OLED_STROKE);
  oled.circle(78, 32, 16, OLED_STROKE);
  oled.rect(0, 0, 78, 64, OLED_CLEAR);
  oled.rect(48, 22, 64, 42, OLED_FILL);
  oled.line(64, 22, 78, 12);
  oled.line(64, 42, 78, 52);
  oled.line(78, 12, 78, 52);
  if (set.mp3Volume == 0) printStr(T_OFF, 7, 1, true);
}

//=============================================================================
// ОБРАБОТЧИКИ ЭНКОДЕРА (ОПТИМИЗИРОВАННЫЕ)
//=============================================================================

void handleEncoderTurn(bool isRight) {
  if (state == FAST_DOSING) {
    set.shotVolumeIndividual[dosingGlassSelector] += isRight ? VOLUME_STEP_ML : -VOLUME_STEP_ML;
    set.shotVolumeIndividual[dosingGlassSelector] = constrain(set.shotVolumeIndividual[dosingGlassSelector], 0, MAX_POUR_VOLUME);
    set.shotVolumeCustomized[dosingGlassSelector] = true;
    updateDisplay();
    return;
  }

  if (state == SET_TOAST_TIME) {
    if (toastSubItem == 0) {
      dosingGlassSelector += isRight ? 1 : -1;
      if (dosingGlassSelector > 4) dosingGlassSelector = 0;
      if (dosingGlassSelector < 0) dosingGlassSelector = 4;
    } else if (toastSubItem == 1) {
      if (dosingGlassSelector == 0) {
        set.mp3Volume += isRight ? 1 : -1;
        set.mp3Volume = constrain(set.mp3Volume, 0, 30);
        setMP3Volume(set.mp3Volume);
      } else if (dosingGlassSelector == 1) {
        set.toastPauseS += isRight ? 1 : -1;
        set.toastPauseS = constrain(set.toastPauseS, 0, 10);
      } else if (dosingGlassSelector == 2) {
        set.toastDelayS += isRight ? 1 : -1;
        set.toastDelayS = constrain(set.toastDelayS, 1, 30);
      } else if (dosingGlassSelector == 3) {
        set.startDelayS += isRight ? 1 : -1;
        set.startDelayS = constrain(set.startDelayS, 0, 10);
      }
    }
    updateDisplay();
    return;
  }

  if (state == SET_MODE) {
    if (toastSubItem == 0) {
      menuSelector += isRight ? 1 : -1;
      if (menuSelector > 3) menuSelector = 0;
      if (menuSelector < 0) menuSelector = 3;
    } else dosingGlassSelector = (dosingGlassSelector == 1) ? 0 : 1;
    updateDisplay();
    return;
  }

  if (state == SET_BASE_VOLUME) {
    int delta = isRight ? VOLUME_STEP_ML : -VOLUME_STEP_ML;
    int targetVol = constrain(set.shotVolume + delta, MIN_POUR_VOLUME, MAX_POUR_VOLUME);
    set.shotVolume = targetVol;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (!set.shotVolumeCustomized[i]) set.shotVolumeIndividual[i] = targetVol;
    }
    updateDisplay();
    return;
  }

  if (state == SET_STALE_MENU) {
    dosingGlassSelector += isRight ? 1 : -1;
    if (dosingGlassSelector > 2) dosingGlassSelector = 0;
    if (dosingGlassSelector < 0) dosingGlassSelector = 2;
    updateDisplay();
    return;
  }

  if (state == SET_STALE_TIME) {
    if (dosingGlassSelector == 0) {
      set.staleTimeS += isRight ? STALE_TIME_STEP_S : -STALE_TIME_STEP_S;
      set.staleTimeS = constrain(set.staleTimeS, 0, MAX_STALE_TIME_S);
    } else if (dosingGlassSelector == 1) {
      set.staleAlertS += isRight ? STALE_ALERT_STEP_S : -STALE_ALERT_STEP_S;
      set.staleAlertS = constrain(set.staleAlertS, 1, MAX_STALE_ALERT_S);
    }
    updateDisplay();
    return;
  }

  if (state == SET_ROULETTE_MENU) {
    if (toastSubItem == 0) {
      dosingGlassSelector += isRight ? 1 : -1;
      if (dosingGlassSelector > 2) dosingGlassSelector = 0;
      if (dosingGlassSelector < 0) dosingGlassSelector = 2;
    } else {
      if (dosingGlassSelector == 0) {
        set.rouletteSpinS += isRight ? 1 : -1;
        set.rouletteSpinS = constrain(set.rouletteSpinS, MIN_ROULETTE_SPIN_S, MAX_ROULETTE_SPIN_S);
      } else if (dosingGlassSelector == 1) {
        set.rouletteSpeed += isRight ? 1 : -1;
        set.rouletteSpeed = constrain(set.rouletteSpeed, 1, 10);
      }
    }
    updateDisplay();
    return;
  }

  if (state == SUB_MENU_EDIT && menuSelector == 1) {
    if (toastSubItem == 0) {
      currentGlass += isRight ? 1 : -1;
      if (currentGlass > 7) currentGlass = 0;
      if (currentGlass < 0) currentGlass = 7;
    } else {
      int stepChange = isRight ? CALIBRATION_STEP_STEPS : -CALIBRATION_STEP_STEPS;
      if (currentGlass == 0) {
        set.homePos += stepChange;
        set.homePos = constrain(set.homePos, -200, 200);
        moveStepperTo(set.homePos);
      } else {
        int idx = currentGlass - 1;
        set.shotPos[idx] += stepChange;
        set.shotPos[idx] = constrain(set.shotPos[idx], 0, stepsPerRevolution);
        moveStepperTo(set.shotPos[idx]);
      }
    }
    updateDisplay();
    return;
  }

  if (state == SUB_MENU_EDIT && menuSelector != 1) {
    if (menuSelector == 7) {
      set.ledBrightness += isRight ? 5 : -5;
      set.ledBrightness = constrain(set.ledBrightness, 10, 255);
      FastLED.setBrightness(set.ledBrightness);
      FastLED.show();
    } else if (menuSelector == 8) {
      set.sensorDelayMs += isRight ? 100 : -100;
      set.sensorDelayMs = constrain(set.sensorDelayMs, 200, 3000);
    }
    updateDisplay();
    return;
  }

  if (state == RESET_PAGE || state == PUMP_FLUSH) {
    dosingGlassSelector = (dosingGlassSelector == 1) ? 0 : 1;
    updateDisplay();
    return;
  }

  if (state == MAIN_SCREEN) {
    int delta = isRight ? VOLUME_STEP_ML : -VOLUME_STEP_ML;
    int baseVol = MIN_POUR_VOLUME;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (!set.shotVolumeCustomized[i] && readGlassSensor(i) == HIGH) {
        baseVol = set.shotVolumeIndividual[i];
        break;
      }
    }
    int targetVol = constrain(baseVol + delta, MIN_POUR_VOLUME, MAX_POUR_VOLUME);
    set.shotVolume = targetVol;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (readGlassSensor(i) == LOW && glassState[i] == FILLED_GLASS) continue;
      if (!set.shotVolumeCustomized[i]) {
        set.shotVolumeIndividual[i] = targetVol;
      } else if (set.shotVolumeIndividual[i] == targetVol) {
        set.shotVolumeCustomized[i] = false;
      }
    }
  } else if (state == ROOT_MENU) {
    menuSelector += isRight ? 1 : -1;
    if (menuSelector >= TOTAL_MENU_ITEMS) menuSelector = 0;
    if (menuSelector < 0) menuSelector = TOTAL_MENU_ITEMS - 1;
  }
  updateDisplay();
}

void handleShortClick() {
  if (state == FAST_DOSING) {
    dosingGlassSelector++;
    if (dosingGlassSelector >= NUM_GLASSES) {
      saveSettings();
      showSaveAlert();
      state = MAIN_SCREEN;
    }
    updateDisplay();
    return;
  }

  if (state == MAIN_SCREEN) {
    if (workMode == MODE_MANUAL || workMode == MODE_ROULETTE) checkAndPour();
  } else if (state == ROOT_MENU) {
    if (menuSelector == 0) { state = SET_MODE; menuSelector = 0; toastSubItem = 0; }
    else if (menuSelector == 1) { state = SUB_MENU_EDIT; currentGlass = 0; toastSubItem = 0; moveStepperTo(set.homePos); }
    else if (menuSelector == 2) { state = SET_BASE_VOLUME; }
    else if (menuSelector == 3) { state = SET_ROULETTE_MENU; dosingGlassSelector = 0; toastSubItem = 0; }
    else if (menuSelector == 4) { state = SET_TOAST_TIME; dosingGlassSelector = 0; toastSubItem = 0; }
    else if (menuSelector == 5) { state = SET_STALE_MENU; dosingGlassSelector = 0; }
    else if (menuSelector == 6) { state = CAL_PUMP; }
    else if (menuSelector == 7) { state = SUB_MENU_EDIT; toastSubItem = 0; }
    else if (menuSelector == 8) { state = SUB_MENU_EDIT; toastSubItem = 0; }
    else if (menuSelector == 9) { state = PUMP_FLUSH; dosingGlassSelector = 0; }
    else if (menuSelector == 10) { state = RESET_PAGE; dosingGlassSelector = 0; }
  } else if (state == SET_BASE_VOLUME) {
    saveSettings(); showSaveAlert(); state = ROOT_MENU; menuSelector = 2;
  } else if (state == SET_STALE_MENU) {
    if (dosingGlassSelector == 2) { state = ROOT_MENU; menuSelector = 5; }
    else { state = SET_STALE_TIME; }
  } else if (state == SET_STALE_TIME) {
    saveSettings(); showSaveAlert(); state = SET_STALE_MENU;
  } else if (state == SUB_MENU_EDIT) {
    if (menuSelector == 1) {
      if (toastSubItem == 0) {
        if (currentGlass == 7) { state = ROOT_MENU; menuSelector = 1; }
        else toastSubItem = 1;
      } else { saveSettings(); showSaveAlert(); toastSubItem = 0; }
    } else { saveSettings(); showSaveAlert(); state = ROOT_MENU; }
  } else if (state == SET_MODE) {
    if (toastSubItem == 0) {
      if (menuSelector == 3) { state = ROOT_MENU; menuSelector = 0; }
      else { toastSubItem = 1; dosingGlassSelector = 0; }
    } else if (toastSubItem == 1) {
      if (dosingGlassSelector == 0) {
        if (menuSelector == 0) { workMode = MODE_MANUAL; set.savedMode = 0; }
        else if (menuSelector == 1) { workMode = MODE_AUTOMATIC; set.savedMode = 1; }
        else if (menuSelector == 2) { workMode = MODE_ROULETTE; set.savedMode = 2; }
        state = MAIN_SCREEN; saveSettings(); showSaveAlert();
        menuSelector = 0; toastSubItem = 0;
      } else toastSubItem = 0;
    }
  } else if (state == SET_ROULETTE_MENU) {
    if (toastSubItem == 0) {
      if (dosingGlassSelector == 2) { state = ROOT_MENU; menuSelector = 3; toastSubItem = 0; }
      else toastSubItem = 1;
    } else { saveSettings(); showSaveAlert(); toastSubItem = 0; }
  } else if (state == SET_TOAST_TIME) {
    if (toastSubItem == 0) {
      if (dosingGlassSelector == 4) { state = ROOT_MENU; menuSelector = 4; }
      else toastSubItem = 1;
    } else { saveSettings(); showSaveAlert(); toastSubItem = 0; }
  } else if (state == CAL_PUMP) { saveSettings(); showSaveAlert(); state = ROOT_MENU; menuSelector = 6; }
  else if (state == PUMP_FLUSH) { if (dosingGlassSelector == 1) runSystemFlush(); else { state = ROOT_MENU; menuSelector = 9; } }
  else if (state == RESET_PAGE) { if (dosingGlassSelector == 1) performReset(); else { state = ROOT_MENU; menuSelector = 10; } }
  updateDisplay();
}

void handleLongPress() {
  if (state == FAST_DOSING) {
    saveSettings();
    showSaveAlert();
    state = MAIN_SCREEN;
    updateDisplay();
    return;
  }

  FastLED.setBrightness(set.ledBrightness);
  FastLED.show();
  setMP3Volume(set.mp3Volume);

  if (state == MAIN_SCREEN || state == ROOT_MENU) {
    state = (state == MAIN_SCREEN) ? ROOT_MENU : MAIN_SCREEN;
    moveStepperTo(set.homePos);
  } else {
    if (toastSubItem > 0) {
      toastSubItem = 0;
      if (state == SUB_MENU_EDIT && menuSelector == 1) {
        if (currentGlass == 0) moveStepperTo(set.homePos);
        else moveStepperTo(set.shotPos[currentGlass - 1]);
      }
    } else {
      if (state == SET_STALE_TIME) { state = SET_STALE_MENU; }
      else if (state == SET_STALE_MENU) { state = ROOT_MENU; menuSelector = 5; }
      else { state = ROOT_MENU; }
    }
    moveStepperTo(set.homePos);
  }
  updateDisplay();
}

void handleDoubleBlank() {
  if (state == MAIN_SCREEN) {
    state = FAST_DOSING;
    dosingGlassSelector = 0;
  }
  else if (state == FAST_DOSING) {
    loadSettings();
    state = MAIN_SCREEN;
  }
  else if (state != ROOT_MENU && state != POURING && state != TOAST_SCREEN) {
    state = MAIN_SCREEN;
    moveStepperTo(set.homePos);
  }
  updateDisplay();
}

//=============================================================================
// ОСНОВНАЯ ЛОГИКА НАЛИВА
//=============================================================================

void checkAndPour() {
  MenuState prevState = state;
  state = POURING;
  bool glassFound = false;
  unsigned long localPlacementTime[NUM_GLASSES] = {0};
  bool localTimerArmed[NUM_GLASSES] = {false};
  for (int g = 0; g < NUM_GLASSES; g++) { rouletteActiveGlasses[g] = false; }

  if (workMode == MODE_ROULETTE) {
    int avail[NUM_GLASSES], availCnt = 0;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (readGlassSensor(i) == LOW && set.shotVolumeIndividual[i] > 0 && glassState[i] == EMPTY_GLASS) {
        avail[availCnt++] = i;
      }
    }
    if (availCnt == 0) {
      oled.clear(); printStr(M_NO_GLASS, 3, 1, true); delay(1200);
      state = prevState; updateDisplay(); return;
    }
    int target = avail[random(0, availCnt)];
    rouletteActiveGlasses[target] = true;
    oled.clear(); printStr(M_ROULETTE_SPIN, 3, 1, true);
    if (set.mp3Volume > 0) playTrack(SYSTEM_SOUND_FOLDER, ROULETTE_TRACK_ID);
    int animSpeed = constrain(65 - (set.rouletteSpeed * 5), 5, 80);
    unsigned long start = millis();
    unsigned long duration = (unsigned long)set.rouletteSpinS * 1000UL;
    int step = 0;
    while (millis() - start < duration) {
      FastLED.clear(); leds[step % NUM_GLASSES] = CRGB::Red; FastLED.show();
      delay(animSpeed);
      step++;
    }
    FastLED.clear(); FastLED.show();
  }

  unsigned long pourLoopStart = millis();
  while (true) {
    if (millis() - pourLoopStart > 120000) { state = MAIN_SCREEN; updateDisplay(); return; }

    if (workMode != MODE_ROULETTE) {
      for (int i = 0; i < NUM_GLASSES; i++) {
        if (readGlassSensor(i) == HIGH) {
          glassState[i] = EMPTY_SPACE;
          localTimerArmed[i] = false; rouletteActiveGlasses[i] = false;
          updateLedWithCheck(i, COLOR_STANDBY, BRIGHT_STANDBY); FastLED.show();
          continue;
        }
        if (readGlassSensor(i) == LOW && glassState[i] != FILLED_GLASS && glassState[i] != POURING_GLASS) {
          if (set.shotVolumeIndividual[i] == 0) {
            glassState[i] = DISABLED_SPACE;
            updateLedWithCheck(i, COLOR_DISABLED, BRIGHT_DISABLED); FastLED.show();
            localTimerArmed[i] = false; rouletteActiveGlasses[i] = false;
            continue;
          }
          if (glassState[i] == EMPTY_SPACE) { glassState[i] = EMPTY_GLASS; }
          if (!localTimerArmed[i] && !rouletteActiveGlasses[i]) {
            localPlacementTime[i] = millis();
            localTimerArmed[i] = true;
            updateLedWithCheck(i, COLOR_ACTIVE, BRIGHT_ACTIVE); FastLED.show();
          }
          if (localTimerArmed[i] && (millis() - localPlacementTime[i] >= (unsigned long)set.sensorDelayMs)) {
            localTimerArmed[i] = false;
            rouletteActiveGlasses[i] = true;
          }
        }
      }
    }

    int targetGlass = -1;
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (rouletteActiveGlasses[i] && glassState[i] == EMPTY_GLASS) { targetGlass = i; break; }
    }
    if (targetGlass == -1) {
      bool anyPending = false;
      if (workMode != MODE_ROULETTE) {
        for (int i = 0; i < NUM_GLASSES; i++) { if (localTimerArmed[i]) { anyPending = true; break; } }
      }
      if (anyPending) { delay(10); continue; }
      break;
    }

    glassFound = true;
    int i = targetGlass;
    int vol = set.shotVolumeIndividual[i];
    oled.clear(); oled.setScale(1);
    oled.setCursor(19, 0);
    oled.print(F("НАЛИВАЮ В РЮМКУ ")); oled.print(i + 1);

    glassState[i] = POURING_GLASS;
    moveStepperTo(set.shotPos[i]); delay(400);
    digitalWrite(PUMP_PIN, HIGH);

    unsigned long totalTime = ((unsigned long)vol * (unsigned long)set.mlTimeMs) / 50UL;
    unsigned long startTime = millis();
    bool removed = false;
    int lastMl = -1;

    while (millis() - startTime < totalTime) {
      if (readGlassSensor(i) == HIGH) { removed = true; break; }
      unsigned long elapsed = millis() - startTime;
      int percent = (elapsed * 100UL) / totalTime;
      int curMl = ((percent * vol) + 50) / 100;
      if (curMl > vol) curMl = vol;
      if (curMl != lastMl) {
        char buf[12];
        // Оптимизированный itoa без sprintf
        int n = curMl;
        char* ptr = buf;
        if (n >= 100) { *ptr++ = '0' + (n / 100); n %= 100; }
        if (curMl >= 10 || curMl >= 100) { *ptr++ = '0' + (n / 10); n %= 10; }
        *ptr++ = '0' + n;
        *ptr++ = ' '; *ptr++ = 'м'; *ptr++ = 'л'; *ptr = '\0';
        uint8_t len = strlen(buf);
        uint8_t x = (128 - (len * 12)) / 2;
        oled.setCursor(x, 4); oled.setScale(2); oled.print(buf);
        lastMl = curMl;
      }
      leds[i] = blend(CRGB::Blue, CRGB::Green, (percent * 255) / 100);
      FastLED.show();

      if (workMode != MODE_ROULETTE) {
        for (int j = 0; j < NUM_GLASSES; j++) {
          if (j == i) continue;
          if (readGlassSensor(j) == HIGH) {
            glassState[j] = EMPTY_SPACE;
            if (localTimerArmed[j]) {
              localTimerArmed[j] = false;
              updateLedWithCheck(j, COLOR_STANDBY, BRIGHT_STANDBY); FastLED.show();
            }
            continue;
          }
          if (readGlassSensor(j) == LOW && glassState[j] != FILLED_GLASS) {
            if (set.shotVolumeIndividual[j] == 0) {
              glassState[j] = DISABLED_SPACE;
              updateLedWithCheck(j, COLOR_DISABLED, BRIGHT_DISABLED); FastLED.show();
              continue;
            }
            if (glassState[j] == EMPTY_SPACE) { glassState[j] = EMPTY_GLASS; }
            if (!localTimerArmed[j] && !rouletteActiveGlasses[j]) {
              localPlacementTime[j] = millis();
              localTimerArmed[j] = true;
              updateLedWithCheck(j, COLOR_ACTIVE, BRIGHT_ACTIVE); FastLED.show();
            }
            if (localTimerArmed[j] && (millis() - localPlacementTime[j] >= (unsigned long)set.sensorDelayMs)) {
              localTimerArmed[j] = false;
              rouletteActiveGlasses[j] = true;
            }
          }
        }
      }
      delay(10);
    }
    digitalWrite(PUMP_PIN, LOW);

    if (removed) {
      if (workMode == MODE_ROULETTE) { stopMP3(); delay(50); }
      glassState[i] = EMPTY_SPACE;
      updateLedWithCheck(i, COLOR_STANDBY, BRIGHT_STANDBY); FastLED.show();
      oled.clear(); printStr(M_REMOVED, 2, 1, true); printStr(M_ABORT, 4, 1, true);
      delay(1200);
      rouletteActiveGlasses[i] = false;
      break;
    } else {
      if (workMode == MODE_ROULETTE) { stopMP3(); delay(50); }
      glassState[i] = FILLED_GLASS;
      updateLedWithCheck(i, COLOR_POURED, BRIGHT_POURED); FastLED.show();
      delay(400);
    }
  }
  delay(100);
  if (!glassFound && workMode == MODE_MANUAL) {
    oled.clear(); printStr(M_NO_NEW, 3, 1, true); delay(1200);
  }
  state = MAIN_SCREEN;
  updateDisplay();
}

//=============================================================================
// ОБНОВЛЕНИЕ ДИСПЛЕЯ (ВСЕ СОСТОЯНИЯ) - ОПТИМИЗИРОВАННОЕ
//=============================================================================

void updateDisplay() {
  oled.clear();
  if (state == SLEEP_MODE) return;

  if (state == MAIN_SCREEN || state == FAST_DOSING) {
    if (state == FAST_DOSING) { printStrFromRAM("ОБЪЕМ", 0, 1); }
    else {
      if (workMode == MODE_MANUAL) printStr(T_MANUAL, 0, 1, true);
      else if (workMode == MODE_AUTOMATIC) printStr(T_AUTO, 0, 1, true);
      else printStr(T_ROULETTE, 0, 1, true);
    }
    if (set.mp3Volume == 0) {
      oled.invertText(true);
      oled.setCursor(110, 0);
      oled.print(F(" M "));
      oled.invertText(false);
    }
    oled.setScale(1);
    printGlassesRow(0, 2, (state == FAST_DOSING), dosingGlassSelector);
    printGlassesRow(3, 4, (state == FAST_DOSING), dosingGlassSelector);
    if (state == FAST_DOSING) { printStrFromRAM("Круть/Клик", 7, 1); }
    else {
      if (workMode == MODE_MANUAL || workMode == MODE_ROULETTE) { printStr(M_CLICK_POUR, 7, 1, true); }
      else { printStr(M_WAIT_GLASS, 7, 1, true); }
    }
  }
  else if (state == ROOT_MENU) {
    printStr(T_MENU, 0, 1, true);
    char* menuPtr = (char*)pgm_read_word(&MENU[menuSelector]);
    printStr(menuPtr, 3, 2, true);
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SUB_MENU_EDIT && menuSelector == 1) {
    if (toastSubItem == 0) {
      printStrFromRAM("КАЛИБРОВКА", 1, 1);
      if (currentGlass == 0) { printStr(T_HOME_BASE, 3, 2, true); }
      else if (currentGlass <= 6) {
        oled.setCursor(45, 3); oled.setScale(2);
        oled.print(F("Р")); oled.print(currentGlass);
      } else { printStr(T_BACK, 3, 2, true); }
      printStr(T_ROT_HINT, 7, 1, true);
    } else {
      printStrFromRAM("КАЛИБРОВКА:", 1, 1);
      oled.setCursor(30, 3); oled.setScale(3);
      if (currentGlass == 0) { oled.print(set.homePos); }
      else { oled.print(set.shotPos[currentGlass - 1]); }
      oled.setScale(1); oled.print(F(" шаг"));
      printStr(T_ROT_HINT, 7, 1, true);
    }
  }
  else if (state == SET_BASE_VOLUME) {
    printStrFromRAM("ОБЪЕМ ПО УМОЛЧАНИЮ", 1, 1);
    oled.setCursor(35, 3); oled.setScale(3);
    if (set.shotVolume == 0) { oled.print(F("ВЫКЛ")); }
    else { oled.print(set.shotVolume); }
    oled.setScale(1);
    if (set.shotVolume > 0) { oled.print(F(" мл")); }
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SET_STALE_MENU) {
    printStrFromRAM("ПИНАТЕЛЬ", 1, 1);
    if (dosingGlassSelector == 0) { printStrFromRAM("ТАЙМАУТ", 3, 2); }
    else if (dosingGlassSelector == 1) { printStrFromRAM("ПИНОК", 3, 2); }
    else { printStr(T_BACK, 3, 2, true); }
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SET_STALE_TIME) {
    if (dosingGlassSelector == 0) {
      printStrFromRAM("ВРЕМЯ ОЖИДАНИЯ", 1, 1);
      oled.setCursor(35, 3); oled.setScale(3);
      if (set.staleTimeS == 0) { oled.print(F("ВЫКЛ")); }
      else { oled.print(set.staleTimeS); }
      oled.setScale(1);
      if (set.staleTimeS > 0) { oled.print(F(" сек")); }
    } else if (dosingGlassSelector == 1) {
      printStrFromRAM("ДЛИТЕЛЬНОСТЬ ПИНКА", 1, 1);
      oled.setCursor(35, 3); oled.setScale(3);
      oled.print(set.staleAlertS);
      oled.setScale(1); oled.print(F(" сек"));
    }
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SUB_MENU_EDIT && menuSelector == 7) {
    printStrFromRAM("ЯРКОСТЬ", 1, 1);
    oled.setCursor(40, 3); oled.setScale(3);
    oled.print(set.ledBrightness);
    oled.setScale(1); oled.print(F(" кд"));
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SUB_MENU_EDIT && menuSelector == 8) {
    printStrFromRAM("ЗАДЕРЖКА ДАТЧИКОВ", 1, 1);
    oled.setCursor(35, 4); oled.setScale(2);
    oled.print(set.sensorDelayMs);
    oled.setScale(1); oled.print(F(" мс"));
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == SET_MODE) {
    if (toastSubItem == 0) {
      printStrFromRAM("РЕЖИМ", 1, 1);
      if (menuSelector == 0) { printStr(T_MANUAL, 3, 2, true); }
      else if (menuSelector == 1) { printStr(T_AUTO, 3, 2, true); }
      else if (menuSelector == 2) { printStr(T_ROULETTE, 3, 2, true); }
      else { printStr(T_BACK, 3, 2, true); }
      printStr(T_ROT_HINT, 7, 1, true);
    } else {
      printStrFromRAM("АКТИВИРОВАТЬ?", 1, 1);
      if (dosingGlassSelector == 0) { printStr(T_ON, 3, 2, true); }
      else { printStr(T_OFF, 3, 2, true); }
      printStr(T_ROT_HINT, 7, 1, true);
    }
  }
  else if (state == SET_ROULETTE_MENU) {
    if (toastSubItem == 0) {
      printStrFromRAM("РУЛЕТКА", 1, 1);
      if (dosingGlassSelector == 0) { printStrFromRAM("ВРЕМЯ", 3, 2); }
      else if (dosingGlassSelector == 1) { printStrFromRAM("СКОРОСТЬ", 3, 2); }
      else { printStr(T_BACK, 3, 2, true); }
      printStr(T_ROT_HINT, 7, 1, true);
    } else {
      if (dosingGlassSelector == 0) {
        printStrFromRAM("ВРЕМЯ ВРАЩЕНИЯ", 1, 1);
        oled.setScale(3); oled.setCursor(35, 3);
        oled.print(set.rouletteSpinS);
        oled.setScale(1); oled.print(F(" сек"));
      } else if (dosingGlassSelector == 1) {
        printStrFromRAM("СКОРОСТЬ ВРАЩЕНИЯ", 1, 1);
        oled.setScale(3); oled.setCursor(55, 3);
        oled.print(set.rouletteSpeed);
        oled.setScale(1); oled.print(F(" об"));
      }
      printStr(T_ROT_HINT, 7, 1, true);
    }
  }
  else if (state == SET_TOAST_TIME) {
    if (toastSubItem == 0) {
      printStrFromRAM("ТАМАДА", 1, 1);
      if (dosingGlassSelector == 0) { printStr(M4, 3, 2, true); }
      else if (dosingGlassSelector == 1) { printStrFromRAM("ПАУЗА", 3, 2); }
      else if (dosingGlassSelector == 2) { printStrFromRAM("ТАЙМАУТ", 3, 2); }
      else if (dosingGlassSelector == 3) { printStrFromRAM("ОТСРОЧКА", 3, 2); }
      else { printStr(T_BACK, 3, 2, true); }
      printStr(T_ROT_HINT, 7, 1, true);
    } else {
      printStrFromRAM("ЗНАЧЕНИЕ:", 1, 1);
      oled.setScale(3); oled.setCursor(45, 3);
      if (dosingGlassSelector == 0) {
        if (set.mp3Volume == 0) { oled.print(F("ВЫКЛ")); }
        else { oled.print(set.mp3Volume); }
      } else if (dosingGlassSelector == 1) {
        oled.print(set.toastPauseS); oled.setScale(1); oled.print(F(" сек"));
      } else if (dosingGlassSelector == 2) {
        oled.print(set.toastDelayS); oled.setScale(1); oled.print(F(" сек"));
      } else if (dosingGlassSelector == 3) {
        oled.print(set.startDelayS); oled.setScale(1); oled.print(F(" сек"));
      }
      printStr(T_ROT_HINT, 7, 1, true);
    }
  }
  else if (state == RESET_PAGE || state == PUMP_FLUSH) {
    if (state == RESET_PAGE) { printStr(M_RESET_CONF, 1, 1, true); }
    else { printStr(M_FLUSH_CONF, 1, 1, true); }
    if (dosingGlassSelector == 1) {
      if (state == RESET_PAGE) { printStr(T_RESET, 3, 2, true); }
      else { printStrFromRAM("ПРОЛИВ", 3, 2); }
    } else { printStr(T_BACK, 3, 2, true); }
    printStr(T_ROT_HINT, 7, 1, true);
  }
  else if (state == CAL_PUMP) {
    printStr(M_CALIB_TITLE, 1, 1, true);
    oled.setCursor(40, 4); oled.setScale(2);
    oled.print(set.mlTimeMs);
    oled.setScale(1); oled.print(F("мс"));
    printStrFromRAM("Удержать Клик", 7, 1);
  }
}

#endif