#include <Keypad.h>
#include "keypad.h"
#include "config.h"

const byte ROWS = KEYPAD_ROW_COUNT;
const byte COLS = KEYPAD_COL_COUNT;
char keys[ROWS][COLS] = {
    {'A', 'F', 'G', 'H'},
    {'B', '1', '2', '3'},
    {'C', '4', '5', '6'},
    {'D', '7', '8', '9'},
    {'E', '*', '0', 'K'}
};
static Keypad keypad = Keypad(makeKeymap(keys), (byte*)KEYPAD_ROWS, (byte*)KEYPAD_COLS, ROWS, COLS);

char getKeypadKey() {
    return keypad.getKey();
}