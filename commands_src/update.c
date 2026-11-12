#include "ui.h"

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#define SETUP_URL                                                              \
    "https://raw.githubusercontent.com/MBaptisteM/Jarvis/main/setup.sh"
// Last commit of the repository (the "sha" media type gives only its hash)
#define LATEST_URL                                                             \
    "https://api.github.com/repos/MBaptisteM/Jarvis/commits/main"
#define USAGE "Usage: jarvis update"

// Files in the home folder. LAST_CHECK_FILE must be the same as in main.c.
#define LAST_CHECK_FILE ".jarvis_last_check"
#define VERSION_FILE ".jarvis_version"

// Run by bash once this program is gone : the installer replaces the binaries
// of Jarvis, which is impossible while one of them is running ("Text file
// busy"), so this program must not be alive during the installation.
// Arguments : $1 installer, $2 installed commit (may be empty), $3 file where
// it is saved, then (automatic update) the command to run again. The version
// is saved and the command is run again only if the installation succeeded.
// The normal output of the installer is hidden (its errors are still shown) :
// an update message is displayed instead of its install message.
#define RUN_SCRIPT                                                             \
    "bash \"$1\" >/dev/null; status=$?; rm -f \"$1\"; "                        \
    "if [ \"$status\" -eq 0 ]; then "                                          \
    "if [ -n \"$2\" ]; then printf 'Jarvis updated (version %.7s).\\n' \"$2\"; "\
    "else printf 'Jarvis updated.\\n'; fi; fi; "                               \
    "if [ \"$status\" -eq 0 ] && [ -n \"$2\" ] && [ -n \"$3\" ]; then "        \
    "printf '%s\\n' \"$2\" > \"$3\"; fi; "                                     \
    "if [ \"$status\" -eq 0 ] && [ \"$#\" -gt 3 ]; then shift 3; "             \
    "exec \"$@\"; fi; "                                                        \
    "exit \"$status\""

// Is <name> an executable program of the PATH ?
static int __InPath(const char *name)
{
    const char *path = getenv("PATH");
    if (path == NULL)
        return 0;

    char *copy = strdup(path);
    if (copy == NULL)
        err(EXIT_FAILURE, "strdup");

    int found = 0;
    char *save = NULL;
    for (char *dir = strtok_r(copy, ":", &save); !found && dir != NULL;
         dir = strtok_r(NULL, ":", &save))
    {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        found = access(full, X_OK) == 0;
    }

    free(copy);
    return found;
}

// Create an empty temporary file (malloc'd path, the caller must free it)
static char *__CreateTempFile(void)
{
    const char *dir = getenv("TMPDIR");
    if (dir == NULL || dir[0] == '\0')
        dir = "/tmp";

    size_t size = strlen(dir) + sizeof("/jarvis-setup-XXXXXX");
    char *path = malloc(size);
    if (path == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(path, size, "%s/jarvis-setup-XXXXXX", dir);

    int fd = mkstemp(path);
    if (fd < 0)
        err(EXIT_FAILURE, "ERROR Impossible to create a temporary file");
    close(fd);

    return path;
}

// Download the installer in <path> with curl (else wget). 1 on success.
static int __Download(const char *path)
{
    int use_curl = __InPath("curl");
    if (!use_curl && !__InPath("wget"))
        errx(EXIT_FAILURE,
             "ERROR 'curl' or 'wget' is required to update Jarvis.");

    pid_t child = fork();
    if (child < 0)
        err(EXIT_FAILURE, "fork");
    if (child == 0)
    {
        if (use_curl)
            execlp("curl", "curl", "-fsSL", "-o", path, SETUP_URL, NULL);
        else
            execlp("wget", "wget", "-q", "-O", path, SETUP_URL, NULL);
        _exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(child, &status, 0) < 0)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int __IsNotEmpty(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && info.st_size > 0;
}

// Path of a file of the home folder (malloc'd), NULL if there is no home
static char *__HomeFile(const char *name)
{
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
        return NULL;

    size_t size = strlen(home) + strlen(name) + 2;
    char *path = malloc(size);
    if (path == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(path, size, "%s/%s", home, name);
    return path;
}

// The check is counted as done now (even if it fails afterwards, so that an
// offline computer does not wait for the network at each command)
static void __WriteCheckStamp(void)
{
    char *path = __HomeFile(LAST_CHECK_FILE);
    if (path == NULL)
        return;

    FILE *file = fopen(path, "w");
    if (file != NULL)
    {
        fprintf(file, "%ld\n", (long)time(NULL));
        fclose(file);
    }
    free(path);
}

// Hash of the commit installed by the last update (NULL if unknown)
static char *__ReadInstalledVersion(void)
{
    char *path = __HomeFile(VERSION_FILE);
    if (path == NULL)
        return NULL;

    FILE *file = fopen(path, "r");
    free(path);
    if (file == NULL)
        return NULL;

    char line[128];
    char *version = NULL;
    if (fgets(line, sizeof(line), file) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0')
            version = strdup(line);
    }
    fclose(file);
    return version;
}

// Hash of the last commit pushed on the repository (malloc'd). NULL if it
// cannot be known (no curl/wget, no network, timeout...) : nothing is said.
static char *__FetchLatestVersion(void)
{
    const char *command;
    if (__InPath("curl"))
        command = "curl -fsSL --connect-timeout 3 --max-time 8 "
                  "-H 'Accept: application/vnd.github.sha' " LATEST_URL
                  " 2>/dev/null";
    else if (__InPath("wget"))
        command = "wget -q -T 8 -O - "
                  "--header='Accept: application/vnd.github.sha' " LATEST_URL
                  " 2>/dev/null";
    else
        return NULL;

    FILE *pipe = popen(command, "r");
    if (pipe == NULL)
        return NULL;

    char line[128];
    char *version = NULL;
    if (fgets(line, sizeof(line), pipe) != NULL)
    {
        size_t length = strcspn(line, "\r\n");
        line[length] = '\0';

        int valid = length == 40;
        for (size_t i = 0; valid && i < length; i++)
            valid = isxdigit((unsigned char)line[i]);
        if (valid)
            version = strdup(line);
    }
    pclose(pipe);
    return version;
}

// Automatic mode only : give back control to the command that was asked.
// JARVIS_NO_AUTO_UPDATE avoids a new check (and any loop) in the meantime.
static void __RunCommand(char **command)
{
    setenv("JARVIS_NO_AUTO_UPDATE", "1", 1);
    execvp(command[0], command);
    err(EXIT_FAILURE, "ERROR Impossible to run the command %s", command[0]);
}

int main(int argc, char *argv[])
{
    if (argc == 2
        && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        puts(USAGE);
        return EXIT_SUCCESS;
    }

    // "update --auto <jarvis> <command and arguments>", started by jarvis
    // when the last check is older than 24 hours : the command to run again
    char **command = NULL;
    if (argc >= 3 && strcmp(argv[1], "--auto") == 0)
        command = &argv[2];
    else if (argc > 1)
        errx(EXIT_FAILURE, "ERROR Unknown argument %s\n" USAGE, argv[1]);

    __WriteCheckStamp();
    char *latest = __FetchLatestVersion();

    if (command != NULL)
    {
        // Nothing to do if the version is known to be the installed one, or
        // cannot be checked
        char *installed = __ReadInstalledVersion();
        int up_to_date =
            latest == NULL || (installed != NULL && strcmp(installed, latest) == 0);
        free(installed);

        if (up_to_date)
        {
            free(latest);
            __RunCommand(command);
        }

        UiInfo("A newer version of Jarvis is available, updating...");
    }
    else
        UiInfo("Updating Jarvis (reinstalling the latest version)...");

    // Download first, run after : a failed download changes nothing
    char *path = __CreateTempFile();
    if (!__Download(path) || !__IsNotEmpty(path))
    {
        remove(path);
        if (command != NULL)
        {
            // The update is not worth blocking the command
            UiWarning("Impossible to download the update, it will be tried "
                      "again later (or run 'jarvis update').");
            __RunCommand(command);
        }
        errx(EXIT_FAILURE, "ERROR Impossible to download the installer.\n"
                           "Check your connection, then try again.");
    }

    // bash -c RUN_SCRIPT jarvis-update <installer> <version> <file> [command]
    size_t command_size = 0;
    while (command != NULL && command[command_size] != NULL)
        command_size++;

    char *version_file = __HomeFile(VERSION_FILE);
    char **bash_argv = calloc(command_size + 8, sizeof(char *));
    if (bash_argv == NULL)
        err(EXIT_FAILURE, "calloc");

    size_t n = 0;
    bash_argv[n++] = "bash";
    bash_argv[n++] = "-c";
    bash_argv[n++] = RUN_SCRIPT;
    bash_argv[n++] = "jarvis-update";
    bash_argv[n++] = path;
    bash_argv[n++] = latest != NULL ? latest : "";
    bash_argv[n++] = version_file != NULL ? version_file : "";
    for (size_t i = 0; i < command_size; i++)
        bash_argv[n++] = command[i];

    if (command != NULL)
        setenv("JARVIS_NO_AUTO_UPDATE", "1", 1);
    execvp("bash", bash_argv);

    remove(path);
    err(EXIT_FAILURE, "ERROR Impossible to run the installer");
}
