#include "test_console_response.hpp"
#include "test_console_context.hpp"
#include "esp_console.h"
#include "input_queue.hpp"

#include <cstring>
#include <cstdlib>
#include <cctype>

namespace test_console {

static bool push_event(const input::InputEvent& ie) {
    return input::globalInputQueue().push(ie, 100);
}

// --- btn <a|b> <short|long> ---
static int cmd_btn(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: btn <a|b> <short|long>"); return 1; }
    input::InputEvent ie{};
    ie.source = input::Source::Button;
    if      (!strcmp(argv[1], "a")) ie.button_id = 0;
    else if (!strcmp(argv[1], "b")) ie.button_id = 1;
    else { err(2, "bad button '%s'", argv[1]); return 1; }
    if      (!strcmp(argv[2], "short")) ie.type = input::EventType::ButtonShort;
    else if (!strcmp(argv[2], "long"))  ie.type = input::EventType::ButtonLong;
    else { err(3, "bad kind '%s'", argv[2]); return 1; }
    if (!push_event(ie)) { err(4, "queue push failed"); return 1; }
    ok("%s", "");
    return 0;
}

static bool push_byte(uint8_t b) {
    input::InputEvent ie{};
    ie.source = input::Source::Keyboard;
    ie.type = input::EventType::Keypress;
    ie.data[0] = b;
    ie.data_length = 1;
    return push_event(ie);
}

static bool push_bytes(const uint8_t* bytes, size_t n) {
    if (n > 8) return false;
    input::InputEvent ie{};
    ie.source = input::Source::Keyboard;
    ie.type = input::EventType::Keypress;
    for (size_t i = 0; i < n; ++i) ie.data[i] = bytes[i];
    ie.data_length = (uint8_t)n;
    return push_event(ie);
}

// --- key-press <text> ---
static int cmd_key_press(int argc, char** argv) {
    if (argc < 2) { err(1, "usage: key-press <text>"); return 1; }
    int n = 0;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            if (!push_byte(' ')) { err(4, "queue full"); return 1; }
            ++n;
        }
        for (const char* p = argv[i]; *p; ++p) {
            if (!push_byte((uint8_t)*p)) { err(4, "queue full"); return 1; }
            ++n;
        }
    }
    ok("%d", n);
    return 0;
}

static int cmd_key_enter(int, char**)     { if (!push_byte('\r')) { err(4, "queue full"); return 1; } ok("%s", ""); return 0; }
static int cmd_key_esc(int, char**)       { if (!push_byte(0x1B)) { err(4, "queue full"); return 1; } ok("%s", ""); return 0; }
static int cmd_key_tab(int, char**)       { if (!push_byte('\t')) { err(4, "queue full"); return 1; } ok("%s", ""); return 0; }
static int cmd_key_backspace(int, char**) { if (!push_byte(0x08)) { err(4, "queue full"); return 1; } ok("%s", ""); return 0; }

// --- key-arrow <up|down|left|right> ---
static int cmd_key_arrow(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: key-arrow <up|down|left|right>"); return 1; }
    uint8_t code;
    if      (!strcmp(argv[1], "up"))    code = 'A';
    else if (!strcmp(argv[1], "down"))  code = 'B';
    else if (!strcmp(argv[1], "right")) code = 'C';
    else if (!strcmp(argv[1], "left"))  code = 'D';
    else { err(2, "bad arrow '%s'", argv[1]); return 1; }
    const uint8_t seq[3] = {0x1B, '[', code};
    if (!push_bytes(seq, 3)) { err(4, "queue push failed"); return 1; }
    ok("%s", "");
    return 0;
}

// --- key-fn <1-12> ---
static int cmd_key_fn(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: key-fn <1-12>"); return 1; }
    int n = atoi(argv[1]);
    if (n < 1 || n > 12) { err(2, "fn key out of range"); return 1; }
    uint8_t seq[8];
    size_t sl = 0;
    if (n <= 4) {
        static const uint8_t letter[4] = {'P','Q','R','S'};
        seq[0]=0x1B; seq[1]='O'; seq[2]=letter[n-1]; sl=3;
    } else {
        static const uint8_t num[8][2] = {
            {'1','5'}, {'1','7'}, {'1','8'}, {'1','9'},
            {'2','0'}, {'2','1'}, {'2','3'}, {'2','4'}
        };
        seq[0]=0x1B; seq[1]='['; seq[2]=num[n-5][0]; seq[3]=num[n-5][1]; seq[4]='~'; sl=5;
    }
    if (!push_bytes(seq, sl)) { err(4, "queue push failed"); return 1; }
    ok("%s", "");
    return 0;
}

// --- key-ctrl <letter> ---
static int cmd_key_ctrl(int argc, char** argv) {
    if (argc != 2 || !argv[1][0] || argv[1][1]) {
        err(1, "usage: key-ctrl <letter>"); return 1;
    }
    char c = (char)toupper((unsigned char)argv[1][0]);
    if (c < 'A' || c > 'Z') { err(2, "not a letter"); return 1; }
    if (!push_byte((uint8_t)(c & 0x1F))) { err(4, "queue full"); return 1; }
    ok("%s", "");
    return 0;
}

void registerInjectionCommands() {
    const esp_console_cmd_t cmds[] = {
        {"btn",           "btn <a|b> <short|long>",           nullptr, cmd_btn,           nullptr, nullptr, nullptr},
        {"key-press",     "key-press <text>",                 nullptr, cmd_key_press,     nullptr, nullptr, nullptr},
        {"key-enter",     "Emit '\\r' keypress",              nullptr, cmd_key_enter,     nullptr, nullptr, nullptr},
        {"key-esc",       "Emit 0x1B keypress",               nullptr, cmd_key_esc,       nullptr, nullptr, nullptr},
        {"key-tab",       "Emit '\\t' keypress",              nullptr, cmd_key_tab,       nullptr, nullptr, nullptr},
        {"key-backspace", "Emit 0x08 keypress",               nullptr, cmd_key_backspace, nullptr, nullptr, nullptr},
        {"key-arrow",     "key-arrow <up|down|left|right>",   nullptr, cmd_key_arrow,     nullptr, nullptr, nullptr},
        {"key-fn",        "key-fn <1-12>",                    nullptr, cmd_key_fn,        nullptr, nullptr, nullptr},
        {"key-ctrl",      "key-ctrl <letter>",                nullptr, cmd_key_ctrl,      nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

} // namespace test_console
