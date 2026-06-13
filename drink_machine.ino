#pragma GCC optimize("Os")

//=============================================================================
// НАЛИВАТОР - АВТОМАТИЧЕСКАЯ РАЗЛИВНАЯ СТАНЦИЯ
//=============================================================================
// Версия: 2.0
// Описание: Умная система налива напитков с 6 рюмками
//=============================================================================

#include "config.h"
#include "hardware.h"
#include "logic.h"

//=============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (определения)
//=============================================================================

Settings set;
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;
CRGB leds[NUM_GLASSES];
SoftwareSerial mp3Serial(MP3_TX_PIN, MP3_RX_PIN);

const int stepperPins[] = { STEPPER_IN1, STEPPER_IN2, STEPPER_IN3, STEPPER_IN4 };
int _stepIdx = 0;
long currentStepperPos = 0;
int currentGlass = 0;

bool rouletteActiveGlasses[NUM_GLASSES] = {false};

int lastClk = HIGH;
unsigned long pressTime = 0;
bool isPress = false, longPressTriggered = false;
int menuSelector = 0, dosingGlassSelector = 0, toastSubItem = 0, activeToastID = 0;
unsigned long glassPlacementTime[NUM_GLASSES] = {0};
bool glassTimerArmed[NUM_GLASSES] = {false};
unsigned long lastActivityTime = 0;

unsigned long glassStaleTimer[NUM_GLASSES] = {0};
unsigned long alertStopTimer[NUM_GLASSES] = {0};
bool isAlerting[NUM_GLASSES] = {false};
bool staleTimerArmed[NUM_GLASSES] = {false};

unsigned long lastToastTime = 0;
unsigned long lastEncoderMoveTime = 0;
unsigned long lastButtonDebounceTime = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
bool lastButtonState = HIGH;
bool buttonPressProcessed = false;
unsigned long lastReleaseTime = 0;
bool waitSecondClick = false;
bool toastAudioPending = false;
unsigned long toastScreenTimer = 0;
unsigned long toastAudioTimer = 0;

MenuState state = MAIN_SCREEN;
WorkMode workMode = MODE_MANUAL;
GlassStatus glassState[NUM_GLASSES];

//=============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
//=============================================================================

void wakeUpSystem() {
  if (state == SLEEP_MODE) {
    state = MAIN_SCREEN;
    oled.init();
    updateDisplay();
    resetSleepTimer();
    delay(100);
  }
}

//=============================================================================
// SETUP - ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ
//=============================================================================
void setup() {
  delay(1000);
  mp3Serial.begin(9600);
  for (int i = 0; i < 4; i++) pinMode(stepperPins[i], OUTPUT);
  disableMotorOutputs();

  oled.init();
  oled.clear();
  oled.setScale(2);
  oled.setCursor(10, 3);
  oled.print(F("НАЛИВАТОР"));
  oled.setScale(1);
  oled.setCursor(44, 6);
  oled.print(F("v 2.0"));

  pinMode(HOME_SW_PIN, INPUT_PULLUP);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(MP3_BUSY_PIN, INPUT_PULLUP);
  digitalWrite(PUMP_PIN, LOW);

  initSensors();
  loadSettings();

  for (int i = 0; i < NUM_GLASSES; i++) {
    glassStaleTimer[i] = 0;
    staleTimerArmed[i] = false;
    glassState[i] = EMPTY_SPACE;
  }

  if (set.savedMode == 2) workMode = MODE_ROULETTE;
  else if (set.savedMode == 1) workMode = MODE_AUTOMATIC;
  else workMode = MODE_MANUAL;

  homeStepper();
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClk = digitalRead(ENCODER_CLK);

  initLEDs();

  setMP3Volume(set.mp3Volume);
  delay(100);

  if (set.mp3Volume > 0) {
    int safeDelay = constrain(set.startDelayS, 0, 10);
    if (safeDelay > 0) delay((unsigned long)safeDelay * 1000UL);
    playTrack(SYSTEM_SOUND_FOLDER, 2);
  }

  resetSleepTimer();
  delay(100);
  updateDisplay();
}

//=============================================================================
// LOOP - ОСНОВНОЙ ЦИКЛ ПРОГРАММЫ
//=============================================================================
void loop() {
  bool hasAnyPouredGlass = false;
  for (int i = 0; i < NUM_GLASSES; i++) {
    if (glassState[i] == FILLED_GLASS) { hasAnyPouredGlass = true; break; }
  }
  if (hasAnyPouredGlass && state == MAIN_SCREEN) { resetSleepTimer(); }

  if (state != SLEEP_MODE && state != POURING && state != PUMP_FLUSH && state != TOAST_SCREEN) {
    if (millis() - lastActivityTime > SLEEP_TIMEOUT_MS) {
      state = SLEEP_MODE;
      disableMotorOutputs();
      oled.clear();
      FastLED.clear();
      FastLED.show();
    }
  }

  if (state == TOAST_SCREEN) {
    if (toastAudioPending) {
      if (millis() - toastAudioTimer > ((unsigned long)set.toastPauseS * 1000UL)) {
        toastAudioPending = false;
        if (set.mp3Volume > 0) {
          playTrack(TOAST_SOUND_FOLDER, activeToastID);
          delay(250);
        }
      }
    }
    if (!toastAudioPending && !isMP3Busy()) {
      state = MAIN_SCREEN;
      updateDisplay();
      resetSleepTimer();
      delay(200);
      return;
    }
    if (millis() - toastScreenTimer > ((unsigned long)set.toastDelayS * 1000UL)) {
      stopMP3();
      delay(50);
      state = MAIN_SCREEN;
      updateDisplay();
      resetSleepTimer();
      delay(200);
      return;
    }
    if (digitalRead(ENCODER_SW) == LOW || digitalRead(ENCODER_CLK) == LOW) {
      stopMP3();
      delay(100);
      state = MAIN_SCREEN;
      updateDisplay();
      resetSleepTimer();
      delay(300);
      return;
    }
    return;
  }

  if (state == SLEEP_MODE) {
    if (digitalRead(ENCODER_SW) == LOW || digitalRead(ENCODER_CLK) == LOW) wakeUpSystem();
    for (int i = 0; i < NUM_GLASSES; i++) {
      if (readGlassSensor(i) == LOW) { wakeUpSystem(); break; }
    }
    delay(20);
    return;
  }

  int currentClk = digitalRead(ENCODER_CLK);
  if (currentClk != lastClk && currentClk == LOW) {
    resetSleepTimer();
    lastEncoderMoveTime = millis();
    if (digitalRead(ENCODER_DT) != currentClk) handleEncoderTurn(true);
    else handleEncoderTurn(false);
  }
  lastClk = currentClk;

  bool currentButtonState = digitalRead(ENCODER_SW);
  if (currentButtonState != lastButtonState) { lastButtonDebounceTime = millis(); }
  if ((millis() - lastButtonDebounceTime) > BUTTON_DEBOUNCE_MS) {
    if (currentButtonState == LOW && !buttonPressProcessed) {
      buttonPressProcessed = true;
      resetSleepTimer();
      if (millis() - lastEncoderMoveTime > 300) {
        pressTime = millis();
        isPress = true;
        longPressTriggered = false;
      }
    }
    else if (currentButtonState == HIGH && buttonPressProcessed) {
      buttonPressProcessed = false;
      if (isPress && !longPressTriggered) {
        unsigned long holdDuration = millis() - pressTime;
        if (holdDuration >= 80 && holdDuration < 600) {
          if (waitSecondClick && (millis() - lastReleaseTime < (unsigned long)DOUBLE_CLICK_TIMEOUT_MS)) {
            waitSecondClick = false;
            handleDoubleBlank();
          } else {
            waitSecondClick = true;
            lastReleaseTime = millis();
          }
        }
      }
      isPress = false;
    }
  }

  if (isPress && !longPressTriggered && (millis() - pressTime > 600)) {
    longPressTriggered = true;
    waitSecondClick = false;
    handleLongPress();
  }

  if (waitSecondClick && (millis() - lastReleaseTime >= (unsigned long)DOUBLE_CLICK_TIMEOUT_MS)) {
    waitSecondClick = false;
    if (state == MAIN_SCREEN && workMode == MODE_MANUAL) {
      bool physicsGlassPresent = false;
      for (int g = 0; g < NUM_GLASSES; g++) {
        if (readGlassSensor(g) == LOW && glassState[g] == EMPTY_GLASS) {
          physicsGlassPresent = true;
          break;
        }
      }
      if (!physicsGlassPresent) {
        oled.clear();
        printStr(M_NO_NEW, 3, 1, true);
        delay(1500);
        updateDisplay();
      } else { handleShortClick(); }
    } else { handleShortClick(); }
  }
  lastButtonState = currentButtonState;

  if (state == CAL_PUMP && currentButtonState == LOW && !longPressTriggered) {
    static unsigned long calibPressStart = 0;
    static bool calibPressActive = false;
    if (!calibPressActive) {
      calibPressStart = millis();
      calibPressActive = true;
    }
    if (millis() - calibPressStart > 400) {
      runPumpCalibration();
      calibPressActive = false;
    }
  }

  bool allGlassesDisabled = true;
  for (int i = 0; i < NUM_GLASSES; i++) {
    if (set.shotVolumeIndividual[i] > 0) { allGlassesDisabled = false; break; }
  }

  bool tableChanged = false;
  for (int i = 0; i < NUM_GLASSES; i++) {
    if (allGlassesDisabled && state == MAIN_SCREEN) {
      if (leds[i] != CRGB::Black) { leds[i] = CRGB::Black; tableChanged = true; }
      glassTimerArmed[i] = false; glassPlacementTime[i] = 0;
      glassState[i] = EMPTY_SPACE;
      glassStaleTimer[i] = 0; alertStopTimer[i] = 0;
      isAlerting[i] = false; staleTimerArmed[i] = false;
      continue;
    }

    bool sensorState = readGlassSensor(i);
    if (sensorState == HIGH) {
      if (glassTimerArmed[i] || glassPlacementTime[i] != 0) {
        glassTimerArmed[i] = false; glassPlacementTime[i] = 0; tableChanged = true;
      }
      glassStaleTimer[i] = 0; alertStopTimer[i] = 0;
      isAlerting[i] = false; staleTimerArmed[i] = false;

      if (glassState[i] == FILLED_GLASS) {
        glassState[i] = EMPTY_SPACE;
        tableChanged = true;
        resetSleepTimer();
        if (state == MAIN_SCREEN) {
          bool triggerToast = false;
          if (workMode == MODE_ROULETTE) {
            if (millis() - lastToastTime >= ((unsigned long)set.toastDelayS * 1000UL)) triggerToast = true;
          } else {
            bool anyFullGlassLeft = false;
            for (int j = 0; j < NUM_GLASSES; j++) {
              if (j != i && readGlassSensor(j) == LOW && glassState[j] == FILLED_GLASS) {
                anyFullGlassLeft = true; break;
              }
            }
            if (!anyFullGlassLeft) triggerToast = true;
          }
          if (triggerToast) {
            lastToastTime = millis();
            activeToastID = random(1, TOTAL_TOAST_TRACKS + 1);
            state = TOAST_SCREEN;
            toastScreenTimer = millis();
            toastAudioTimer = millis();
            toastAudioPending = true;
            showToastOnDisplay();
          }
        }
      } else { glassState[i] = EMPTY_SPACE; }

      if (set.shotVolumeIndividual[i] == 0 && state == MAIN_SCREEN) {
        if (leds[i] != CRGB::Black) { leds[i] = CRGB::Black; tableChanged = true; }
      } else if (state == MAIN_SCREEN) {
        updateLedWithCheck(i, COLOR_STANDBY, BRIGHT_STANDBY);
        tableChanged = true;
      }
    } else {
      if (state == MAIN_SCREEN) {
        if (set.shotVolumeIndividual[i] == 0) {
          if (glassState[i] != DISABLED_SPACE) { glassState[i] = DISABLED_SPACE; }
          updateLedWithCheck(i, COLOR_DISABLED, BRIGHT_DISABLED);
          tableChanged = true;
          glassTimerArmed[i] = false;
        } else {
          if (glassState[i] == DISABLED_SPACE) { glassState[i] = FILLED_GLASS; tableChanged = true; }
          if (glassState[i] == EMPTY_SPACE) { glassState[i] = EMPTY_GLASS; tableChanged = true; }

          if (glassState[i] == FILLED_GLASS) {
            if (!staleTimerArmed[i] && !isAlerting[i] && set.staleTimeS > 0) {
              glassStaleTimer[i] = millis();
              staleTimerArmed[i] = true;
            }
            if (set.staleTimeS > 0 && staleTimerArmed[i]) {
              if (!isAlerting[i]) {
                if (millis() - glassStaleTimer[i] >= ((unsigned long)set.staleTimeS * 1000UL)) {
                  isAlerting[i] = true;
                  alertStopTimer[i] = millis();
                  if (set.mp3Volume > 0 && !isMP3Busy()) {
                    playTrack(SYSTEM_SOUND_FOLDER, STALE_SOUND_TRACK_ID);
                  }
                }
                updateLedWithCheck(i, COLOR_POURED, BRIGHT_POURED);
                tableChanged = true;
              } else {
                if (millis() - alertStopTimer[i] < ((unsigned long)set.staleAlertS * 1000UL)) {
                  int flashPhase = (millis() / 150) % 6;
                  CRGB nextColor;
                  if (flashPhase == 0 || flashPhase == 2 || flashPhase == 4) { nextColor = CRGB::Black; }
                  else {
                    nextColor = COLOR_POURED;
                    nextColor.nscale8_video(BRIGHT_POURED);
                  }
                  if (leds[i] != nextColor) { leds[i] = nextColor; FastLED.show(); }
                } else {
                  isAlerting[i] = false; alertStopTimer[i] = 0;
                  glassStaleTimer[i] = millis();
                  updateLedWithCheck(i, COLOR_POURED, BRIGHT_POURED);
                  tableChanged = true;
                }
              }
            } else {
              updateLedWithCheck(i, COLOR_POURED, BRIGHT_POURED);
              tableChanged = true;
            }
          }
          else if (glassState[i] == EMPTY_GLASS) {
            updateLedWithCheck(i, COLOR_ACTIVE, BRIGHT_ACTIVE);
            tableChanged = true;
            resetSleepTimer();
            if (!glassTimerArmed[i]) {
              glassPlacementTime[i] = millis();
              glassTimerArmed[i] = true;
              tableChanged = true;
            }
          }
        }
      }
    }
  }

  if (tableChanged) FastLED.show();
  if (tableChanged && state == MAIN_SCREEN) updateDisplay();

  if (state == MAIN_SCREEN && !allGlassesDisabled) {
    bool needPour = false;
    if (workMode == MODE_AUTOMATIC) {
      for (int i = 0; i < NUM_GLASSES; i++) {
        if (readGlassSensor(i) == LOW && glassState[i] == EMPTY_GLASS && glassTimerArmed[i] && set.shotVolumeIndividual[i] > 0) {
          if (millis() - glassPlacementTime[i] >= (unsigned long)set.sensorDelayMs) {
            glassTimerArmed[i] = false;
            needPour = true;
            break;
          }
        }
      }
    }
    if (needPour) {
      resetSleepTimer();
      checkAndPour();
    }
    else if (currentStepperPos != set.homePos) {
      static unsigned long lastStepMicros = 0;
      if (micros() - lastStepMicros >= 2000) {
        lastStepMicros = micros();
        int dir = (set.homePos - currentStepperPos > 0) ? 1 : -1;
        stepMotor(dir);
        currentStepperPos += dir;
        if (workMode == MODE_AUTOMATIC) {
          for (int i = 0; i < NUM_GLASSES; i++) {
            if (readGlassSensor(i) == LOW && glassState[i] == EMPTY_GLASS && set.shotVolumeIndividual[i] > 0 && !glassTimerArmed[i]) {
              glassPlacementTime[i] = millis();
              glassTimerArmed[i] = true;
              updateLedWithCheck(i, COLOR_ACTIVE, BRIGHT_ACTIVE);
              FastLED.show();
            }
          }
        }
        if (currentStepperPos == set.homePos) disableMotorOutputs();
      }
    }
  }
}