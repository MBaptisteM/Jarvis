#ifndef UI_H
#define UI_H

#define _GNU_SOURCE

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

// Same, with the text in bold green
void UiPanelBold(FILE *stream, UiKind kind, const char *title, const char *fmt,
                 ...) __attribute__((format(printf, 4, 5)));

// On stdout
void UiInfo(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void UiSuccess(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void UiQuestion(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// On stderr
void UiWarning(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void UiError(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Text between UI_BOLD and UI_NORMAL is drawn in bold in the panels (with
// colors only). The marks take no room, a bold text may span several rows.
#define UI_BOLD "\001"
#define UI_NORMAL "\002"

// Number of columns of the terminal behind this stream, 0 when it is not known
// (not a terminal, or an OS that can't tell). Read again at each call, so it
// follows the resizing of the window.
int UiTerminalColumns(FILE *stream);

// Number of bytes of text that fit in "columns" displayed characters (a
// character is never cut in the middle)
size_t UiFitBytes(const char *text, int columns);

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

#endif /* UI_H */
