#ifndef UI_H
#define UI_H

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Panels drawn like the ones of "jarvis help" :
//   ╭─ Title ───────╮
//   │ message       │
//   ╰───────────────╯
// Error = red, Warning = yellow, Success = green, Info / Question = grey.

typedef enum
{
    UI_INFO,
    UI_SUCCESS,
    UI_WARNING,
    UI_ERROR
} UiKind;

// Generic panel. title == NULL -> default title of the kind.
void UiPanel(FILE *stream, UiKind kind, const char *title, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

// On stdout
void UiInfo(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void UiSuccess(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void UiQuestion(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// On stderr
void UiWarning(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void UiError(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Replacements of errx / err : red panel on stderr, then exit(status)
void UiErrx(int status, const char *fmt, ...)
    __attribute__((noreturn, format(printf, 2, 3)));
void UiErr(int status, const char *fmt, ...)
    __attribute__((noreturn, format(printf, 2, 3)));

// Every errx / err of the project now draws a red panel. The "ERROR " /
// "WARNING " prefix of the messages is removed (the title says it already).
#undef err
#undef errx
#define err(status, ...) UiErr((status), __VA_ARGS__)
#define errx(status, ...) UiErrx((status), __VA_ARGS__)

#endif