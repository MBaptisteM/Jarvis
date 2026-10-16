#include <get_jarvis_paths.h>

// Get the commands_path depending on how the tool has been installed
const char *GetCommandPath()
{
    char *home_path = NULL;
    if (__GetRootPath(&home_path) != EXIT_SUCCESS
        && __GetLocalPath(&home_path) != EXIT_SUCCESS)
    {
        if (access("./commands", F_OK) == 0)
            return "./commands";
        else
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the path to jarvis commands, try to "
                 "install again.");
    }

    static char commands_path[512];
    snprintf(commands_path, sizeof(commands_path), "%s/cmd", home_path);

    free(home_path);

    return commands_path;
}

// Get the subject_path depending on how the tool has been installed
const char *GetSubjectFolderPath()
{
    char *home_path = NULL;
    if (GetDotJarvisPath(&home_path))
    {
        if (access("./subject", F_OK) == 0)
            return "./subject";
        else
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the path to jarvis commands, try to "
                 "install again.");
    }

    static char subject_path[512];
    snprintf(subject_path, sizeof(subject_path), "%s/subject", home_path);

    free(home_path);

    return subject_path;
}

// Get path of the info file
char *GetInfoPath()
{
    char *home_path = NULL;
    if (GetDotJarvisPath(&home_path))
        errx(EXIT_FAILURE, "ERROR Impoossible to find home path.");

    char *info_file_full_path = malloc(512);
    if (info_file_full_path == NULL)
    {
        free(home_path);
        err(EXIT_FAILURE, "malloc");
    }
    snprintf(info_file_full_path, 512, "%s/%s", home_path,
             INFORMATION_FILE_NAME);
    free(home_path);

    return info_file_full_path;
}

#ifndef REPO_NAME
#define REPO_NAME "EPITA-TPs"
#endif

char *FindFileBFS(const char *file_to_find);

// Is there a .git inside this folder ?
static int __HasGitFolder(const char *path)
{
    size_t size = strlen(path) + sizeof("/.git");
    char *git_path = malloc(size);
    if (git_path == NULL)
        return 0;

    snprintf(git_path, size, "%s/.git", path);
    int result = access(git_path, F_OK) == 0;
    free(git_path);
    return result;
}

// Search the root repository (REPO_NAME) in the arborescence, starting from
// $HOME, and register it in the info file. Returns its path with a trailing
// '/', or NULL if it can't be found anywhere.
static char *__SearchRootRepo(void)
{
    char *parent = FindFileBFS(REPO_NAME);
    if (parent == NULL)
        return NULL;

    size_t size = strlen(parent) + strlen(REPO_NAME) + 3;
    char *root = malloc(size);
    if (root == NULL)
    {
        free(parent);
        err(EXIT_FAILURE, "malloc");
    }
    snprintf(root, size, "%s/%s/", parent, REPO_NAME);
    free(parent);

    if (!__HasGitFolder(root))
    {
        free(root);
        return NULL;
    }

    WriteInfo(NAME_PATH_INFO_FILE, root);
    if (strcmp(NAME_PATH_INFO_FILE, "main_path") != 0)
        WriteInfo("main_path", root);

    return root;
}

// Same contract as ReadInfo("main_path", value) (0 : found | 1 : not found),
// but when the saved root repository is missing or no longer valid, it is
// searched again in the arborescence and the info file is updated.
int ReadRootPath(char **value)
{
    *value = NULL;

    char *stored = NULL;
    if (ReadInfo("main_path", &stored) == EXIT_SUCCESS && stored != NULL)
    {
        int valid = __HasGitFolder(stored);

        // The saved path may be the parent of the root repository
        if (!valid)
        {
            size_t size = strlen(stored) + strlen(REPO_NAME) + 2;
            char *nested = malloc(size);
            if (nested == NULL)
                err(EXIT_FAILURE, "malloc");
            snprintf(nested, size, "%s/%s", stored, REPO_NAME);
            valid = __HasGitFolder(nested);
            free(nested);
        }

        if (valid)
        {
            *value = stored;
            return EXIT_SUCCESS;
        }
    }

    // Not saved, or the saved path is gone : search it again
    char *found = __SearchRootRepo();
    if (found != NULL)
    {
        free(stored);
        *value = found;
        return EXIT_SUCCESS;
    }

    // Not a git repository yet (e.g. folder just created) but it exists
    if (stored != NULL && access(stored, F_OK) == 0)
    {
        *value = stored;
        return EXIT_SUCCESS;
    }

    free(stored);
    return EXIT_FAILURE;
}

// Get path of the main folder TPs (root folder) | NULL if not found
char *GetTPsPath()
{
    char *TPs_path = NULL;

    if (!ReadInfo(NAME_PATH_INFO_FILE, &TPs_path))
    {
        if (access(TPs_path, F_OK) == 0)
            return TPs_path;
        free(TPs_path);
    }

    // Saved path missing or gone : search the root repository again
    TPs_path = __SearchRootRepo();
    if (TPs_path != NULL)
        return TPs_path;

    char *marked_path = FindMarkedFileBFS();
    if (marked_path == NULL)
        return NULL;

    size_t marked_size = strlen(marked_path) + 2;
    TPs_path = realloc(marked_path, marked_size);
    if (TPs_path == NULL)
    {
        free(marked_path);
        err(EXIT_FAILURE, "realloc");
    }
    strcat(TPs_path, "/");
    WriteInfo(NAME_PATH_INFO_FILE, TPs_path);

    return TPs_path;
}

// Get the path to the system-wide Jarvis installation
int __GetRootPath(char **path)
{
    *path = NULL;
    if (access("/usr/local/lib/jarvis", F_OK) == 0)
    {
        *path = malloc(512);
        if (!*path)
            return EXIT_FAILURE;

        snprintf(*path, 512, "%s", "/usr/local/lib/jarvis");
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

// Get the path to the user-local Jarvis installation
int __GetLocalPath(char **path)
{
    *path = NULL;
    char *home = getenv("HOME");
    if (home)
    {
        *path = malloc(512);

        if (!*path)
            return EXIT_FAILURE;

        snprintf(*path, 512, "%s/.local/lib/jarvis", home);

        if (access(*path, F_OK) == 0)
            return EXIT_SUCCESS;
        free(*path);
        *path = NULL;
    }

    return EXIT_FAILURE;
}

// Prefer the XDG config directory when it already exists.
int GetDotJarvisPath(char **path)
{
    *path = NULL;
    char *home = getenv("HOME");
    if (home)
    {
        *path = malloc(512);

        if (!*path)
            return EXIT_FAILURE;

        char config_path[512];
        struct stat config_stat;
        snprintf(config_path, sizeof(config_path), "%s/.config", home);
        if (stat(config_path, &config_stat) == 0
            && S_ISDIR(config_stat.st_mode))
            snprintf(*path, 512, "%s/.config/.jarvis", home);
        else
            snprintf(*path, 512, "%s/.jarvis", home);

        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}