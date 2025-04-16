#include "oled.h"
#include "config.h"

// Создаем объект для I2C дисплея SSD1306 128x64
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void initOLED() {
    u8g2.begin();
}

bool displayMessage(const char* msg) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    
    // Определяем высоту строки с учетом текущего шрифта
    int ascent = u8g2.getAscent();      // расстояние от базовой линии до верхней точки
    int descent = -u8g2.getDescent();     // делаем положительным
    int lineHeight = ascent + descent + 2; // добавляем небольшой отступ, например 2 пикселя
    
    // Ширина дисплея (можно взять из SCREEN_WIDTH, если оно определено)
    int displayWidth = 128;
    
    int y = ascent; // начинаем с высоты шрифта, чтобы первая строка не обрезалась
    const char* ptr = msg;
    
    // Буфер для формирования строки
    char line[128]; // увеличил размер буфера на случай длинных строк
    
    while (*ptr != '\0') {
        int linePos = 0;
        // Формируем строку до конца сообщения или до символа новой строки
        while (*ptr != '\0' && *ptr != '\n' && linePos < (int)(sizeof(line) - 1)) {
            line[linePos++] = *ptr++;
        }
        line[linePos] = '\0';
        
        // Если обнаружен символ новой строки, пропускаем его
        if (*ptr == '\n') {
            ptr++;
        }
        
        // Реализуем простую логику переноса: если строка длиннее дисплея, разделяем её по словам
        char *token = strtok(line, " ");
        char currentLine[128] = "";
        while(token != NULL) {
            // Проверяем, если добавление очередного слова не превышает дисплей
            char tempLine[128];
            if(strlen(currentLine) > 0)
                snprintf(tempLine, sizeof(tempLine), "%s %s", currentLine, token);
            else
                snprintf(tempLine, sizeof(tempLine), "%s", token);
            
            if(u8g2.getStrWidth(tempLine) > displayWidth) {
                // Если текущее накопленное слово уже выходит за пределы,
                // выводим текущую строку и начинаем новую
                u8g2.drawStr(0, y, currentLine);
                y += lineHeight;
                strcpy(currentLine, token); // начинаем новую строку с текущего слова
            } else {
                strcpy(currentLine, tempLine);
            }
            token = strtok(NULL, " ");
        }
        // Выводим оставшуюся часть строки
        if(strlen(currentLine) > 0) {
            u8g2.drawStr(0, y, currentLine);
            y += lineHeight;
        }
    }
    
    u8g2.sendBuffer();
    return true;
}
