#include "ui.h"

typedef struct
{
    const char *border;
    const char *title;
    const char *reset;
} Style;

static Style __GetStyle(FILE *stream, UiKind kind)
{
    Style style = { "", "", "" };
    if (!isatty(fileno(stream)) || getenv("NO_COLOR") != NULL)
        return style;

    style.reset = "\033[0m";
    switch (kind)
    {
    case UI_ERROR:
        style.border = "\033[31m";
        style.title = "\033[1;31m";
        break;
    case UI_WARNING:
        style.border = "\033[33m";
        style.title = "\033[1;33m";
        break;
    case UI_SUCCESS:
        style.border = "\033[32m";
        style.title = "\033[1;32m";
        break;
    default:
        style.border = "\033[90m";
        style.title = "\033[1m";
        break;
    }
    return style;
}

static const char *__DefaultTitle(UiKind kind)
{
    switch (kind)
    {
    case UI_ERROR:
        return "Error";
    case UI_WARNING:
        return "Warning";
    case UI_SUCCESS:
        return "Success";
    default:
        return "Info";
    }
}

static int __Width(FILE *stream)
{
    int width = 0;
    struct winsize ws;
    if (isatty(fileno(stream)) && ioctl(fileno(stream), TIOCGWINSZ, &ws) == 0)
        width = ws.ws_col;
    if (width <= 0 && getenv("COLUMNS") != NULL)
        width = atoi(getenv("COLUMNS"));
    if (width <= 0)
        width = 80;
    if (width > 120)
        width = 120;
    if (width < 50)
        width = 50;
    return width;
}

// Size in bytes of the UTF-8 character starting at p
static size_t __CharLen(const char *p)
{
    unsigned char c = (unsigned char)*p;
    size_t n = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    for (size_t i = 1; i < n; i++)
        if (((unsigned char)p[i] & 0xC0) != 0x80)
            return i;
    return n;
}

// Number of displayed characters in the first "bytes" bytes of p
static int __Cols(const char *p, size_t bytes)
{
    int cols = 0;
    for (size_t i = 0; i < bytes; i += __CharLen(p + i))
        cols++;
    return cols;
}

// Bytes of p that fit in one row (cut on a space when possible)
static size_t __Chunk(const char *p, int width)
{
    size_t i = 0;
    size_t last_space = 0;
    int cols = 0;
    while (p[i] != '\0' && cols < width)
    {
        if (p[i] == ' ' && i > 0 && p[i - 1] != ' ')
            last_space = i;
        i += __CharLen(p + i);
        cols++;
    }
    if (p[i] == '\0' || p[i] == ' ')
        return i;
    return last_space > 0 ? last_space : i;
}

// Remove ANSI codes, \r and tabs, and the trailing newlines (keeps the box
// aligned)
static void __Sanitize(char *s)
{
    char *w = s;
    for (char *r = s; *r != '\0'; r++)
    {
        if (*r == '\033' && r[1] == '[')
        {
            r += 2;
            while (*r != '\0' && !isalpha((unsigned char)*r))
                r++;
            if (*r == '\0')
                break;
            continue;
        }
        if (*r == '\r')
            continue;
        *w++ = *r == '\t' ? ' ' : *r;
    }
    *w = '\0';
    while (w > s && w[-1] == '\n')
        *--w = '\0';
}

static const char *__StripPrefix(const char *message)
{
    static const char *prefixes[] = { "ERROR: ", "ERROR ", "WARNING: ",
                                      "WARNING ", NULL };
    for (int i = 0; prefixes[i] != NULL; i++)
        if (strncmp(message, prefixes[i], strlen(prefixes[i])) == 0)
            return message + strlen(prefixes[i]);
    return message;
}

// Set by UiPanelBold : the text of the rows is drawn in bold
static int __bold_text = 0;

static void __Row(FILE *mem, const Style *style, int text_width, const char *p,
                  size_t n)
{
    int pad = text_width - __Cols(p, n);
    const char *text = __bold_text && style->reset[0] != '\0' ? "\033[1m" : "";
    const char *end = text[0] != '\0' ? style->reset : "";
    fprintf(mem, "%s│%s %s%.*s%s%*s %s│%s\n", style->border, style->reset, text,
            (int)n, p, end, pad, "", style->border, style->reset);
}

static void __Render(FILE *stream, UiKind kind, const char *title,
                     const char *suffix, const char *fmt, va_list ap)
{
    va_list copy;
    va_copy(copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0)
        return;

    size_t suffix_len = suffix != NULL ? strlen(suffix) : 0;
    char *message = malloc((size_t)needed + suffix_len + 1);
    if (message == NULL)
        return;
    vsnprintf(message, (size_t)needed + 1, fmt, ap);
    if (suffix != NULL)
        memcpy(message + needed, suffix, suffix_len + 1);

    __Sanitize(message);
    char *line = (char *)__StripPrefix(message);

    if (title == NULL)
        title = __DefaultTitle(kind);

    Style style = __GetStyle(stream, kind);
    int width = __Width(stream);
    int text_width = width - 4;

    char *buffer = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buffer, &size);
    if (mem == NULL)
    {
        fprintf(stream, "%s\n", line);
        free(message);
        return;
    }

    // Top : ╭─ Title ────╮
    int dashes = width - __Cols(title, strlen(title)) - 5;
    if (dashes < 1)
        dashes = 1;
    fprintf(mem, "%s╭─ %s%s%s%s ", style.border, style.reset, style.title, title,
            style.reset);
    fprintf(mem, "%s", style.border);
    for (int i = 0; i < dashes; i++)
        fprintf(mem, "─");
    fprintf(mem, "╮%s\n", style.reset);

    // Rows
    while (1)
    {
        char *newline = strchr(line, '\n');
        if (newline != NULL)
            *newline = '\0';

        const char *p = line;
        if (*p == '\0')
            __Row(mem, &style, text_width, "", 0);
        while (*p != '\0')
        {
            size_t n = __Chunk(p, text_width);
            __Row(mem, &style, text_width, p, n);
            p += n;
            while (*p == ' ')
                p++;
        }

        if (newline == NULL)
            break;
        line = newline + 1;
    }

    // Bottom : ╰────╯
    fprintf(mem, "%s╰", style.border);
    for (int i = 0; i < width - 2; i++)
        fprintf(mem, "─");
    fprintf(mem, "╯%s\n", style.reset);
    fclose(mem);

    // One single write so that two processes do not mix their panels
    fflush(stdout);
    fflush(stream);
    size_t done = 0;
    int fd = fileno(stream);
    while (done < size)
    {
        ssize_t written = write(fd, buffer + done, size - done);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            break;
        done += (size_t)written;
    }

    free(buffer);
    free(message);
}

void UiPanel(FILE *stream, UiKind kind, const char *title, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __Render(stream, kind, title, NULL, fmt, ap);
    va_end(ap);
}

// Same as UiPanel, with the text in bold (to be sure it is seen)
void UiPanelBold(FILE *stream, UiKind kind, const char *title, const char *fmt,
                 ...)
{
    va_list ap;
    va_start(ap, fmt);
    __bold_text = 1;
    __Render(stream, kind, title, NULL, fmt, ap);
    __bold_text = 0;
    va_end(ap);
}

#define UI_SHORTCUT(name, stream, kind, title)                                 \
    void name(const char *fmt, ...)                                            \
    {                                                                          \
        va_list ap;                                                            \
        va_start(ap, fmt);                                                     \
        __Render(stream, kind, title, NULL, fmt, ap);                          \
        va_end(ap);                                                            \
    }

UI_SHORTCUT(UiInfo, stdout, UI_INFO, NULL)
UI_SHORTCUT(UiSuccess, stdout, UI_SUCCESS, NULL)
UI_SHORTCUT(UiQuestion, stdout, UI_INFO, "Question")
UI_SHORTCUT(UiWarning, stderr, UI_WARNING, NULL)
UI_SHORTCUT(UiError, stderr, UI_ERROR, NULL)

void UiErrx(int status, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __Render(stderr, UI_ERROR, NULL, NULL, fmt, ap);
    va_end(ap);
    exit(status);
}

void UiErr(int status, const char *fmt, ...)
{
    int saved_errno = errno;
    char suffix[256];
    snprintf(suffix, sizeof(suffix), ": %s", strerror(saved_errno));

    va_list ap;
    va_start(ap, fmt);
    __Render(stderr, UI_ERROR, NULL, suffix, fmt, ap);
    va_end(ap);
    exit(status);
}