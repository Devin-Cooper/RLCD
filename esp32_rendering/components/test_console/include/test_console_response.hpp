#pragma once

namespace test_console {

/// Emit >>> OK [fmt-result]\n on a single line, atomically.
void ok(const char* fmt = "", ...) __attribute__((format(printf, 1, 2)));

/// Emit >>> ERR <code> <msg>\n on a single line, atomically.
void err(int code, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

/// Emit >>> DATA <payload>\n on a single line, atomically.
/// Terminate multi-line responses with ok() or err() after the last data().
void data(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

} // namespace test_console
