#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <FastLED.h>
#include <GyverOLED.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

// ========== ПИНЫ ПЕРИФЕРИИ ==========
#define ENCODER_CLK 2
#define ENCODER_DT  3
#define ENCODER_SW  4
#define PUMP_PIN    5
#define LED_PIN     6
#define MP3_BUSY_PIN 7

#define MP3_TX_PIN  0
#define MP3_RX_PIN  1

// ========== НАСТРОЙКИ СТОЛА И МОТОРА ==========
#define NUM_GLASSES 6
#define BUTTON_DEBOUNCE_MS 40
#define DOUBLE_CLICK_TIMEOUT_MS 280
#define SLEEP_TIMEOUT_MS 45000UL

const int stepsPerRevolution = 4046; // Ваше оригинальное число шагов
const int pinStepper[4] = {8, 9, 10, 11};
const int pinSensors[NUM_GLASSES] = {A0, A1, A2, A7, A6, A5};

// Полушаговая последовательность для мотора ULN2003
const uint8_t stepperSequence[8] = {
  0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001
};

// ========== НАСТРОЙКИ ЦВЕТОВ (FASTLED) ==========
#define COLOR_STANDBY  CRGB::Blue
#define BRIGHT_STANDBY 30

#define COLOR_ACTIVE   CRGB::Yellow
#define BRIGHT_ACTIVE  150

#define COLOR_POURED   CRGB::Green
#define BRIGHT_POURED  200

#define COLOR_DISABLED CRGB::Red
#define BRIGHT_DISABLED 15

// ========== ПАПКИ И ТРЕКИ DFPLAYER ==========
#define SYSTEM_SOUND_FOLDER  1
#define TOAST_SOUND_FOLDER   2

#define ROULETTE_TRACK_ID    1
#define STALE_SOUND_TRACK_ID 2
#define TOTAL_TOAST_TRACKS   10

// ========== ЛИМИТЫ МЕНЮ ==========
#define MIN_POUR_VOLUME     5
#define MAX_POUR_VOLUME     100
#define VOLUME_STEP_ML      5

// Вместо лимитов рулетки делаем лимиты для Зазывалы (в минутах)
#define MAX_INVITE_TIME_M   60
#define INVITE_TIME_STEP_M  5

#define MAX_STALE_TIME_S    120
#define STALE_TIME_STEP_S   5

#define MAX_STALE_ALERT_S   15
#define STALE_ALERT_STEP_S  1

#define CALIBRATION_STEP_STEPS 10
#define TOTAL_MENU_ITEMS    11

// ========== СТРУКТУРА ДАННЫХ EEPROM ==========
struct Settings {
  int homePos;
  int shotPos[NUM_GLASSES];
  int mlTimeMs;
  int shotVolume;
  int shotVolumeIndividual[NUM_GLASSES];
  bool shotVolumeCustomized[NUM_GLASSES];
  int ledBrightness;
  int sensorDelayMs;
  int mp3Volume;
  int toastPauseS;
  int toastDelayS;
  int startDelayS;
  int inviteTimeM;      // <- Переименовали rouletteSpinS в inviteTimeM (время в минутах)
  int inviteEnabled;    // <- Переименовали rouletteSpeed в inviteEnabled (1 - вкл, 0 - выкл)
  int staleTimeS;
  int staleAlertS;
  int savedMode;
};



#endif
