#include "hid_translate.hpp"

namespace ble_hid {

// Standard US keyboard layout: HID usage ID → ASCII (unshifted)
static const char HID_TO_ASCII[128] = {
    0,    0,    0,    0,   'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',  'q',  'r',  's',  't',
    'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2',
    '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',
    '\r', 0x1b, 0x7f, '\t', ' ',  '-',  '=',  '[',
    ']',  '\\', 0,    ';',  '\'', '`',  ',',  '.',
    '/',  0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    '/',  '*',  '-',  '+',
    '\r', '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  '0',  '.',  0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
};

static const char HID_TO_ASCII_SHIFT[128] = {
    0,    0,    0,    0,   'A',  'B',  'C',  'D',
    'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',
    'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',
    'U',  'V',  'W',  'X',  'Y',  'Z',  '!',  '@',
    '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',
    '\r', 0x1b, 0x7f, '\t', ' ',  '_',  '+',  '{',
    '}',  '|',  0,    ':',  '"',  '~',  '<',  '>',
    '?',  0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    '/',  '*',  '-',  '+',
    '\r', '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  '0',  '.',  0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
};

KeyEvent translateKeycode(uint8_t keycode, uint8_t modifiers) noexcept {
    KeyEvent event = {};
    bool shift = (modifiers & (MOD_LSHIFT | MOD_RSHIFT)) != 0;
    bool ctrl  = (modifiers & (MOD_LCTRL  | MOD_RCTRL )) != 0;
    bool gui   = (modifiers & (MOD_LGUI   | MOD_RGUI  )) != 0;

    // CMD/Win + Space → same byte sequence as F1 (ESC O P). Lets keyboards
    // without dedicated F-keys trigger the menu via the standard GUI+Space
    // combo, and reuses the F1 intercept in the dispatcher.
    if (gui && keycode == 0x2C) {
        event.bytes[0] = 0x1B;
        event.bytes[1] = 'O';
        event.bytes[2] = 'P';
        event.length = 3;
        return event;
    }

    switch (keycode) {
    case 0x4F: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='C'; event.length=3; return event;
    case 0x50: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='D'; event.length=3; return event;
    case 0x51: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='B'; event.length=3; return event;
    case 0x52: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='A'; event.length=3; return event;

    case 0x49: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='2'; event.bytes[3]='~'; event.length=4; return event;
    case 0x4C: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='3'; event.bytes[3]='~'; event.length=4; return event;
    case 0x4A: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='H'; event.length=3; return event;
    case 0x4D: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='F'; event.length=3; return event;
    case 0x4B: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='5'; event.bytes[3]='~'; event.length=4; return event;
    case 0x4E: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='6'; event.bytes[3]='~'; event.length=4; return event;

    case 0x3A: event.bytes[0]=0x1B; event.bytes[1]='O'; event.bytes[2]='P'; event.length=3; return event;
    case 0x3B: event.bytes[0]=0x1B; event.bytes[1]='O'; event.bytes[2]='Q'; event.length=3; return event;
    case 0x3C: event.bytes[0]=0x1B; event.bytes[1]='O'; event.bytes[2]='R'; event.length=3; return event;
    case 0x3D: event.bytes[0]=0x1B; event.bytes[1]='O'; event.bytes[2]='S'; event.length=3; return event;
    case 0x3E: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='1'; event.bytes[3]='5'; event.bytes[4]='~'; event.length=5; return event;
    case 0x3F: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='1'; event.bytes[3]='7'; event.bytes[4]='~'; event.length=5; return event;
    case 0x40: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='1'; event.bytes[3]='8'; event.bytes[4]='~'; event.length=5; return event;
    case 0x41: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='1'; event.bytes[3]='9'; event.bytes[4]='~'; event.length=5; return event;
    case 0x42: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='2'; event.bytes[3]='0'; event.bytes[4]='~'; event.length=5; return event;
    case 0x43: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='2'; event.bytes[3]='1'; event.bytes[4]='~'; event.length=5; return event;
    case 0x44: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='2'; event.bytes[3]='3'; event.bytes[4]='~'; event.length=5; return event;
    case 0x45: event.bytes[0]=0x1B; event.bytes[1]='['; event.bytes[2]='2'; event.bytes[3]='4'; event.bytes[4]='~'; event.length=5; return event;
    }

    if (keycode < 128) {
        char ch;
        if (ctrl && keycode >= 0x04 && keycode <= 0x1D) {
            ch = static_cast<char>(keycode - 0x04 + 1);
        } else if (shift) {
            ch = HID_TO_ASCII_SHIFT[keycode];
        } else {
            ch = HID_TO_ASCII[keycode];
        }
        if (ch != 0) {
            event.bytes[0] = static_cast<uint8_t>(ch);
            event.length = 1;
        }
    }
    return event;
}

} // namespace ble_hid
