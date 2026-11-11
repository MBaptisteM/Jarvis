#include "tidy_handler.h"

// Folders never checked
static const char *IGNORED_FOLDERS[] = { ".git", ".git.jarvis-tmp",
                                         "node_modules" };

// Files checked
static const char *SOURCE_EXTENSIONS[] = { ".c",   ".h",  ".cc",  ".cpp",
                                           ".cxx", ".hh", ".hpp", ".hxx" };

// Files checked with the rules of the C coding style
static const char *C_SOURCE_EXTENSIONS[] = { ".c" };
static const char *C_HEADER_EXTENSIONS[] = { ".h" };

// Files that are headers (include guard checked)
static const char *HEADER_EXTENSIONS[] = { ".h", ".hh", ".hpp", ".hxx" };

static size_t files_checked = 0;
static size_t files_with_problems = 0;
static size_t total_problems = 0;
static size_t total_fixable = 0; // of total_problems
static size_t files_fixed = 0;
static size_t total_fixed = 0;
static int fix_mode = 0; // 1 : the problems that can be fixed are fixed

// Problems of one file

typedef struct
{
    size_t line;
    size_t order;
    int fixable; // jarvis tidy --fix can fix it
    char *message;
} Problem;

typedef struct
{
    Problem *items;
    size_t count;
    size_t capacity;
} ProblemList;

static void __VAdd(ProblemList *list, size_t line, int fixable, const char *fmt,
                   va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0)
        return;

    char *message = malloc((size_t)needed + 1);
    if (message == NULL)
        err(EXIT_FAILURE, "malloc");
    vsnprintf(message, (size_t)needed + 1, fmt, args);

    if (list->count == list->capacity)
    {
        size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        Problem *grown = realloc(list->items, capacity * sizeof(Problem));
        if (grown == NULL)
            err(EXIT_FAILURE, "realloc");
        list->items = grown;
        list->capacity = capacity;
    }

    list->items[list->count].line = line;
    list->items[list->count].order = list->count;
    list->items[list->count].fixable = fixable;
    list->items[list->count].message = message;
    list->count++;
}

// A problem that jarvis tidy can't fix
static void __Add(ProblemList *list, size_t line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __VAdd(list, line, 0, fmt, ap);
    va_end(ap);
}

// A problem that jarvis tidy --fix fixes
static void __AddFixable(ProblemList *list, size_t line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __VAdd(list, line, 1, fmt, ap);
    va_end(ap);
}

static int __CompareProblems(const void *a, const void *b)
{
    const Problem *left = a;
    const Problem *right = b;
    if (left->line != right->line)
        return left->line < right->line ? -1 : 1;
    if (left->order != right->order)
        return left->order < right->order ? -1 : 1;
    return 0;
}

static void __FreeProblems(ProblemList *list)
{
    for (size_t i = 0; i < list->count; i++)
        free(list->items[i].message);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

// Fixes of one file : lines to replace, lines to insert after a line, and a
// missing newline at the end of the file

typedef struct
{
    char **replace; // by line (from 0), NULL when the line is kept
    char **after; // by line (from 0), line to insert after it
    size_t line_count;
    size_t count; // number of problems fixed
    int add_newline;
} FixSet;

static void __FixInit(FixSet *fix, size_t line_count)
{
    fix->replace = calloc(line_count + 1, sizeof(char *));
    fix->after = calloc(line_count + 1, sizeof(char *));
    if (fix->replace == NULL || fix->after == NULL)
        err(EXIT_FAILURE, "calloc");
    fix->line_count = line_count;
    fix->count = 0;
    fix->add_newline = 0;
}

static void __FixFree(FixSet *fix)
{
    for (size_t i = 0; i < fix->line_count; i++)
    {
        free(fix->replace[i]);
        free(fix->after[i]);
    }
    free(fix->replace);
    free(fix->after);
}

// Write a text in a slot of the fix (a slot counts for one fix)
static void __FixSet(FixSet *fix, char **slot, const char *fmt, ...)
{
    char buffer[1100];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    char *text = strdup(buffer);
    if (text == NULL)
        err(EXIT_FAILURE, "strdup");
    if (*slot == NULL)
        fix->count++;
    else
        free(*slot);
    *slot = text;
}

// "#endif" -> "#endif /* NAME */" (the indentation and the spaces after the #
// are kept, the old comment is replaced)
static void __FixEndif(FixSet *fix, size_t l, const char *line, size_t length,
                       const char *name)
{
    char buffer[1024];
    if (length >= sizeof(buffer))
        length = sizeof(buffer) - 1;
    memcpy(buffer, line, length);
    buffer[length] = '\0';

    const char *keyword = strstr(buffer, "endif");
    if (keyword == NULL)
        return;
    int prefix = (int)(keyword - buffer) + 5;
    __FixSet(fix, &fix->replace[l], "%.*s /* %s */", prefix, buffer, name);
}

// Small helpers

static int __IsIdentChar(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

// Trim in place, returns the first non-blank character
static char *__Trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    size_t length = strlen(s);
    while (length > 0 && isspace((unsigned char)s[length - 1]))
        s[--length] = '\0';
    return s;
}

static int __HasWord(const char *s, size_t length, const char *word)
{
    size_t word_length = strlen(word);
    for (size_t i = 0; i + word_length <= length; i++)
        if (strncmp(s + i, word, word_length) == 0
            && (i == 0 || !__IsIdentChar(s[i - 1]))
            && (i + word_length == length
                || !__IsIdentChar(s[i + word_length])))
            return 1;
    return 0;
}

static int __EndsWithOneOf(const char *name, const char *list[], size_t count)
{
    size_t length = strlen(name);
    for (size_t i = 0; i < count; i++)
    {
        size_t extension_length = strlen(list[i]);
        if (length > extension_length
            && strcasecmp(name + length - extension_length, list[i]) == 0)
            return 1;
    }
    return 0;
}

// "test.h" -> "TEST_H"
static void __GuardName(const char *name, char *out, size_t size)
{
    size_t k = 0;
    for (const char *c = name; *c != '\0' && k + 1 < size; c++)
        out[k++] =
            isalnum((unsigned char)*c) ? (char)toupper((unsigned char)*c) : '_';
    out[k] = '\0';
}

// Source : the text of a file and a "clean" version of it

typedef struct
{
    char *text; // the file as it is
    char *comment_free; // comments and the content of literals are blanked
    char *clean; // same, and the preprocessor lines are blanked too
    size_t length;
    size_t *line_start; // offset of each line, line_start[line_count] = length
    size_t line_count;
    // 1 : first line of a preprocessor directive, 2 : its next lines
    unsigned char *preproc;
} Source;

// Same size as the text, comments and the content of strings / characters are
// replaced by spaces (new lines are kept so that the offsets do not move)
static char *__Blank(const char *text, size_t length)
{
    char *out = malloc(length + 1);
    if (out == NULL)
        err(EXIT_FAILURE, "malloc");
    memcpy(out, text, length);
    out[length] = '\0';

    size_t i = 0;
    while (i < length)
    {
        if (text[i] == '/' && i + 1 < length && text[i + 1] == '/')
        {
            while (i < length && text[i] != '\n')
                out[i++] = ' ';
        }
        else if (text[i] == '/' && i + 1 < length && text[i + 1] == '*')
        {
            out[i++] = ' ';
            out[i++] = ' ';
            while (i < length
                   && !(text[i] == '*' && i + 1 < length && text[i + 1] == '/'))
            {
                if (text[i] != '\n')
                    out[i] = ' ';
                i++;
            }
            if (i < length)
            {
                out[i++] = ' ';
                out[i++] = ' ';
            }
        }
        else if (text[i] == '"' || text[i] == '\'')
        {
            char quote = text[i++];
            while (i < length && text[i] != quote && text[i] != '\n')
            {
                if (text[i] == '\\' && i + 1 < length && text[i + 1] != '\n')
                    out[i++] = ' ';
                out[i++] = ' ';
            }
            if (i < length && text[i] == quote)
                i++;
        }
        else
            i++;
    }

    return out;
}

static void __FreeSource(Source *src)
{
    free(src->text);
    free(src->comment_free);
    free(src->clean);
    free(src->line_start);
    free(src->preproc);
    memset(src, 0, sizeof(*src));
}

// 0 : loaded | 1 : binary file, ignored | -1 : impossible to read
static int __LoadSource(const char *path, Source *src)
{
    memset(src, 0, sizeof(*src));

    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return -1;

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return -1;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return -1;
    }

    src->text = malloc((size_t)size + 1);
    if (src->text == NULL)
        err(EXIT_FAILURE, "malloc");
    src->length = fread(src->text, 1, (size_t)size, file);
    fclose(file);
    src->text[src->length] = '\0';

    if (memchr(src->text, '\0', src->length) != NULL)
    {
        __FreeSource(src);
        return 1;
    }

    size_t newlines = 0;
    for (size_t i = 0; i < src->length; i++)
        if (src->text[i] == '\n')
            newlines++;
    src->line_count =
        newlines + (src->length > 0 && src->text[src->length - 1] != '\n');

    src->line_start = malloc((src->line_count + 1) * sizeof(size_t));
    if (src->line_start == NULL)
        err(EXIT_FAILURE, "malloc");
    size_t line = 0;
    if (src->line_count > 0)
        src->line_start[line++] = 0;
    for (size_t i = 0; i < src->length; i++)
        if (src->text[i] == '\n' && i + 1 < src->length)
            src->line_start[line++] = i + 1;
    src->line_start[src->line_count] = src->length;

    src->comment_free = __Blank(src->text, src->length);
    src->clean = malloc(src->length + 1);
    src->preproc = calloc(src->line_count + 1, 1);
    if (src->clean == NULL || src->preproc == NULL)
        err(EXIT_FAILURE, "malloc");
    memcpy(src->clean, src->comment_free, src->length + 1);

    // Preprocessor lines (with their continuation lines) are blanked in
    // "clean" : their braces and parentheses are not code
    int continued = 0;
    for (size_t l = 0; l < src->line_count; l++)
    {
        size_t start = src->line_start[l];
        size_t end = src->line_start[l + 1];
        if (end > start && src->clean[end - 1] == '\n')
            end--;

        int is_directive = continued;
        int first = 0;
        if (!is_directive)
        {
            size_t k = start;
            while (k < end && isspace((unsigned char)src->clean[k]))
                k++;
            is_directive = k < end && src->clean[k] == '#';
            first = is_directive;
        }

        if (!is_directive)
        {
            continued = 0;
            continue;
        }

        src->preproc[l] = first ? 1 : 2;

        size_t last = end;
        while (last > start && src->clean[last - 1] == '\r')
            last--;
        continued = last > start && src->clean[last - 1] == '\\';

        for (size_t k = start; k < end; k++)
            src->clean[k] = ' ';
    }

    return 0;
}

// Index (from 0) of the line containing this offset
static size_t __LineOf(const Source *src, size_t offset)
{
    size_t low = 0;
    size_t high = src->line_count;
    while (high - low > 1)
    {
        size_t middle = (low + high) / 2;
        if (src->line_start[middle] <= offset)
            low = middle;
        else
            high = middle;
    }
    return low;
}

// #if / #endif and include guard

typedef struct
{
    char name[256];
    size_t line;
} IfEntry;

// The "variable" of a #if : FOO for "#ifdef FOO", "#ifndef FOO", "#if
// defined(FOO)", "#if !defined FOO" and "#if FOO". Any other expression is
// used as it is.
static void __ExpectedName(const char *keyword, const char *condition,
                           char *out, size_t size)
{
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s", condition);
    char *text = __Trim(buffer);

    if (strcmp(keyword, "if") != 0)
    {
        size_t length = 0;
        while (__IsIdentChar(text[length]))
            length++;
        if (length >= size)
            length = size - 1;
        memcpy(out, text, length);
        out[length] = '\0';
        return;
    }

    char *p = text;
    while (*p == '!' || isspace((unsigned char)*p))
        p++;

    if (strncmp(p, "defined", 7) == 0 && !__IsIdentChar(p[7]))
    {
        p += 7;
        while (isspace((unsigned char)*p))
            p++;
        int parenthesis = *p == '(';
        if (parenthesis)
            p++;
        while (isspace((unsigned char)*p))
            p++;
        char *start = p;
        while (__IsIdentChar(*p))
            p++;
        char *end = p;
        while (isspace((unsigned char)*p))
            p++;
        if (parenthesis && *p == ')')
            p++;
        while (isspace((unsigned char)*p))
            p++;
        if (end > start && *p == '\0')
        {
            size_t length = (size_t)(end - start);
            if (length >= size)
                length = size - 1;
            memcpy(out, start, length);
            out[length] = '\0';
            return;
        }
    }
    else
    {
        char *start = p;
        while (__IsIdentChar(*p))
            p++;
        if (p > start && *p == '\0')
        {
            snprintf(out, size, "%s", start);
            return;
        }
    }

    snprintf(out, size, "%s", text);
}

// Comment written after a #endif (original text of the line). 1 if there is
// one, "out" is then its content without the comment marks and blanks.
static int __EndifComment(const char *line, size_t length, char *out,
                          size_t size)
{
    char buffer[1024];
    if (length >= sizeof(buffer))
        length = sizeof(buffer) - 1;
    memcpy(buffer, line, length);
    buffer[length] = '\0';

    char *p = strchr(buffer, '#');
    if (p == NULL)
        return 0;
    p++;
    while (isspace((unsigned char)*p))
        p++;
    if (strncmp(p, "endif", 5) != 0)
        return 0;
    p += 5;
    while (isspace((unsigned char)*p))
        p++;

    char *content;
    if (p[0] == '/' && p[1] == '*')
    {
        p += 2;
        char *close = strstr(p, "*/");
        if (close != NULL)
            *close = '\0';
        content = p;
    }
    else if (p[0] == '/' && p[1] == '/')
        content = p + 2;
    else
        return 0;

    snprintf(out, size, "%s", __Trim(content));
    return 1;
}

// Include guard of a header : #ifndef NAME then #define NAME (NAME in upper
// case, made from the file name)
typedef struct
{
    const char *guard; // NULL when the file is not a header
    ProblemList *list;
    FixSet *fix; // NULL : nothing is modified
    size_t first_line; // line (from 0) of the #ifndef
    int open; // the first directive is the #ifndef of the guard
    int define_checked;
} Guard;

static void __GuardDefineMissing(Guard *g)
{
    g->define_checked = 1;
    __AddFixable(g->list, g->first_line + 1,
                 "the include guard #ifndef must be followed by #define %s",
                 g->guard);
    if (g->fix != NULL)
        __FixSet(g->fix, &g->fix->after[g->first_line], "#define %s", g->guard);
}

// First directive of a header : the #ifndef of the guard
static void __CheckGuardStart(Guard *g, const char *keyword,
                              const char *condition, size_t l)
{
    if (strcmp(keyword, "ifndef") != 0 && strcmp(keyword, "if") != 0)
    {
        __Add(g->list, l + 1,
              "a header must start with the include guard #ifndef %s",
              g->guard);
        return;
    }

    g->open = 1;
    g->first_line = l;

    char name[256];
    __ExpectedName(keyword, condition, name, sizeof(name));
    if (strcmp(name, g->guard) == 0)
        return;

    __AddFixable(g->list, l + 1, "include guard is \"%s\", expected \"%s\"",
                 name, g->guard);
    if (g->fix != NULL)
        __FixSet(g->fix, &g->fix->replace[l], "#ifndef %s", g->guard);
}

// Second directive of a header : the #define of the guard, same name
static void __CheckGuardDefine(Guard *g, const char *keyword,
                               const char *condition, size_t l)
{
    char name[256] = "";
    int shaped = 0;

    if (strcmp(keyword, "define") == 0)
    {
        size_t length = 0;
        while (__IsIdentChar(condition[length]))
            length++;
        if (length > 0 && length < sizeof(name) && condition[length] == '\0')
        {
            memcpy(name, condition, length);
            name[length] = '\0';
            shaped = 1;
        }
    }

    if (!shaped)
    {
        __GuardDefineMissing(g);
        return;
    }

    g->define_checked = 1;
    if (strcmp(name, g->guard) == 0)
        return;

    __AddFixable(g->list, l + 1,
                 "include guard #define is \"%s\", expected \"%s\"", name,
                 g->guard);
    if (g->fix != NULL)
        __FixSet(g->fix, &g->fix->replace[l], "#define %s", g->guard);
}

static void __CheckPreprocessor(const Source *src, const char *guard,
                                ProblemList *list, FixSet *fix)
{
    Guard g = { guard, list, fix, 0, 0, 0 };
    IfEntry *stack = NULL;
    size_t depth = 0;
    size_t capacity = 0;
    int first_seen = 0;

    for (size_t l = 0; l < src->line_count; l++)
    {
        if (src->preproc[l] != 1)
            continue;

        size_t start = src->line_start[l];
        size_t length = src->line_start[l + 1] - start;
        char line[1024];
        size_t copied = length >= sizeof(line) ? sizeof(line) - 1 : length;
        memcpy(line, src->comment_free + start, copied);
        line[copied] = '\0';

        char *content = __Trim(line);
        size_t content_length = strlen(content);
        if (content_length > 0 && content[content_length - 1] == '\\')
            content[content_length - 1] = '\0';
        content = __Trim(content);

        char *p = content + 1; // after the '#'
        while (isspace((unsigned char)*p))
            p++;
        char keyword[16];
        size_t keyword_length = 0;
        while (isalpha((unsigned char)*p)
               && keyword_length < sizeof(keyword) - 1)
            keyword[keyword_length++] = *p++;
        keyword[keyword_length] = '\0';
        char *condition = __Trim(p);

        // The include guard (#ifndef then #define) starts a header
        if (!first_seen)
        {
            first_seen = 1;
            if (guard != NULL)
                __CheckGuardStart(&g, keyword, condition, l);
        }
        else if (g.open && !g.define_checked)
            __CheckGuardDefine(&g, keyword, condition, l);

        if (strcmp(keyword, "if") == 0 || strcmp(keyword, "ifdef") == 0
            || strcmp(keyword, "ifndef") == 0)
        {
            if (depth == capacity)
            {
                capacity = capacity == 0 ? 16 : capacity * 2;
                IfEntry *grown = realloc(stack, capacity * sizeof(IfEntry));
                if (grown == NULL)
                    err(EXIT_FAILURE, "realloc");
                stack = grown;
            }
            __ExpectedName(keyword, condition, stack[depth].name,
                           sizeof(stack[depth].name));
            // A wrong guard name is fixed : its #endif follows the new name
            if (fix != NULL && g.open && l == g.first_line)
                snprintf(stack[depth].name, sizeof(stack[depth].name), "%s",
                         guard);
            stack[depth].line = l + 1;
            depth++;
        }
        else if (strcmp(keyword, "endif") == 0)
        {
            if (depth == 0)
            {
                __Add(list, l + 1, "#endif without matching #if");
                continue;
            }

            const IfEntry *entry = &stack[--depth];
            char found[256];
            int has_comment =
                __EndifComment(src->text + start, length, found, sizeof(found));
            if (has_comment && strcmp(found, entry->name) == 0)
                continue;

            if (!has_comment)
                __AddFixable(
                    list, l + 1,
                    "#endif must be followed by a comment with the name "
                    "\"%s\" (ex : #endif /* %s */) - #if of line %zu",
                    entry->name, entry->name, entry->line);
            else
                __AddFixable(
                    list, l + 1,
                    "#endif comment is \"%s\", expected exactly \"%s\" - #if "
                    "of line %zu",
                    found, entry->name, entry->line);

            if (fix != NULL)
                __FixEndif(fix, l, src->text + start, length, entry->name);
        }
    }

    for (size_t i = 0; i < depth; i++)
        __Add(list, stack[i].line, "#if %s is never closed by an #endif",
              stack[i].name);

    if (guard != NULL && !first_seen)
        __Add(list, 1, "a header must start with the include guard #ifndef %s",
              guard);
    if (g.open && !g.define_checked)
        __GuardDefineMissing(&g);

    free(stack);
}

// Functions

enum
{
    BRACE_TRANSPARENT, // namespace, extern "C" : functions are inside
    BRACE_FUNCTION,
    BRACE_OTHER // struct, enum, initializer, block inside a function...
};

typedef struct
{
    int kind;
    size_t open;
    size_t head_line; // line of the name of the function (from 1)
    char name[128];
} Brace;

// Number of arguments of "int a, char *b" (void and nothing : 0)
static int __CountArgs(const char *params, size_t length)
{
    size_t start = 0;
    size_t end = length;
    while (start < end && isspace((unsigned char)params[start]))
        start++;
    while (end > start && isspace((unsigned char)params[end - 1]))
        end--;

    if (start == end)
        return 0;
    if (end - start == 4 && strncmp(params + start, "void", 4) == 0)
        return 0;

    int count = 1;
    int depth = 0;
    for (size_t i = start; i < end; i++)
    {
        char c = params[i];
        if (c == '(' || c == '[' || c == '{' || c == '<')
            depth++;
        else if (c == ')' || c == ']' || c == '}'
                 || (c == '>' && !(i > start && params[i - 1] == '-')))
        {
            if (depth > 0)
                depth--;
        }
        else if (c == ',' && depth == 0)
            count++;
    }
    return count;
}

// What does the text before a '{' declare ? For a function, its name, the
// offset of its name in "header" and its number of arguments are given.
static int __Classify(const char *header, size_t length, size_t *name_offset,
                      char *name, size_t name_size, int *args)
{
    size_t start = 0;
    size_t end = length;
    while (start < end && isspace((unsigned char)header[start]))
        start++;
    while (end > start && isspace((unsigned char)header[end - 1]))
        end--;
    if (start == end)
        return BRACE_OTHER;

    if (__HasWord(header + start, end - start, "namespace"))
        return BRACE_TRANSPARENT;
    if (end - start >= 6 && strncmp(header + start, "extern", 6) == 0
        && header[end - 1] == '"')
        return BRACE_TRANSPARENT;

    // C++ qualifiers after the parameters
    static const char *QUALIFIERS[] = { "const", "noexcept", "override",
                                        "final", "volatile" };
    int changed = 1;
    while (changed)
    {
        changed = 0;
        for (size_t q = 0; q < sizeof(QUALIFIERS) / sizeof(QUALIFIERS[0]); q++)
        {
            size_t q_length = strlen(QUALIFIERS[q]);
            if (end - start > q_length
                && strncmp(header + end - q_length, QUALIFIERS[q], q_length)
                    == 0
                && !__IsIdentChar(header[end - q_length - 1]))
            {
                end -= q_length;
                while (end > start && isspace((unsigned char)header[end - 1]))
                    end--;
                changed = 1;
            }
        }
    }

    if (end == start || header[end - 1] != ')')
        return BRACE_OTHER;

    // Matching '('
    int depth = 0;
    size_t k = end;
    while (k > start)
    {
        k--;
        if (header[k] == ')')
            depth++;
        else if (header[k] == '(')
        {
            depth--;
            if (depth == 0)
                break;
        }
    }
    if (depth != 0 || header[k] != '(')
        return BRACE_OTHER;

    size_t name_end = k;
    while (name_end > start && isspace((unsigned char)header[name_end - 1]))
        name_end--;
    size_t name_start = name_end;
    while (name_start > start
           && (__IsIdentChar(header[name_start - 1])
               || header[name_start - 1] == ':'
               || header[name_start - 1] == '~'))
        name_start--;

    size_t name_length = name_end - name_start;
    if (name_length == 0)
        snprintf(name, name_size, "?");
    else
    {
        if (name_length >= name_size)
            name_length = name_size - 1;
        memcpy(name, header + name_start, name_length);
        name[name_length] = '\0';
    }

    *name_offset = name_start;
    *args = __CountArgs(header + k + 1, end - 1 - (k + 1));
    return BRACE_FUNCTION;
}

// Lines of code between two braces : the lines with only blanks, comments or
// braces are not counted (the code written next to a brace is)
static int __CountCodeLines(const Source *src, size_t open, size_t close)
{
    size_t first = __LineOf(src, open);
    size_t last = __LineOf(src, close);
    int count = 0;

    for (size_t l = first; l <= last; l++)
    {
        size_t start = src->line_start[l];
        size_t end = src->line_start[l + 1];
        if (l == first)
            start = open + 1;
        if (l == last)
            end = close;

        int has_code = l != first && l != last && src->preproc[l] != 0;
        for (size_t k = start; k < end && !has_code; k++)
        {
            char c = src->clean[k];
            if (!isspace((unsigned char)c) && c != '{' && c != '}')
                has_code = 1;
        }
        count += has_code;
    }

    return count;
}

// Rules of the C coding style that clang-format can't check (only for the .c
// and .h files). Nothing here is fixed automatically : renaming or rewriting
// code needs a decision of the author.

typedef struct
{
    int is_source; // .c file : the exported symbols are counted
    int is_header;
    ProblemList *list;
    size_t exported_functions;
    size_t exported_variables;
} CStyle;

static const char *TYPE_WORDS[] = { "void",     "char",     "short",
                                    "int",      "long",     "float",
                                    "double",   "signed",   "unsigned",
                                    "const",    "volatile", "_Bool",
                                    "struct",   "union",    "enum" };

static const char *INCLUDE_EXTENSIONS[] = { ".h", ".cxx" };

// [a-z][a-z0-9_]*
static int __IsLowerName(const char *name)
{
    if (!islower((unsigned char)name[0]))
        return 0;
    for (size_t i = 1; name[i] != '\0'; i++)
        if (!islower((unsigned char)name[i]) && !isdigit((unsigned char)name[i])
            && name[i] != '_')
            return 0;
    return 1;
}

static int __IsWord(const char *s, size_t length, const char *word)
{
    return strlen(word) == length && strncmp(s, word, length) == 0;
}

static int __IsTagWord(const char *s, size_t length)
{
    return __IsWord(s, length, "struct") || __IsWord(s, length, "union")
        || __IsWord(s, length, "enum");
}

// Type keyword, or a name like size_t / uint8_t
static int __IsTypeWord(const char *s, size_t length)
{
    for (size_t i = 0; i < sizeof(TYPE_WORDS) / sizeof(TYPE_WORDS[0]); i++)
        if (__IsWord(s, length, TYPE_WORDS[i]))
            return 1;
    return length > 2 && s[length - 2] == '_' && s[length - 1] == 't';
}

// Is "int", "char *", "struct foo *", "size_t"... the whole content ?
static int __IsTypeName(const char *content)
{
    int words = 0;
    int after_tag = 0; // the word follows struct / union / enum
    const char *p = content;
    while (*p != '\0')
    {
        if (isspace((unsigned char)*p) || (*p == '*' && words > 0))
        {
            p++;
            continue;
        }
        size_t length = 0;
        while (__IsIdentChar(p[length]))
            length++;
        if (length == 0 || !(after_tag || __IsTypeWord(p, length)))
            return 0;
        after_tag = __IsTagWord(p, length);
        words++;
        p += length;
    }
    return words > 0;
}

// Can the parentheses at [open, close] be a cast ? Not after a name (function
// call, sizeof, if...) and followed by something to convert.
static int __IsCastContext(const char *s, size_t length, size_t open,
                           size_t close)
{
    size_t k = open;
    while (k > 0 && isspace((unsigned char)s[k - 1]))
        k--;
    if (k > 0 && (s[k - 1] == ')' || s[k - 1] == ']'))
        return 0;
    if (k > 0 && __IsIdentChar(s[k - 1]))
    {
        size_t start = k;
        while (start > 0 && __IsIdentChar(s[start - 1]))
            start--;
        if (!__IsWord(s + start, k - start, "return"))
            return 0;
    }

    size_t m = close + 1;
    while (m < length && isspace((unsigned char)s[m]))
        m++;
    if (m >= length)
        return 0;
    return __IsIdentChar(s[m]) || strchr("(*&-+!~\"'", s[m]) != NULL;
}

// cast : the number of explicit casts is 0
static void __CheckCasts(const Source *src, ProblemList *list)
{
    const char *s = src->clean;
    for (size_t i = 0; i < src->length; i++)
    {
        if (s[i] != '(')
            continue;

        size_t close = i + 1;
        while (close < src->length && strchr(")(;{}", s[close]) == NULL)
            close++;
        if (close >= src->length || s[close] != ')' || close - i > 128)
            continue;

        char content[130];
        memcpy(content, s + i + 1, close - i - 1);
        content[close - i - 1] = '\0';
        if (__IsTypeName(content) && __IsCastContext(s, src->length, i, close))
            __Add(list, __LineOf(src, i) + 1,
                  "explicit cast (%s) is not allowed", __Trim(content));
    }
}

// cpp.digraphs : digraphs and trigraphs must not be used
static void __CheckDigraphs(const Source *src, ProblemList *list)
{
    const char *s = src->comment_free;
    size_t reported = 0;
    for (size_t i = 0; i + 1 < src->length; i++)
    {
        int digraph = (s[i] == '<'
                       && (s[i + 1] == '%'
                           || (s[i + 1] == ':'
                               && !(i + 2 < src->length && s[i + 2] == ':'))))
            || (s[i] == '%' && (s[i + 1] == '>' || s[i + 1] == ':'))
            || (s[i] == ':' && s[i + 1] == '>');
        int trigraph = s[i] == '?' && s[i + 1] == '?' && i + 2 < src->length
            && strchr("=/'()!<>-", s[i + 2]) != NULL;

        size_t line = __LineOf(src, i) + 1;
        if ((digraph || trigraph) && line != reported)
        {
            __Add(list, line, "digraphs and trigraphs must not be used");
            reported = line;
        }
    }
}

// name.case : file names
static void __CheckFileName(const char *name, ProblemList *list)
{
    char stem[256];
    const char *dot = strrchr(name, '.');
    size_t length = dot != NULL ? (size_t)(dot - name) : strlen(name);
    if (length >= sizeof(stem))
        length = sizeof(stem) - 1;
    memcpy(stem, name, length);
    stem[length] = '\0';

    if (!__IsLowerName(stem))
        __Add(list, 1, "file name \"%s\" must match [a-z][a-z0-9_]*", name);
}

// keyword.goto, stat.asm, gcc.builtins, name.struct (typedef), enum.comma

// name.struct : typedef is only for function pointers
static void __CheckTypedef(const Source *src, ProblemList *list, size_t start,
                           size_t from)
{
    const char *s = src->clean;
    int depth = 0;
    int pointer = 0;
    for (size_t k = from; k < src->length; k++)
    {
        if (s[k] == '{')
            depth++;
        else if (s[k] == '}' && depth > 0)
            depth--;
        else if (s[k] == ';' && depth == 0)
            break;
        else if (s[k] == '(' && depth == 0)
        {
            size_t n = k + 1;
            while (n < src->length && isspace((unsigned char)s[n]))
                n++;
            pointer |= n < src->length && s[n] == '*';
        }
    }

    if (!pointer)
        __Add(list, __LineOf(src, start) + 1,
              "typedef must only be used for function pointers (structures and "
              "unions must not be aliased)");
}

// enum.comma : the last enumerator ends with a comma
static void __CheckEnum(const Source *src, ProblemList *list, size_t from)
{
    const char *s = src->clean;
    size_t k = from;
    while (k < src->length && strchr("{;(),=", s[k]) == NULL)
        k++;
    if (k >= src->length || s[k] != '{')
        return;

    size_t close = k + 1;
    int depth = 1;
    while (close < src->length && depth > 0)
    {
        depth += s[close] == '{';
        depth -= s[close] == '}';
        close++;
    }
    if (depth != 0)
        return;
    close--; // the closing brace

    size_t last = close;
    while (last > k + 1 && isspace((unsigned char)s[last - 1]))
        last--;
    if (last > k + 1 && s[last - 1] != ',')
        __Add(list, __LineOf(src, close) + 1,
              "the last enumerator must be followed by a comma");
}

static void __CheckWord(const Source *src, ProblemList *list, size_t start,
                        size_t end)
{
    const char *word = src->clean + start;
    size_t length = end - start;
    size_t line = __LineOf(src, start) + 1;

    if (__IsWord(word, length, "goto"))
        __Add(list, line, "the goto statement must not be used");
    else if (__IsWord(word, length, "asm") || __IsWord(word, length, "__asm")
             || __IsWord(word, length, "__asm__"))
        __Add(list, line, "the asm declaration must not be used");
    else if (length > 10 && strncmp(word, "__builtin_", 10) == 0)
        __Add(list, line, "gcc builtin %.*s must not be used", (int)length,
              word);
    else if (__IsWord(word, length, "typedef"))
        __CheckTypedef(src, list, start, end);
    else if (__IsWord(word, length, "enum"))
        __CheckEnum(src, list, end);
}

static void __CheckTokens(const Source *src, ProblemList *list)
{
    const char *s = src->clean;
    size_t i = 0;
    while (i < src->length)
    {
        int starts_word = isalpha((unsigned char)s[i]) || s[i] == '_';
        if (!starts_word || (i > 0 && __IsIdentChar(s[i - 1])))
        {
            i++;
            continue;
        }

        size_t end = i;
        while (end < src->length && __IsIdentChar(s[end]))
            end++;
        __CheckWord(src, list, i, end);
        i = end;
    }
}

// Preprocessor : #include and #define

typedef struct
{
    int seen;
    int rank; // 0 : header of the file | 1 : system | 2 : local
    size_t line;
    char target[256];
} LastInclude;

// Keyword and arguments of the preprocessor directive of a line (copy of the
// text of the file in "buffer")
static char *__Directive(const Source *src, size_t l, char *buffer, size_t size,
                         char *keyword)
{
    size_t start = src->line_start[l];
    size_t length = src->line_start[l + 1] - start;
    if (length >= size)
        length = size - 1;
    memcpy(buffer, src->text + start, length);
    buffer[length] = '\0';

    keyword[0] = '\0';
    char *p = strchr(buffer, '#');
    if (p == NULL)
        return buffer;

    p++;
    while (isspace((unsigned char)*p))
        p++;
    size_t k = 0;
    while (isalpha((unsigned char)*p) && k < 15)
        keyword[k++] = *p++;
    keyword[k] = '\0';
    return __Trim(p);
}

// cpp.include.filetype and cpp.include.order : the header of the file, then the
// system headers, then the local ones, in blocks separated by an empty line and
// sorted alphabetically
static void __CheckInclude(const char *rest, size_t line, const char *own,
                           LastInclude *last, ProblemList *list)
{
    char close = rest[0] == '<' ? '>' : rest[0] == '"' ? '"' : '\0';
    const char *end = close == '\0' ? NULL : strchr(rest + 1, close);
    if (end == NULL || end == rest + 1 || (size_t)(end - rest) > 255)
        return;

    char target[256];
    memcpy(target, rest + 1, (size_t)(end - rest - 1));
    target[end - rest - 1] = '\0';

    size_t extensions = sizeof(INCLUDE_EXTENSIONS) / sizeof(char *);
    if (!__EndsWithOneOf(target, INCLUDE_EXTENSIONS, extensions))
        __Add(list, line, "included file \"%s\" must be a header (.h)", target);

    const char *base = strrchr(target, '/');
    base = base != NULL ? base + 1 : target;
    int rank = close == '>' ? 1 : (own != NULL && strcmp(base, own) == 0 ? 0 : 2);

    if (last->seen && rank < last->rank)
        __Add(list, line,
              "include order : header of the file, system headers, then local "
              "headers (\"%s\" is misplaced)",
              target);
    else if (last->seen && rank > last->rank && line == last->line + 1)
        __Add(list, line, "include blocks must be separated by an empty line");
    else if (last->seen && rank == last->rank && line == last->line + 1
             && strcasecmp(target, last->target) < 0)
        __Add(list, line,
              "includes must be sorted alphabetically (\"%s\" before \"%s\")",
              target, last->target);

    last->seen = 1;
    last->rank = rank;
    last->line = line;
    snprintf(last->target, sizeof(last->target), "%s", target);
}

// name.case.macro : macro arguments are in title case (Left, Right)
static void __CheckMacroArgument(char *argument, size_t line,
                                 ProblemList *list)
{
    char *dots = strstr(argument, "...");
    if (dots != NULL)
        *dots = '\0';
    argument = __Trim(argument);
    if (argument[0] == '\0')
        return;

    int has_lower = 0;
    for (size_t i = 0; argument[i] != '\0'; i++)
        has_lower |= islower((unsigned char)argument[i]) != 0;

    if (!isupper((unsigned char)argument[0])
        || (strlen(argument) > 1 && !has_lower))
        __Add(list, line,
              "macro argument \"%s\" must be in title case (ex : Left)",
              argument);
}

// name.case.macro : macro names are in upper case
static void __CheckMacro(char *rest, size_t line, ProblemList *list)
{
    size_t length = 0;
    int upper = 1;
    while (__IsIdentChar(rest[length]))
        upper &= islower((unsigned char)rest[length++]) == 0;
    if (length == 0)
        return;

    if (!upper)
        __Add(list, line, "macro name \"%.*s\" must be upper case", (int)length,
              rest);
    if (rest[length] != '(' || strchr(rest, ')') == NULL)
        return;

    char *params = rest + length + 1;
    *strchr(params, ')') = '\0';
    char *save = NULL;
    for (char *token = strtok_r(params, ",", &save); token != NULL;
         token = strtok_r(NULL, ",", &save))
        __CheckMacroArgument(token, line, list);
}

static void __CheckDirectives(const Source *src, const char *name,
                              ProblemList *list)
{
    char own[300] = "";
    const char *own_header = NULL;
    if (__EndsWithOneOf(name, (const char *[]){ ".c" }, 1))
    {
        snprintf(own, sizeof(own), "%.*s.h", (int)(strlen(name) - 2), name);
        own_header = own;
    }

    LastInclude last = { 0, 0, 0, "" };
    for (size_t l = 0; l < src->line_count; l++)
    {
        if (src->preproc[l] != 1)
            continue;

        char buffer[1024];
        char keyword[16];
        char *rest = __Directive(src, l, buffer, sizeof(buffer), keyword);
        if (strcmp(keyword, "include") == 0)
            __CheckInclude(rest, l + 1, own_header, &last, list);
        else if (strcmp(keyword, "define") == 0)
            __CheckMacro(rest, l + 1, list);
    }
}

// Declarations at the top level : global variables and prototypes

// Identifier ending at s[end] (exclusive) : its length, "start" is its offset
static size_t __IdentBefore(const char *s, size_t end, size_t *start)
{
    size_t k = end;
    while (k > 0 && __IsIdentChar(s[k - 1]))
        k--;
    *start = k;
    return end - k;
}

// Text of a declaration : the content of the braces is removed ("{}") and the
// blanks are collapsed. malloc'd.
static char *__Flatten(const char *text, size_t length)
{
    char *out = malloc(length + 1);
    if (out == NULL)
        err(EXIT_FAILURE, "malloc");

    size_t n = 0;
    int depth = 0;
    for (size_t i = 0; i < length; i++)
    {
        char c = text[i];
        if (c == '{' && depth++ == 0)
            out[n++] = c;
        else if (c == '}' && depth > 0 && --depth == 0)
            out[n++] = c;
        else if (depth == 0 && c != '{' && c != '}' && !isspace((unsigned char)c))
            out[n++] = c;
        else if (depth == 0 && isspace((unsigned char)c) && n > 0
                 && out[n - 1] != ' ')
            out[n++] = ' ';
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = '\0';
    return out;
}

// name.prefix.global, export.other : a global variable
static void __ReportGlobal(CStyle *ctx, const char *text, const char *name,
                           size_t line)
{
    if (strncmp(name, "g_", 2) != 0)
        __Add(ctx->list, line, "global variable \"%s\" must start with g_",
              name);
    else if (!__IsLowerName(name))
        __Add(ctx->list, line, "name \"%s\" must match [a-z][a-z0-9_]*", name);

    size_t length = strlen(text);
    int hidden = __HasWord(text, length, "static")
        || __HasWord(text, length, "extern");
    if (ctx->is_source && !hidden
        && ++ctx->exported_variables > TIDY_MAX_EXPORTED_VARIABLES)
        __Add(ctx->list, line,
              "exported global variable \"%s\" : a file can export at most %d "
              "non-function symbol",
              name, TIDY_MAX_EXPORTED_VARIABLES);
}

// Name of the first variable declared by "int *p[3] = ..." (text up to the "="
// excluded), NULL when the declaration declares no variable (struct foo, ...)
static int __VariableName(const char *text, size_t end, char *name, size_t size)
{
    for (size_t i = 0; i < end; i++)
        if (text[i] == ',')
            end = i;

    for (;;)
    {
        while (end > 0 && text[end - 1] == ' ')
            end--;
        if (end == 0 || text[end - 1] != ']')
            break;
        while (end > 0 && text[end - 1] != '[')
            end--;
        end -= end > 0;
    }

    size_t start;
    size_t length = __IdentBefore(text, end, &start);
    size_t before = start;
    while (before > 0 && text[before - 1] == ' ')
        before--;
    if (length == 0 || length >= size || before == 0)
        return 0;

    size_t tag_start;
    size_t tag = __IdentBefore(text, before, &tag_start);
    if (tag > 0 && __IsTagWord(text + tag_start, tag))
        return 0;

    memcpy(name, text + start, length);
    name[length] = '\0';
    return 1;
}

// A prototype : fun.proto.layout (exported prototypes are in headers) and
// fun.proto.void. A function pointer variable is a global variable.
static void __CheckPrototype(CStyle *ctx, const char *text, const char *paren,
                             size_t line)
{
    const char *after = paren + 1;
    while (*after == ' ')
        after++;

    char name[128];
    if (*after == '*')
    {
        while (*after == '*' || *after == ' ')
            after++;
        size_t length = 0;
        while (__IsIdentChar(after[length]) && length < sizeof(name) - 1)
        {
            name[length] = after[length];
            length++;
        }
        name[length] = '\0';
        if (length > 0)
            __ReportGlobal(ctx, text, name, line);
        return;
    }

    size_t start;
    size_t length = __IdentBefore(text, (size_t)(paren - text), &start);
    size_t before = start;
    while (before > 0 && text[before - 1] == ' ')
        before--;
    if (length == 0 || length >= sizeof(name) || before == 0)
        return; // macro call, not a prototype

    memcpy(name, text + start, length);
    name[length] = '\0';
    if (*after == ')')
        __Add(ctx->list, line,
              "prototype of \"%s\" must use (void) when it takes no argument",
              name);
    if (ctx->is_source && !__HasWord(text, strlen(text), "static"))
        __Add(ctx->list, line,
              "prototype of the exported function \"%s\" must be in a header, "
              "not in a source file",
              name);
}

static void __CheckTopStatement(CStyle *ctx, const Source *src, size_t start,
                                size_t end)
{
    size_t first = start;
    while (first < end && isspace((unsigned char)src->clean[first]))
        first++;
    if (first >= end)
        return;

    char *text = __Flatten(src->clean + first, end - first);
    const char *paren = strchr(text, '(');
    const char *equal = strchr(text, '=');
    size_t line = __LineOf(src, first) + 1;

    if (__HasWord(text, strlen(text), "typedef"))
    {
        free(text);
        return;
    }

    char name[128];
    if (paren != NULL && (equal == NULL || paren < equal))
        __CheckPrototype(ctx, text, paren, line);
    else if (__VariableName(text, equal != NULL ? (size_t)(equal - text)
                                                : strlen(text),
                            name, sizeof(name)))
        __ReportGlobal(ctx, text, name, line);

    free(text);
}

// fun.proto.void, name.case, export.fun : head of a function definition
static void __CheckFunctionHead(CStyle *ctx, const char *head, size_t length,
                                size_t name_offset, const Brace *brace)
{
    if (strchr(brace->name, ':') == NULL && strcmp(brace->name, "?") != 0
        && !__IsLowerName(brace->name))
        __Add(ctx->list, brace->head_line,
              "function name \"%s\" must match [a-z][a-z0-9_]*", brace->name);

    const char *paren = memchr(head + name_offset, '(', length - name_offset);
    if (paren != NULL)
    {
        const char *after = paren + 1;
        while (isspace((unsigned char)*after))
            after++;
        if (*after == ')')
            __Add(ctx->list, brace->head_line,
                  "function \"%s\" must use (void) when it takes no argument",
                  brace->name);
    }

    if (ctx->is_source && !__HasWord(head, length, "static")
        && ++ctx->exported_functions == TIDY_MAX_EXPORTED_FUNCTIONS + 1)
        __Add(ctx->list, brace->head_line,
              "more than %d exported functions in this file (\"%s\" is the "
              "%dth) : move some of them or make them static",
              TIDY_MAX_EXPORTED_FUNCTIONS, brace->name,
              TIDY_MAX_EXPORTED_FUNCTIONS + 1);
}

static void __CheckCStyle(const Source *src, const char *name, CStyle *ctx)
{
    __CheckFileName(name, ctx->list);
    __CheckDirectives(src, name, ctx->list);
    __CheckDigraphs(src, ctx->list);
    __CheckTokens(src, ctx->list);
    __CheckCasts(src, ctx->list);
}

static void __CheckFunctions(const Source *src, ProblemList *list,
                            CStyle *ctx)
{
    const char *s = src->clean;
    Brace *stack = NULL;
    size_t depth = 0;
    size_t capacity = 0;
    size_t opaque = 0; // braces on the stack that are not "transparent"
    size_t statement_start = 0;
    size_t decl_start = 0; // start of the declaration at the top level

    for (size_t i = 0; i < src->length; i++)
    {
        if (s[i] == '{')
        {
            Brace brace = { BRACE_OTHER, i, 0, "" };
            if (opaque == 0)
            {
                size_t name_offset = 0;
                int args = 0;
                brace.kind = __Classify(s + statement_start,
                                        i - statement_start, &name_offset,
                                        brace.name, sizeof(brace.name), &args);
                if (brace.kind == BRACE_FUNCTION)
                {
                    brace.head_line =
                        __LineOf(src, statement_start + name_offset) + 1;
                    if (args > TIDY_MAX_ARGS)
                        __Add(list, brace.head_line,
                              "function \"%s\" has %d arguments (max %d)",
                              brace.name, args, TIDY_MAX_ARGS);
                    if (ctx != NULL)
                        __CheckFunctionHead(ctx, s + statement_start,
                                            i - statement_start, name_offset,
                                            &brace);
                }
            }

            if (brace.kind != BRACE_TRANSPARENT)
                opaque++;
            if (depth == capacity)
            {
                capacity = capacity == 0 ? 16 : capacity * 2;
                Brace *grown = realloc(stack, capacity * sizeof(Brace));
                if (grown == NULL)
                    err(EXIT_FAILURE, "realloc");
                stack = grown;
            }
            stack[depth++] = brace;
            statement_start = i + 1;
            if (brace.kind == BRACE_TRANSPARENT)
                decl_start = i + 1;
        }
        else if (s[i] == '}')
        {
            if (depth > 0)
            {
                Brace brace = stack[--depth];
                if (brace.kind != BRACE_TRANSPARENT)
                    opaque--;
                if (brace.kind != BRACE_OTHER)
                    decl_start = i + 1;

                if (brace.kind == BRACE_FUNCTION)
                {
                    int lines = __CountCodeLines(src, brace.open, i);
                    if (lines > TIDY_MAX_LINES)
                        __Add(
                            list, brace.head_line,
                            "function \"%s\" has %d lines (max %d), from line "
                            "%zu to line %zu",
                            brace.name, lines, TIDY_MAX_LINES,
                            __LineOf(src, brace.open) + 1,
                            __LineOf(src, i) + 1);
                }
            }
            statement_start = i + 1;
        }
        else if (s[i] == ';' && opaque == 0)
        {
            if (ctx != NULL)
                __CheckTopStatement(ctx, src, decl_start, i);
            statement_start = i + 1;
            decl_start = i + 1;
        }
    }

    free(stack);
}

// One file, one folder

// Path shown to the user : without the leading "./"
static const char *__DisplayPath(const char *path)
{
    while (path[0] == '.' && path[1] == '/')
    {
        path += 2;
        while (*path == '/')
            path++;
    }
    return path;
}

// Every problem found, one line each (<file>:<line>: <problem>) : they are all
// shown at the end in the same Info panel
static char *remaining = NULL;
static size_t remaining_size = 0;

static void __KeepProblem(const char *path, const Problem *problem)
{
    int length = snprintf(NULL, 0, "%s:%zu: %s\n", path, problem->line,
                          problem->message);
    if (length < 0)
        return;

    char *grown = realloc(remaining, remaining_size + (size_t)length + 1);
    if (grown == NULL)
        err(EXIT_FAILURE, "realloc");
    remaining = grown;
    snprintf(remaining + remaining_size, (size_t)length + 1, "%s:%zu: %s\n",
             path, problem->line, problem->message);
    remaining_size += (size_t)length;
}

static int __HeaderGuard(const char *name, char *guard, size_t size)
{
    int is_header = __EndsWithOneOf(name, HEADER_EXTENSIONS,
                                    sizeof(HEADER_EXTENSIONS)
                                        / sizeof(HEADER_EXTENSIONS[0]));
    if (is_header)
        __GuardName(name, guard, size);
    return is_header;
}

// Write the file with its fixes. 0 : done | -1 : impossible to write
static int __WriteFixed(const char *path, const Source *src, const FixSet *fix)
{
    size_t capacity = src->length + 16;
    for (size_t l = 0; l < src->line_count; l++)
    {
        if (fix->replace[l] != NULL)
            capacity += strlen(fix->replace[l]) + 4;
        if (fix->after[l] != NULL)
            capacity += strlen(fix->after[l]) + 4;
    }

    char *out = malloc(capacity);
    if (out == NULL)
        err(EXIT_FAILURE, "malloc");
    size_t n = 0;

    for (size_t l = 0; l < src->line_count; l++)
    {
        const char *line = src->text + src->line_start[l];
        size_t length = src->line_start[l + 1] - src->line_start[l];
        size_t content = length;
        if (content > 0 && line[content - 1] == '\n')
            content--;
        if (content > 0 && line[content - 1] == '\r')
            content--;
        size_t eol_length = length - content;

        if (fix->replace[l] != NULL)
        {
            size_t size = strlen(fix->replace[l]);
            memcpy(out + n, fix->replace[l], size);
            n += size;
        }
        else
        {
            memcpy(out + n, line, content);
            n += content;
        }

        // The last line may have no line ending
        const char *eol = eol_length > 0 ? line + content : "\n";
        size_t eol_size = eol_length > 0 ? eol_length : 1;
        if (eol_length > 0 || fix->after[l] != NULL
            || (l + 1 == src->line_count && fix->add_newline))
        {
            memcpy(out + n, eol, eol_size);
            n += eol_size;
        }

        if (fix->after[l] != NULL)
        {
            size_t size = strlen(fix->after[l]);
            memcpy(out + n, fix->after[l], size);
            n += size;
            memcpy(out + n, eol, eol_size);
            n += eol_size;
        }
    }

    FILE *file = fopen(path, "wb");
    int result = -1;
    if (file != NULL)
    {
        result = fwrite(out, 1, n, file) == n ? 0 : -1;
        if (fclose(file) != 0)
            result = -1;
    }
    free(out);
    return result;
}

// Fix what can be fixed in a file : the include guard (#ifndef and #define),
// the comments of the #endif and the newline at the end of the file. The
// functions with too many lines or arguments are never modified.
// Returns the number of problems fixed, -1 if the file can't be written.
static int __FixFile(const char *path, const char *name, const Source *src)
{
    if (src->length == 0)
        return 0;

    FixSet fix;
    __FixInit(&fix, src->line_count);
    if (src->text[src->length - 1] != '\n')
    {
        fix.add_newline = 1;
        fix.count++;
    }

    char guard[256];
    int is_header = __HeaderGuard(name, guard, sizeof(guard));
    ProblemList ignored = { NULL, 0, 0 };
    __CheckPreprocessor(src, is_header ? guard : NULL, &ignored, &fix);
    __FreeProblems(&ignored);

    int result = (int)fix.count;
    if (fix.count > 0 && __WriteFixed(path, src, &fix) != 0)
        result = -1;
    __FixFree(&fix);
    return result;
}

static void __AnalyzeFile(const char *path, const char *name)
{
    Source src;
    int loaded = __LoadSource(path, &src);
    if (loaded < 0)
    {
        UiWarning("Impossible to read %s, not checked.", __DisplayPath(path));
        return;
    }
    if (loaded > 0)
        return;

    if (fix_mode)
    {
        int fixed = __FixFile(path, name, &src);
        if (fixed < 0)
            UiWarning("Impossible to modify %s.", __DisplayPath(path));
        else if (fixed > 0)
        {
            printf("%s: %d problem(s) fixed\n", __DisplayPath(path), fixed);
            files_fixed++;
            total_fixed += (size_t)fixed;

            // The problems left are the ones of the new content
            __FreeSource(&src);
            if (__LoadSource(path, &src) != 0)
            {
                UiWarning("Impossible to read %s again, not checked.",
                          __DisplayPath(path));
                return;
            }
        }
    }

    files_checked++;

    ProblemList list = { NULL, 0, 0 };
    if (src.length > 0)
    {
        if (src.text[src.length - 1] != '\n')
            __AddFixable(&list, src.line_count,
                         "the file does not end with a newline");

        char guard[256];
        int is_header = __HeaderGuard(name, guard, sizeof(guard));

        __CheckPreprocessor(&src, is_header ? guard : NULL, &list, NULL);

        // The rules of the C coding style only apply to .c and .h files
        CStyle style = { __EndsWithOneOf(name, C_SOURCE_EXTENSIONS, 1),
                         is_header && __EndsWithOneOf(name, C_HEADER_EXTENSIONS, 1),
                         &list, 0, 0 };
        int is_c = style.is_source || style.is_header;
        __CheckFunctions(&src, &list, is_c ? &style : NULL);
        if (is_c)
            __CheckCStyle(&src, name, &style);
    }

    if (list.count > 0)
    {
        qsort(list.items, list.count, sizeof(Problem), __CompareProblems);
        for (size_t i = 0; i < list.count; i++)
            __KeepProblem(__DisplayPath(path), &list.items[i]);

        files_with_problems++;
        total_problems += list.count;
        for (size_t i = 0; i < list.count; i++)
            total_fixable += list.items[i].fixable;
    }

    __FreeProblems(&list);
    __FreeSource(&src);
}

static int __IsIgnoredFolder(const char *name)
{
    for (size_t i = 0; i < sizeof(IGNORED_FOLDERS) / sizeof(IGNORED_FOLDERS[0]);
         i++)
        if (strcmp(name, IGNORED_FOLDERS[i]) == 0)
            return 1;
    return 0;
}

static int __Visit(const char *path, const struct stat *info, int type,
                   struct FTW *position)
{
    (void)info;
    const char *name = path + position->base;

    if (type == FTW_D || type == FTW_DNR)
        return position->level > 0 && __IsIgnoredFolder(name) ? FTW_SKIP_SUBTREE
                                                              : 0;

    if (type == FTW_F
        && __EndsWithOneOf(name, SOURCE_EXTENSIONS,
                           sizeof(SOURCE_EXTENSIONS)
                               / sizeof(SOURCE_EXTENSIONS[0])))
        __AnalyzeFile(path, name);

    return 0;
}

static int __Run(const char *folder, int fix)
{
    files_checked = 0;
    files_with_problems = 0;
    total_problems = 0;
    total_fixable = 0;
    files_fixed = 0;
    total_fixed = 0;
    fix_mode = fix;
    free(remaining);
    remaining = NULL;
    remaining_size = 0;

    if (nftw(folder, __Visit, 32, FTW_PHYS | FTW_ACTIONRETVAL) != 0)
    {
        UiError("Impossible to browse %s.", folder);
        return -1;
    }
    fflush(stdout);

    if (files_checked == 0)
    {
        UiInfo("No C / C++ file to check in %s.", folder);
        return 0;
    }

    if (total_fixed > 0)
        UiSuccess("%zu problem(s) fixed in %zu file(s).", total_fixed,
                  files_fixed);

    // All the problems, in the same Info panel
    if (remaining != NULL)
    {
        if (remaining_size > 0 && remaining[remaining_size - 1] == '\n')
            remaining[--remaining_size] = '\0';
        UiPanel(stdout, UI_INFO, fix ? "Problems left" : "Problems found", "%s",
                remaining);
        fflush(stdout);
    }

    if (total_problems == 0)
    {
        if (fix)
            UiSuccess("%zu file(s) checked, no problem left.", files_checked);
        else
            UiSuccess("%zu file(s) checked, no problem found.", files_checked);
        return 0;
    }

    if (fix)
    {
        UiWarning("%zu problem(s) left in %zu file(s) (%zu file(s) checked), "
                  "Jarvis can't fix them.",
                  total_problems, files_with_problems, files_checked);
    }
    else
        UiWarning("%zu problem(s) found in %zu file(s) (%zu file(s) checked).",
                  total_problems, files_with_problems, files_checked);
    return (int)total_problems;
}

size_t TidyLastFixable(void)
{
    return total_fixable;
}

size_t TidyLastUnfixable(void)
{
    return total_problems - total_fixable;
}

int TidyTree(const char *folder)
{
    return __Run(folder, 0);
}

int TidyTreeFix(const char *folder)
{
    return __Run(folder, 1);
}
