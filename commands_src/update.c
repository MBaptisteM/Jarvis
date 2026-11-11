#include "ui.h"

#include <sys/stat.h>
#include <sys/wait.h>

#define SETUP_URL                                                              \
    "https://raw.githubusercontent.com/MBaptisteM/Jarvis/main/setup.sh"
#define USAGE "Usage: jarvis update"

// Run by bash once this program is gone : the installer replaces the binaries
// of Jarvis, which is impossible while one of them is running ("Text file
// busy"), so this program must not be alive during the installation.
#define RUN_SCRIPT "bash \"$1\"; status=$?; rm -f \"$1\"; exit \"$status\""

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

int main(int argc, char *argv[])
{
    if (argc == 2
        && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        puts(USAGE);
        return EXIT_SUCCESS;
    }
    if (argc > 1)
        errx(EXIT_FAILURE, "ERROR Unknown argument %s\n" USAGE, argv[1]);

    UiInfo("Updating Jarvis (reinstalling the latest version)...");

    // Download first, run after : a failed download changes nothing
    char *path = __CreateTempFile();
    if (!__Download(path) || !__IsNotEmpty(path))
    {
        remove(path);
        errx(EXIT_FAILURE, "ERROR Impossible to download the installer.\n"
                           "Check your connection, then try again.");
    }

    execlp("bash", "bash", "-c", RUN_SCRIPT, "jarvis-update", path, NULL);

    remove(path);
    err(EXIT_FAILURE, "ERROR Impossible to run the installer");
}
