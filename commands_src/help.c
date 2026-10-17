#include "help.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_ROWS 8
#define MAX_EXAMPLES 5

typedef struct
{
    const char *label;
    const char *text;
} Row;

typedef struct
{
    const char *name;
    const char *shortcut; // NULL when there is none (destructive commands)
    const char *group;
    const char *usage;
    const char *summary;
    const char *details;
    const char *rows_title;
    Row rows[MAX_ROWS + 1];
    const char *examples[MAX_EXAMPLES + 1];
} Command;

// Order of the panels in the general help
static const char *GROUPS[] = { "Setup",        "Repositories", "Navigation",
                                "Organization", "Maintenance",  "Others" };

static const Command COMMANDS[] = {
    { .name = "auth",
      .shortcut = "a",
      .group = "Setup",
      .usage = "jarvis auth",
      .summary = "Authenticate to your EPITA account.",
      .details =
          "Opens a connection window and saves the connection cookies for the "
          "next uses, then finds or clones the root repository. Close the "
          "window once you are connected. The first call may install many "
          "packages.",
      .rows_title = "Arguments",
      .rows = { { NULL, NULL } },
      .examples = { "jarvis auth", NULL } },

    { .name = "clone",
      .shortcut = "c",
      .group = "Repositories",
      .usage = "jarvis clone [all | current | <remote-repository>]",
      .summary = "Clone a repository and build its tree structure.",
      .details =
          "Clones the repository, places it in the tree structure, creates "
          "the structure of the TP for EPITA repositories, then pushes the "
          "modifications. The first call may install many packages.",
      .rows_title = "Arguments",
      .rows = { { "all", "Find or clone the root repository (same as no "
                         "argument)." },
                { "current", "Pick one of your forge projects and clone it." },
                { "<remote>", "Clone the specified remote repository." },
                { NULL, NULL } },
      .examples = { "jarvis clone", "jarvis clone current",
                    "jarvis clone firstname.name@git.forge.epita.fr:p/...",
                    NULL } },

    { .name = "pull",
      .shortcut = "pl",
      .group = "Repositories",
      .usage = "jarvis pull [all | current | <repository>]",
      .summary = "Pull and update repositories.",
      .details =
          "Updates the whole tree, a specific repository, or the current one. "
          "Submodules are initialized and pulled recursively.",
      .rows_title = "Arguments",
      .rows = { { "all", "Pull the root repository and every repository of "
                         "the tree (default)." },
                { "current", "Pull the current repository." },
                { "<repository>", "Pull this repository (if already "
                                  "cloned)." },
                { NULL, NULL } },
      .examples = { "jarvis pull", "jarvis pull current",
                    "jarvis pull my_project", NULL } },

    { .name = "push",
      .shortcut = "ps",
      .group = "Repositories",
      .usage = "jarvis push [root | current | <path>]",
      .summary = "Commit and push a repository and the root repository.",
      .details =
          "Adds, commits and pushes a repository, and updates the root "
          "repository at the same time. The commit name is generated "
          "automatically unless one is given.",
      .rows_title = "Arguments",
      .rows = { { "root", "Push only the root repository." },
                { "current", "Push the current repository." },
                { "<path>", "Push the repository at this path (default: the "
                            "current folder)." },
                { NULL, NULL } },
      .examples = { "jarvis push", "jarvis push current", "jarvis push root",
                    NULL } },

    { .name = "find",
      .shortcut = "f",
      .group = "Navigation",
      .usage = "jarvis find [root | current | <repository> | <keyword>]",
      .summary = "Find the path of specific repositories.",
      .details = "Prints the local path of the repositories matching the "
                 "request, without touching them.",
      .rows_title = "Arguments",
      .rows = { { "root", "Find the root repository (default)." },
                { "current", "Find the current repository." },
                { "<repo url>", "Find the local clone of this remote "
                                "repository." },
                { "<keyword>", "Find the repositories whose title contains "
                               "this word." },
                { NULL, NULL } },
      .examples = { "jarvis find root", "jarvis find current",
                    "jarvis find firstname.name@git.forge.epita.fr:p/...",
                    "jarvis find \"trees\"", NULL } },

    { .name = "go",
      .shortcut = "g",
      .group = "Navigation",
      .usage = "jarvis go [root | current | <repository> | <keyword>]",
      .summary = "Go to a repository (like find, but moves you there).",
      .details =
          "Finds a repository like find, then changes your current shell's "
          "directory. This also updates OLDPWD, so \"cd -\" returns to the "
          "previous directory. If several folders match, a choice is proposed.",
      .rows_title = "Arguments",
      .rows = { { "root", "Go to the root repository (default)." },
                { "current", "Go to the current repository." },
                { "<repo url>", "Go to the local clone of this remote "
                                "repository." },
                { "<keyword>", "Go to a repository whose title contains this "
                               "word." },
                { NULL, NULL } },
      .examples = { "jarvis go root", "jarvis go current",
                    "jarvis go \"trees\"", NULL } },

    { .name = "custom",
      .shortcut = NULL,
      .group = "Organization",
      .usage = "jarvis custom",
      .summary = "Customize the tree structure (layers, prefixes).",
      .details =
          "Interactive menu, everything is chosen with the arrows and enter. "
          "Quitting applies the changes to the repositories already cloned "
          "(they are moved); Save also pushes the root repository.",
      .rows_title = "Menu",
      .rows = { { "Layers", "Enable or disable Year, Semester and Bimester "
                            "folders (default: Semester and Bimester)." },
                { "Prefixes", "Text written before the number (default: Y, S, "
                              "B)." },
                { "Reset", "Go back to the default organization." },
                { "Save", "Apply the changes to the tree and push the root "
                          "repository." },
                { "Quit", "Apply the changes to the tree (no push)." },
                { NULL, NULL } },
      .examples = { "jarvis custom", NULL } },

    { .name = "erase",
      .shortcut = NULL,
      .group = "Maintenance",
      .usage = "jarvis erase",
      .summary = "Back up, clean, and push the root repository.",
      .details =
          "Pushes a backup, deletes all the content of the root repository "
          "except .git, then pushes the cleaned repository. A confirmation is "
          "asked first (answer y or Y). The backup uses force-with-lease and "
          "replaces remote-only commits. Ignored files are not backed up. To "
          "restore: git restore --source=<backup-hash> --staged --worktree . "
          "then jarvis push root.",
      .rows_title = "Arguments",
      .rows = { { NULL, NULL } },
      .examples = { "jarvis erase", NULL } },

    { .name = "uninstall",
      .shortcut = NULL,
      .group = "Maintenance",
      .usage = "jarvis uninstall",
      .summary = "Uninstall Jarvis.",
      .details = "Removes Jarvis from the system after a confirmation (write "
                 "y to confirm).",
      .rows_title = "Arguments",
      .rows = { { NULL, NULL } },
      .examples = { "jarvis uninstall", NULL } },

    { .name = "help",
      .shortcut = "h",
      .group = "Others",
      .usage = "jarvis help [command]",
      .summary = "Show the commands, or the help of one command.",
      .details = "<command> --help works too. \"j\" works instead of "
                 "\"jarvis\".",
      .rows_title = "Arguments",
      .rows = { { "<command>", "Name or shortcut of a command." },
                { NULL, NULL } },
      .examples = { "jarvis help", "jarvis help clone", "j h c", NULL } },
};

#define COMMAND_COUNT (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

static FILE *out;
static const char *K_RESET = "";
static const char *K_BOLD = "";
static const char *K_BORDER = "";
static const char *K_TITLE = "";
static const char *K_NAME = "";
static const char *K_EXAMPLE = "";
static const char *K_ERROR = "";

static void __InitStyle(FILE *stream)
{
    out = stream;

    int colors = isatty(fileno(stream)) && getenv("NO_COLOR") == NULL;
    K_RESET = colors ? "\033[0m" : "";
    K_BOLD = colors ? "\033[1m" : "";
    K_BORDER = colors ? "\033[90m" : "";
    K_TITLE = colors ? "\033[1m" : "";
    K_NAME = colors ? "\033[1;36m" : "";
    K_EXAMPLE = colors ? "\033[32m" : "";
    K_ERROR = colors ? "\033[31m" : "";
}

static int __Width(void)
{
    int width = 0;

    struct winsize ws;
    if (isatty(fileno(out)) && ioctl(fileno(out), TIOCGWINSZ, &ws) == 0)
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

static const Command *__Find(const char *name)
{
    for (size_t i = 0; i < COMMAND_COUNT; i++)
    {
        if (strcasecmp(name, COMMANDS[i].name) == 0
            || (COMMANDS[i].shortcut != NULL
                && strcasecmp(name, COMMANDS[i].shortcut) == 0))
            return &COMMANDS[i];
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Panels : ╭─ Title ────╮ / │ name   text │ / ╰────╯
// ---------------------------------------------------------------------------

static void __PanelTop(const char *title, int width, const char *title_color)
{
    int dashes = width - (int)strlen(title) - 5;
    if (dashes < 1)
        dashes = 1;

    fprintf(out, "%s╭─ %s%s%s%s ", K_BORDER, K_RESET, title_color, title,
            K_RESET);
    fprintf(out, "%s", K_BORDER);
    for (int i = 0; i < dashes; i++)
        fprintf(out, "─");
    fprintf(out, "╮%s\n", K_RESET);
}

static void __PanelBottom(int width)
{
    fprintf(out, "%s╰", K_BORDER);
    for (int i = 0; i < width - 2; i++)
        fprintf(out, "─");
    fprintf(out, "╯%s\n", K_RESET);
}

// One row, the text is wrapped on several lines when it is too long. The name
// column is empty when name_width is 0.
static void __PanelRow(int width, int name_width, const char *name,
                       const char *text, const char *text_color)
{
    int text_width = name_width > 0 ? width - 5 - name_width : width - 4;
    if (text_width < 10)
        text_width = 10;

    const char *p = text;
    int first = 1;
    do
    {
        int length = (int)strlen(p);
        int n = length <= text_width ? length : text_width;
        if (n < length)
        {
            int k = n;
            while (k > 0 && p[k] != ' ')
                k--;
            if (k > 0)
                n = k;
        }

        fprintf(out, "%s│%s ", K_BORDER, K_RESET);
        if (name_width > 0)
        {
            fprintf(out, "%s%-*s%s ", K_NAME, name_width,
                    first && name != NULL ? name : "", K_RESET);
        }
        fprintf(out, "%s%.*s%s%*s", text_color, n, p, K_RESET, text_width - n,
                "");
        fprintf(out, " %s│%s\n", K_BORDER, K_RESET);

        p += n;
        while (*p == ' ')
            p++;
        first = 0;
    } while (*p != '\0');
}

// A paragraph written outside of the panels, wrapped and indented by one space
static void __Paragraph(const char *text, int width)
{
    int line_width = width - 2;
    const char *p = text;

    while (*p != '\0')
    {
        int length = (int)strlen(p);
        int n = length <= line_width ? length : line_width;
        if (n < length)
        {
            int k = n;
            while (k > 0 && p[k] != ' ')
                k--;
            if (k > 0)
                n = k;
        }

        fprintf(out, " %.*s\n", n, p);
        p += n;
        while (*p == ' ')
            p++;
    }
}

static void __PrintUsage(const char *usage)
{
    fprintf(out, "\n %sUsage:%s %s%s%s\n\n", K_BOLD, K_RESET, K_NAME, usage,
            K_RESET);
}

// ---------------------------------------------------------------------------
// Help pages
// ---------------------------------------------------------------------------

static void __PrintGeneralHelp(void)
{
    int width = __Width();

    __PrintUsage("jarvis COMMAND [ARGS]...");

    // Same name column in every panel
    int global_width = 0;
    for (size_t i = 0; i < COMMAND_COUNT; i++)
    {
        int length = (int)strlen(COMMANDS[i].name)
            + (COMMANDS[i].shortcut != NULL
                   ? 2 + (int)strlen(COMMANDS[i].shortcut)
                   : 0);
        if (length > global_width)
            global_width = length;
    }
    global_width += 2;

    for (size_t g = 0; g < sizeof(GROUPS) / sizeof(GROUPS[0]); g++)
    {
        char labels[COMMAND_COUNT][32];
        const Command *rows[COMMAND_COUNT];
        size_t count = 0;
        int name_width = global_width;

        for (size_t i = 0; i < COMMAND_COUNT; i++)
        {
            if (strcmp(COMMANDS[i].group, GROUPS[g]) != 0)
                continue;

            if (COMMANDS[i].shortcut != NULL)
                snprintf(labels[count], sizeof(labels[count]), "%s, %s",
                         COMMANDS[i].name, COMMANDS[i].shortcut);
            else
                snprintf(labels[count], sizeof(labels[count]), "%s",
                         COMMANDS[i].name);

            rows[count++] = &COMMANDS[i];
        }
        if (count == 0)
            continue;

        __PanelTop(GROUPS[g], width, K_TITLE);
        for (size_t i = 0; i < count; i++)
            __PanelRow(width, name_width, labels[i], rows[i]->summary, "");
        __PanelBottom(width);
    }

    fprintf(out, "\n %sType%s jarvis help COMMAND %sfor more information on a "
                 "command.%s\n",
            K_BORDER, K_RESET, K_BORDER, K_RESET);
    fprintf(out, " %s\"j\" works instead of \"jarvis\" (ex : j c, j g, j pl)."
                 "%s\n\n",
            K_BORDER, K_RESET);
}

static void __PrintCommandHelp(const Command *command)
{
    int width = __Width();

    __PrintUsage(command->usage);
    __Paragraph(command->summary, width);
    fprintf(out, "\n");
    __Paragraph(command->details, width);
    if (command->shortcut != NULL)
        fprintf(out, "\n %sShortcut:%s %s%s%s\n", K_BOLD, K_RESET, K_NAME,
                command->shortcut, K_RESET);
    fprintf(out, "\n");

    if (command->rows[0].label != NULL)
    {
        int name_width = 0;
        for (int i = 0; command->rows[i].label != NULL; i++)
            if ((int)strlen(command->rows[i].label) > name_width)
                name_width = (int)strlen(command->rows[i].label);
        name_width += 2;

        __PanelTop(command->rows_title, width, K_TITLE);
        for (int i = 0; command->rows[i].label != NULL; i++)
            __PanelRow(width, name_width, command->rows[i].label,
                       command->rows[i].text, "");
        __PanelBottom(width);
    }

    __PanelTop("Examples", width, K_TITLE);
    for (int i = 0; command->examples[i] != NULL; i++)
    {
        char line[512];
        snprintf(line, sizeof(line), "$ %s", command->examples[i]);
        __PanelRow(width, 0, NULL, line, K_EXAMPLE);
    }
    __PanelBottom(width);
    fprintf(out, "\n");
}

static void __PrintUnknown(const char *name)
{
    UiError("No such command '%s'.\n\nUsage: jarvis COMMAND [ARGS]...\n"
            "Try 'jarvis help' for help.",
            name);
}

int main(int argc, char *argv[])
{
    __InitStyle(stdout);

    if (argc < 2)
    {
        __PrintGeneralHelp();
        return EXIT_SUCCESS;
    }

    const Command *command = __Find(argv[1]);
    if (command == NULL)
    {
        __PrintUnknown(argv[1]);
        return 2;
    }

    __PrintCommandHelp(command);
    return EXIT_SUCCESS;
}