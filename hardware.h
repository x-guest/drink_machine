#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <GyverOLED.h>
#include "config.h"

//=============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (extern)
//=============================================================================
extern Settings set;
extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;
extern CRGB leds[NUM_GLASSES];
extern SoftwareSerial mp3Serial;
extern const int stepperPins[];
extern int _stepIdx;
extern long currentStepperPos;
extern unsigned long lastActivityTime;

//=============================================================================
// ШАГОВЫЙ ДВИГАТЕЛЬ
//=============================================================================

// ШАГ ДВИГАТЕЛЯ (полушаговый режим)
void stepMotor(int dir) {
  _stepIdx += (dir > 0) ? 1 : -1;
  _stepIdx &= 7;
  static const byte halfSteps[] = {0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09};
  for (int i = 0; i < 4; i++) digitalWrite(stepperPins[i], bitRead(halfSteps[_stepIdx], i));
}

// ОТКЛЮЧЕНИЕ ВСЕХ ОБМОТОК ДВИГАТЕЛЯ
void disableMotorOutputs() {
  for (int i = 0; i < 4; i++) digitalWrite(stepperPins[i], LOW);
}

// ПЕРЕМЕЩЕНИЕ ДВИГАТЕЛЯ В ЗАДАННУЮ ПОЗИЦИЮ (ШАГИ)
void moveStepperTo(int targetSteps) {
  long stepsToMove = targetSteps - currentStepperPos;
  if (stepsToMove == 0) return;
  int dir = (stepsToMove > 0) ? 1 : -1;
  long totalAbs = abs(stepsToMove);
  for (long s = 0; s < totalAbs; s++) {
    stepMotor(dir);
    currentStepperPos += dir;
    delayMicroseconds(1200);
  }
  disableMotorOutputs();
}

// ПОИСК НУЛЕВОЙ ПОЗИЦИИ (ДОМ) ПО КОНЦЕВИКУ
void homeStepper() {
  while (digitalRead(HOME_SW_PIN) == HIGH) {
    stepMotor(-1);
    delayMicroseconds(2500);
  }
  currentStepperPos = set.homePos;
  disableMotorOutputs();
}

//=============================================================================
// ПОМПА
//=============================================================================

// ВКЛЮЧЕНИЕ ПОМПЫ
void pumpOn() { digitalWrite(PUMP_PIN, HIGH); }

// ВЫКЛЮЧЕНИЕ ПОМПЫ
void pumpOff() { digitalWrite(PUMP_PIN, LOW); }

//=============================================================================
// MP3 ПЛЕЕР DFPLAYER MINI
//=============================================================================

// ОТПРАВКА СЫРОЙ КОМАНДЫ В MP3 ПЛЕЕР
void sendMP3Raw(byte cmd, byte d1, byte d2) {
  byte buf[] = {0x7E, 0xFF, 0x06, cmd, 0x00, d1, d2, 0xEF};
  for (byte i = 0; i < 8; i++) mp3Serial.write(buf[i]);
}

// УСТАНОВКА ГРОМКОСТИ MP3 (0-30)
void setMP3Volume(int vol) {
  vol = constrain(vol, 0, 30);
  sendMP3Raw(0x06, 0, vol);
}

// ВОСПРОИЗВЕДЕНИЕ ТРЕКА ИЗ ПАПКИ
void playTrack(int folder, int track) {
  sendMP3Raw(0x0F, (byte)folder, track);
}

// ОСТАНОВКА ВОСПРОИЗВЕДЕНИЯ MP3
void stopMP3() { sendMP3Raw(0x16, 0, 0); }

// ПРОВЕРКА: ИГРАЕТ ЛИ MP3 (LOW = ИГРАЕТ, HIGH = СТОИТ)
bool isMP3Busy() { return digitalRead(MP3_BUSY_PIN) == LOW; }

//=============================================================================
// ДАТЧИКИ НАЛИЧИЯ РЮМОК
//=============================================================================

// ОПРОС ДАТЧИКА КОНКРЕТНОЙ РЮМКИ
bool readGlassSensor(uint8_t glassIdx) {
  int pin = pinSensors[glassIdx];
  if (pin == A6 || pin == A7) return (analogRead(pin) < 400) ? LOW : HIGH;
  return digitalRead(pin);
}

// ИНИЦИАЛИЗАЦИЯ ВСЕХ ДАТЧИКОВ РЮМОК
void initSensors() {
  for (int i = 0; i < NUM_GLASSES; i++) {
    int pin = pinSensors[i];
    if (pin != A6 && pin != A7) pinMode(pin, INPUT_PULLUP);
  }
}

//=============================================================================
// LED ПОДСВЕТКА WS2812B
//=============================================================================

// ОБНОВЛЕНИЕ ЦВЕТА LED С ПРОВЕРКОЙ ИЗМЕНЕНИЙ
void updateLedWithCheck(int idx, CRGB color, uint8_t brightness) {
  CRGB target = color;
  target.nscale8_video(brightness);
  if (leds[idx] != target) {
    leds[idx] = target;
  }
}

// ИНИЦИАЛИЗАЦИЯ LED ЛЕНТЫ
void initLEDs() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_GLASSES);
  FastLED.setBrightness(set.ledBrightness);
  FastLED.clear();
  FastLED.show();
}

//=============================================================================
// РАБОТА С EEPROM (СОХРАНЕНИЕ НАСТРОЕК)
//=============================================================================

// УСТАНОВКА ЗНАЧЕНИЙ ПО УМОЛЧАНИЮ
void setDefaultSettings() {
  set.shotVolume = DEFAULT_SHOT_VOLUME;
  set.mlTimeMs = DEFAULT_ML_TIME_MS;
  set.savedMode = DEFAULT_SAVED_MODE;
  set.toastDelayS = DEFAULT_TOAST_DELAY;
  set.toastPauseS = DEFAULT_TOAST_PAUSE;
  set.homePos = DEFAULT_HOME_OFFSET;
  set.mp3Volume = DEFAULT_VOLUME;
  set.ledBrightness = DEFAULT_LED_BRIGHTNESS;
  set.rouletteSpeed = DEFAULT_ROULETTE_SPEED;
  set.sensorDelayMs = DEFAULT_SENSOR_DELAY_MS;
  set.rouletteSpinS = DEFAULT_ROULETTE_SPIN_S;
  set.startDelayS = 3;
  set.staleTimeS = DEFAULT_STALE_TIME_S;
  set.staleAlertS = DEFAULT_STALE_ALERT_S;
  for (int i = 0; i < NUM_GLASSES; i++) {
    set.shotPos[i] = DEFAULT_SHOT_POS[i];
    set.shotVolumeIndividual[i] = DEFAULT_SHOT_VOLUME;
    set.shotVolumeCustomized[i] = false;
  }
  EEPROM.put(0, set);
  EEPROM.write(100, 43);
}

// ЗАГРУЗКА НАСТРОЕК ИЗ EEPROM
void loadSettings() {
  if (EEPROM.read(100) != 43) {
    setDefaultSettings();
  } else {
    EEPROM.get(0, set);
    if (set.toastDelayS < 1 || set.toastDelayS > 30) set.toastDelayS = DEFAULT_TOAST_DELAY;
    if (set.toastPauseS < 0 || set.toastPauseS > 10) set.toastPauseS = DEFAULT_TOAST_PAUSE;
    if (set.mp3Volume < 0 || set.mp3Volume > 30) set.mp3Volume = DEFAULT_VOLUME;
    if (set.ledBrightness < 10 || set.ledBrightness > 255) set.ledBrightness = DEFAULT_LED_BRIGHTNESS;
    if (set.rouletteSpeed < 1 || set.rouletteSpeed > 10) set.rouletteSpeed = DEFAULT_ROULETTE_SPEED;
    if (set.sensorDelayMs < 100 || set.sensorDelayMs > 3000) set.sensorDelayMs = DEFAULT_SENSOR_DELAY_MS;
    if (set.rouletteSpinS < MIN_ROULETTE_SPIN_S || set.rouletteSpinS > MAX_ROULETTE_SPIN_S) set.rouletteSpinS = DEFAULT_ROULETTE_SPIN_S;
    if (set.staleTimeS < 0 || set.staleTimeS > MAX_STALE_TIME_S) set.staleTimeS = DEFAULT_STALE_TIME_S;
    if (set.staleAlertS < 1 || set.staleAlertS > MAX_STALE_ALERT_S) set.staleAlertS = DEFAULT_STALE_ALERT_S;
    set.shotVolume = constrain(set.shotVolume, MIN_POUR_VOLUME, MAX_POUR_VOLUME);
  }
}

// СОХРАНЕНИЕ НАСТРОЕК В EEPROM
void saveSettings() { EEPROM.put(0, set); }

//=============================================================================
// ТАЙМЕР СНА (ЭНЕРГОСБЕРЕЖЕНИЕ)
//=============================================================================

// СБРОС ТАЙМЕРА АКТИВНОСТИ
void resetSleepTimer() { lastActivityTime = millis(); }

#endif