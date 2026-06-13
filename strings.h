#ifndef STRINGS_H
#define STRINGS_H

#include <Arduino.h>

//=============================================================================
// ВСЕ PROGMEM СТРОКИ ВЫНЕСЕНЫ В ОТДЕЛЬНЫЙ ФАЙЛ ДЛЯ ЭКОНОМИИ FLASH
//=============================================================================

// Основные строки меню
const char T_MANUAL[] PROGMEM = "РУЧНОЙ";
const char T_AUTO[] PROGMEM = "АВТОМАТ";
const char T_ROULETTE[] PROGMEM = "РУЛЕТКА";
const char T_BACK[] PROGMEM = "НАЗАД";
const char T_ON[] PROGMEM = "ВКЛ";
const char T_OFF[] PROGMEM = "ВЫКЛ";
const char T_MENU[] PROGMEM = "МЕНЮ";
const char T_HOME_BASE[] PROGMEM = "ДОМ";
const char T_RESET[] PROGMEM = "СБРОС";
const char T_FLUSH[] PROGMEM = "ПРОЛИВ...";
const char T_OK[] PROGMEM = "ОК";
const char T_DONE[] PROGMEM = "ВЫПОЛНЕНО!";
const char T_ROT_HINT[] PROGMEM = "Круть Клик";

// Сообщения системы
const char M_WAIT_GLASS[] PROGMEM = "Жду рюмку...";
const char M_CLICK_POUR[] PROGMEM = "Клик";
const char M_NO_GLASS[] PROGMEM = "НЕТ РЮМОК!";
const char M_PLACE_GLASS[] PROGMEM = "УСТАНОВИТЕ РЮМКУ!";
const char M_REMOVED[] PROGMEM = "РЮМКА СНЯТА!";
const char M_ABORT[] PROGMEM = "НАЛИВ ПРЕРВАН";
const char M_NO_NEW[] PROGMEM = "НЕТ НОВЫХ РЮМОК!";
const char M_ROULETTE_SPIN[] PROGMEM = "РУЛЕТКА...";
const char M_RESET_CONF[] PROGMEM = "СБРОС НАСТРОЕК";
const char M_FLUSH_CONF[] PROGMEM = "ПРОМЫВКА СИСТЕМЫ";
const char M_CALIB_TITLE[] PROGMEM = "ВРЕМЯ НАЛИВА 50МЛ";

// Пункты меню
const char M0[] PROGMEM = "РЕЖИМ";
const char M1[] PROGMEM = "УГЛЫ";
const char M2[] PROGMEM = "ОБЪЕМ";
const char M3[] PROGMEM = "РУЛЕТКА";
const char M4[] PROGMEM = "ТАМАДА";
const char M5[] PROGMEM = "ПИНАТЕЛЬ";
const char M6[] PROGMEM = "ПОМПА";
const char M7[] PROGMEM = "СВЕТ";
const char M8[] PROGMEM = "ЗАДЕРЖКА";
const char M9[] PROGMEM = "ПРОЛИВ";
const char M10[] PROGMEM = "СБРОС";

// Массив указателей на пункты меню
extern const char* const MENU[];

#endif