#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Параметры OLED-дисплея
#define SCREEN_WIDTH 128        // Ширина экрана в пикселях
#define SCREEN_HEIGHT 64        // Высота экрана в пикселях
#define OLED_RESET    -1        // Пин сброса дисплея (-1, если не используется)
#define SCREEN_ADDRESS 0x3C     // I2C-адрес дисплея

// Параметры клавиатуры
#define KEYPAD_ROW_COUNT 5      // Количество строк клавиатуры
#define KEYPAD_COL_COUNT 4      // Количество столбцов клавиатуры
const byte KEYPAD_ROWS[KEYPAD_ROW_COUNT] = {22, 23, 24, 25, 26}; // Пины строк
const byte KEYPAD_COLS[KEYPAD_COL_COUNT] = {27, 28, 29, 30};     // Пины столбцов
#define KEY_DEBOUNCE_MS 100
#define MAX_ERROR_COUNT 10 // Увеличено для большей надёжности

// Параметры интерфейса RS-422
#define RS422_BAUD_RATE 9600    // Скорость передачи данных (бод)

// Таймауты и задержки
#define RESPONSE_TIMEOUT 3000   // Максимальное время ожидания ответа ТРК (мс)
#define INTERBYTE_TIMEOUT 3     // Таймаут между байтами в ответе (мс)
#define DELAY_AFTER_RESPONSE 3  // Задержка после получения ответа (мс)
#define DISPLAY_WELCOME_DURATION 500 // Длительность отображения приветствия (мс)
#define EDIT_TIMEOUT 10000      // Таймаут редактирования цены (мс)
#define VIEW_TIMEOUT 2000       // Таймаут просмотра цены (мс)
#define TRANSITION_TIMEOUT 2000 // Таймаут переходных состояний (мс)

// Параметры ввода цены
#define PRICE_FORMAT_LENGTH 5   // Максимальная длина ввода цены (символы)
#define PRICE_MIN 0             // Минимальная цена
#define PRICE_MAX 99999         // Максимальная цена

// Длины ответов протокола
#define STATUS_RESPONSE_LENGTH 7            // Длина ответа на команду статуса
#define MONITOR_RESPONSE_LENGTH 15          // Длина ответа на команды L и R
#define TRANSACTION_END_RESPONSE_LENGTH 27  // Длина ответа на команду T
#define TOTAL_COUNTER_RESPONSE_LENGTH 16    // Длина ответа на команду C

// Прочие параметры
#define MAX_ERROR_COUNT 5       // Максимальное число ошибок перед TRK Error
#define KEY_DEBOUNCE_MS 200     // Антидребезг клавиш (мс)
#define NOZZLE_COUNT 6          // Максимальное число рукавов
#define POST_ADDRESS 1          // Адрес поста (1-32)

// Параметры логирования
#define LOG_LEVEL_DEBUG 0       // Уровень отладочных сообщений
#define LOG_LEVEL_ERROR 1       // Уровень сообщений об ошибках
#define LOG_LEVEL LOG_LEVEL_DEBUG // Текущий уровень логирования

// Параметры кадров протокола
#define MAX_FRAME_PAYLOAD 16    // Максимальная длина полезной нагрузки кадра

#endif