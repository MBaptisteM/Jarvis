#include "help.h"

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
          "Asks how you want to connect, saves the connection cookies for the "
          "next uses, then finds or clones the root repository. With the "
          "browser (default) a window opens, close it once you are connected. "
          "With the console you type your username and password, and Jarvis "
          "gets the cookies by itself (the password is never saved). The "
          "first call may install many packages.",
      .rows_title = "Choice",
      .rows = { { "Browser", "Sign in on the forge in a window (default)." },
                { "Console", "Type your username and password here." },
                { NULL, NULL } },
      .examples = { "jarvis auth", NULL } },

    { .name = "clone",
      .shortcut = "c",
      .group = "Repositories",
      .usage =
          "jarvis clone [all | current/c/cu/cur/curr | <remote-repository>]",
      .summary = "Clone a repository and build its tree structure.",
      .details =
          "Clones the repository, places it in the tree structure, creates "
          "the structure of the TP for EPITA repositories, then pushes the "
          "modifications. The first call may install many packages.",
      .rows_title = "Arguments",
      .rows = { { "all",
                  "Find or clone the root repository (same as no "
                  "argument)." },
                { "current (c/cu/cur/curr)",
                  "Pick one of your forge projects and clone it." },
                { "<remote>", "Clone the specified remote repository." },
                { NULL, NULL } },
      .examples = { "jarvis clone", "jarvis clone current",
                    "jarvis clone firstname.name@git.forge.epita.fr:p/...",
                    NULL } },

    { .name = "fill",
      .shortcut = "fl",
      .group = "Repositories",
      .usage = "jarvis fill [old | all | current/now | <project>]",
      .summary = "Clone many repositories of the forge at once.",
      .details =
          "Reads your projects on the forge and clones every matching "
          "repository like clone does, without any question. The repositories "
          "already cloned are skipped, and the content of a repository that "
          "is not empty is never changed. A repository whose course can't be "
          "deduced goes in Others/<project>/ (ex : "
          "Others/epita-prepa-computer-science/). The root repository is "
          "pushed once at the end.",
      .rows_title = "Arguments",
      .rows = { { "old (anc/ancient/past)",
                  "Every finished repository (default)." },
                { "all", "Every repository, finished or not." },
                { "current (now)",
                  "Only the repositories that are not "
                  "finished." },
                { "<project>",
                  "Every repository listed on the page of this "
                  "project." },
                { NULL, NULL } },
      .examples = { "jarvis fill", "jarvis fill all", "jarvis fill now",
                    "jarvis fill epita-prepa-computer-science", NULL } },

    { .name = "pull",
      .shortcut = "pl",
      .group = "Repositories",
      .usage = "jarvis pull [all | current/c/cu/cur/curr | <repository>]",
      .summary = "Pull and update repositories.",
      .details =
          "Updates the whole tree, a specific repository, or the current one. "
          "Submodules are initialized and pulled recursively.",
      .rows_title = "Arguments",
      .rows = { { "all",
                  "Pull the root repository and every repository of "
                  "the tree (default)." },
                { "current (c/cu/cur/curr)", "Pull the current repository." },
                { "<repository>",
                  "Pull this repository (if already "
                  "cloned)." },
                { NULL, NULL } },
      .examples = { "jarvis pull", "jarvis pull current",
                    "jarvis pull my_project", NULL } },

    { .name = "push",
      .shortcut = "ps",
      .group = "Repositories",
      .usage =
          "jarvis push [root | current/c/cu/cur/curr | <path>] [-n <name>] "
          "[-t <tag>] [-p <path>]",
      .summary = "Commit and push a repository and the root repository.",
      .details =
          "Adds, commits and pushes a repository, then updates and pushes the "
          "root repository. The commit name is generated automatically unless "
          "one is given (or it is the name of the tag, if any). If the "
          "automatic format is enabled in 'jarvis custom' (off by default), "
          "the repository is formatted with jarvis format first ; the root "
          "repository alone is never formatted. The tidy mode set in 'jarvis "
          "custom' decides what is done with jarvis tidy before the push : "
          "nothing (default), warn (on a problem, you choose to push anyway or "
          "to cancel the push) or fix (the problems that can be fixed are "
          "fixed in the files, the others are reported like with warn). A "
          "problem never stops the "
          "other steps : the root repository is pushed even if the "
          "repository push failed, \"pushed\" is displayed only when commits "
          "were really sent, and the exit code is not zero if something "
          "failed. A nested repository without any commit is hidden while "
          "adding. The --option=<value> form also works (ex : --name=\"Fix\").",
      .rows_title = "Arguments",
      .rows = { { "root", "Push only the root repository." },
                { "current (c/cu/cur/curr)", "Push the current repository." },
                { "<path>",
                  "Push the repository at this path (default: the "
                  "current folder)." },
                { "-n, --name <name>",
                  "Name of the commit (generated when "
                  "missing). --name works too." },
                { "-t, --tag <name>",
                  "Create and push a tag named <name>-<number> "
                  "(ex : v1-0)." },
                { "-p, --path <path>",
                  "Same as <path> (root and current "
                  "work too)." },
                { NULL, NULL } },
      .examples = { "jarvis push", "jarvis push current", "jarvis push root",
                    "jarvis push -n \"Fix bug\" -t v1 -p ~/TPs/my_repo",
                    "jarvis push --name=\"Fix bug\" --path current", NULL } },

    { .name = "find",
      .shortcut = "f",
      .group = "Navigation",
      .usage = "jarvis find [root | current/c/cu/cur/curr | <repository> | "
               "<keyword>]",
      .summary = "Find the path of specific repositories.",
      .details = "Prints the local path of the repositories matching the "
                 "request, without touching them.",
      .rows_title = "Arguments",
      .rows = { { "root", "Find the root repository (default)." },
                { "current (c/cu/cur/curr)", "Find the current repository." },
                { "<repo url>",
                  "Find the local clone of this remote "
                  "repository." },
                { "<keyword>",
                  "Find the repositories whose title contains "
                  "this word." },
                { NULL, NULL } },
      .examples = { "jarvis find root", "jarvis find current",
                    "jarvis find firstname.name@git.forge.epita.fr:p/...",
                    "jarvis find \"trees\"", NULL } },

    { .name = "go",
      .shortcut = "g",
      .group = "Navigation",
      .usage = "jarvis go [root | current/c/cu/cur/curr | <repository> | "
               "<keyword>]",
      .summary = "Go to a repository (like find, but moves you there).",
      .details =
          "Finds a repository like find, then changes your current shell's "
          "directory. This also updates OLDPWD, so \"cd -\" returns to the "
          "previous directory. If several folders match, a choice is proposed.",
      .rows_title = "Arguments",
      .rows = { { "root", "Go to the root repository (default)." },
                { "current (c/cu/cur/curr)", "Go to the current repository." },
                { "<repo url>",
                  "Go to the local clone of this remote "
                  "repository." },
                { "<keyword>",
                  "Go to a repository whose title contains this "
                  "word." },
                { NULL, NULL } },
      .examples = { "jarvis go root", "jarvis go current",
                    "jarvis go \"trees\"", NULL } },

    { .name = "custom",
      .shortcut = NULL,
      .group = "Organization",
      .usage = "jarvis custom",
      .summary =
          "Customize the tree structure and the format (layers, prefixes, "
          "Others, Format, Tidy).",
      .details =
          "Interactive menu, chosen with the arrows and enter (prefixes, names "
          "and extensions are typed). The settings are stored in the "
          ".jarvis_config file of the root repository (created with the "
          "default values if missing), so once pushed and pulled every Jarvis "
          "of a project shares them. Quitting applies the changes to the "
          "repositories already cloned (they are moved); Save also pushes the "
          "root repository. The Format and Tidy settings are saved at once, Save "
          "pushes them.",
      .rows_title = "Menu",
      .rows = { { "Layers",
                  "Enable or disable Year, Semester and Bimester "
                  "folders (default: Semester and Bimester)." },
                { "Prefixes",
                  "Text written before the number (default: Y, S, "
                  "B)." },
                { "Others",
                  "Rename the folder used when the course can't "
                  "be deduced (default: Others)." },
                { "Format",
                  "Enable or disable the automatic jarvis format "
                  "before each push (default: disabled), add or "
                  "remove the extensions it formats (case, spaces "
                  "and the leading dot don't matter : \"C++, "
                  "Json\" gives .c++ and .json), show or restore "
                  "the default list." },
                { "Tidy",
                  "What jarvis does with jarvis tidy before each push : "
                  "nothing (default), warn (if problems are found, you "
                  "choose to push anyway or to cancel) or fix (names, "
                  "comments and newlines are fixed in the files, the other "
                  "problems are reported)." },
                { "Reset", "Go back to the default organization." },
                { "Save",
                  "Apply the changes to the tree and push the root "
                  "repository." },
                { "Quit", "Apply the changes to the tree (no push)." },
                { NULL, NULL } },
      .examples = { "jarvis custom", NULL } },

    { .name = "format",
      .shortcut = "fm",
      .group = "Organization",
      .usage = "jarvis format [<path> | -p <path>]",
      .summary = "Format the code of the current folder with clang-format.",
      .details =
          "Runs clang-format -i on every C, C++, Java, JavaScript, TypeScript, "
          "C#, JSON, Objective-C, Protobuf, Verilog (and other languages "
          "supported by clang-format) file of the current folder (or of the "
          "given folder) and all its sub-folders. clang-format must be "
          "installed. The .clang-format file "
          "closest to each file is used. .git and node_modules folders are "
          "skipped. A file that can't be "
          "formatted is reported but does not stop the others. The "
          "extensions, and an automatic run before each jarvis push, are set "
          "in 'jarvis custom' > Format.",
      .rows_title = "Arguments",
      .rows = { { "<path>", "Folder to format (default: the current folder)." },
                { "-p, --path <path>", "Same as <path>." },
                { NULL, NULL } },
      .examples = { "jarvis format", "jarvis format ../EPITA-TPs", "j fm",
                    NULL } },

    { .name = "tidy",
      .shortcut = "t",
      .group = "Organization",
      .usage = "jarvis tidy [-f | --fix] [<path> | -p <path>]",
      .summary = "Check the style rules of the code of the current folder.",
      .details =
          "Scans every C / C++ file (.c .h .cc .cpp .cxx .hh .hpp .hxx) of the "
          "current folder (or of the given folder) and all its sub-folders, "
          "and lists every problem as <file>:<line>: <problem> in an Info panel. "
          "Nothing is "
          "modified without your answer : when problems that can be fixed are "
          "found (include guard names, #endif comments, final newline), Jarvis "
          "asks whether to fix them, and warns you about the others, that "
          "stay to fix by hand. With --fix they are fixed without asking "
          "(nothing is asked outside a terminal). A function with "
          "too many lines or arguments is never modified. The exit code is "
          "not zero when a problem is left. .git "
          "and node_modules folders are skipped. Lines of a function : blank "
          "lines, comments and lines made of braces only are not counted. "
          "What is done automatically before each jarvis push (nothing, warn "
          "or fix ; nothing by default) is set in 'jarvis custom' > Tidy.",
      .rows_title = "Checks",
      .rows = { { "Arguments", "At most 4 arguments per function." },
                { "Length", "At most 30 lines per function." },
                { "Newline", "Every file ends with a newline." },
                { "#endif",
                  "Followed by a comment with exactly the name of its #if "
                  "(ex : #endif /* DEBUG */)." },
                { "Header guard",
                  "A header starts with #ifndef <FILE NAME>_H in upper case "
                  "(test.h : TEST_H), followed by #define with the same "
                  "name." },
                { "<path>", "Folder to check (default: the current folder)." },
                { "-f, --fix",
                  "Fix what can be fixed without asking, report the rest." },
                { "-p, --path <path>", "Same as <path>." },
                { NULL, NULL } },
      .examples = { "jarvis tidy", "jarvis tidy --fix",
                    "jarvis tidy ../EPITA-TPs", "j t", NULL } },

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

    { .name = "counter",
      .shortcut = NULL,
      .group = "Others",
      .usage = "jarvis help counter",
      .summary = "How the Jarvis user counter works.",
      .details =
          "Jarvis counts its users with the stars of its GitHub repository "
          "(MBaptisteM/Jarvis). The first time you use Jarvis in a terminal "
          "while connected to GitHub (gh), it asks whether you accept to be "
          "counted. If you say yes, a star is added to the repository with "
          "your GitHub account. The question is asked only once, your answer "
          "is saved and Jarvis works the same whatever you answer. The "
          "counter is not anonymous : the list of the people who starred a "
          "repository is public on GitHub. This way was choosen because it "
          " is the simpliest to show publicly how many people use Jarvis "
          "without external server. To leave the count, remove the star "
          "on the GitHub page of the repository.",
      .rows_title = "Arguments",
      .rows = { { NULL, NULL } },
      .examples = { "jarvis help counter", NULL } },

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


// Panels : ╭─ Title ────╮ / │ name   text │ / ╰────╯

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

// Help pages

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

    fprintf(out,
            "\n %sType%s jarvis help COMMAND %sfor more information on a "
            "command.%s\n",
            K_BORDER, K_RESET, K_BORDER, K_RESET);
    fprintf(out,
            " %s\"j\" works instead of \"jarvis\" (ex : j c, j g, j pl)."
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