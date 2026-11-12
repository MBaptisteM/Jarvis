#include "main.h"

#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define COMMAND_UPDATE "update"

// File whose modification date is the date of the last update check (it is
// written by the update command), and delay between two checks.
#define LAST_CHECK_FILE ".jarvis_last_check"
#define CHECK_DELAY (24 * 60 * 60)

static char *ResolveShortcut(char *command)
{
    static char *shortcuts[][2] = {
        { "a", "auth" },    { "c", "clone" }, { "pl", "pull" },
        { "ps", "push" },   { "f", "find" },  { "g", "go" },
        { "h", "help" },    { "t", "tidy" },  { "cu", "custom" },
        { "fm", "format" }, {"u", "update"},
    };

    for (size_t i = 0; i < sizeof(shortcuts) / sizeof(shortcuts[0]); i++)
        if (strcasecmp(command, shortcuts[i][0]) == 0)
            return shortcuts[i][1];

    return command;
}

// Has the last update check been done more than 24 hours ago (or never) ?
// Set JARVIS_NO_AUTO_UPDATE to disable the automatic check.
static int AutoUpdateDue(void)
{
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0' || getenv("JARVIS_NO_AUTO_UPDATE"))
        return 0;

    char stamp_path[4096];
    snprintf(stamp_path, sizeof(stamp_path), "%s/%s", home, LAST_CHECK_FILE);

    struct stat info;
    if (stat(stamp_path, &info) != 0)
        return 1;

    double elapsed = difftime(time(NULL), info.st_mtime);
    return elapsed >= CHECK_DELAY || elapsed < 0;
}

// Hand over to "update --auto <jarvis> <arguments of the command>"
static void RunAutoUpdate(int argc, char *argv[])
{
    char update_path[256];
    snprintf(update_path, sizeof(update_path), "%s/%s", GetCommandPath(),
             COMMAND_UPDATE);
    if (access(update_path, X_OK) != 0)
        return;

    char **update_argv = calloc((size_t)argc + 3, sizeof(char *));
    if (update_argv == NULL)
        return;

    update_argv[0] = COMMAND_UPDATE;
    update_argv[1] = "--auto";
    for (int i = 0; i < argc; i++)
        update_argv[i + 2] = argv[i];

    execv(update_path, update_argv);
    free(update_argv);
}

// Main function that choose which command should be executed
int main(int argc, char *argv[])
{
    // No command entered case : call help if it is possible
    if (argc < 2)
    {
        char help_path[256];
        snprintf(help_path, sizeof(help_path), "%s/%s", GetCommandPath(),
                 COMMAND_HELP);

        if (access(help_path, X_OK) != 0)
            errx(EXIT_FAILURE,
                 "ERROR you need to select a command and command %s not found",
                 COMMAND_HELP);

        printf("ERROR you need to select a command\n");

        char *default_args[] = { COMMAND_HELP, NULL };
        execvp(help_path, default_args);
    }

    // A command has been entered case : call the command if it is possible
    else
    {
        // "c" -> "clone", "g" -> "go", ...
        argv[1] = ResolveShortcut(argv[1]);

        // At most once a day, look for a newer version before the command
        if (strcasecmp(argv[1], COMMAND_UPDATE) != 0 && AutoUpdateDue())
            RunAutoUpdate(argc, argv);

        // "jarvis counter" alone -> "jarvis help counter"
        if (argc == 2 && strcasecmp(argv[1], "counter") == 0)
        {
            char help_path[256];
            snprintf(help_path, sizeof(help_path), "%s/%s", GetCommandPath(),
                     COMMAND_HELP);

            if (access(help_path, X_OK) != 0)
                errx(EXIT_FAILURE,
                     "ERROR you need to select a command and command %s "
                     "not found",
                     COMMAND_HELP);

            char *help_args[] = { COMMAND_HELP, argv[1], NULL };
            execvp(help_path, help_args);
        }

        if (argc > 2)
        {
            if (strcasecmp(argv[2], "--help") == 0
                || strcasecmp(argv[2], "-h") == 0
                || strcasecmp(argv[2], "help") == 0)
            {
                char help_path[256];
                snprintf(help_path, sizeof(help_path), "%s/%s",
                         GetCommandPath(), COMMAND_HELP);

                if (access(help_path, X_OK) != 0)
                    errx(EXIT_FAILURE,
                         "ERROR you need to select a command and command %s "
                         "not found",
                         COMMAND_HELP);

                execvp(help_path, argv);
                return 0;
            }
        }
        char command_path[256];

        snprintf(command_path, sizeof(command_path), "%s/%s", GetCommandPath(),
                 argv[1]);

        if (access(command_path, X_OK) != 0)
            errx(EXIT_FAILURE,
                 "ERROR command %s not found\nHint: run 'jarvis help' to "
                 "see available commands",
                 argv[1]);

        execv(command_path, &argv[1]);
    }

    // Error case, impossible to call
    errx(EXIT_FAILURE, "ERROR unknown error, command impossible to execute");
}
