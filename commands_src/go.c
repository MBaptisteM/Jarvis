#include "go.h"

static char **found_paths = NULL;
static size_t found_count = 0;
static size_t found_capacity = 0;

static void __Trim(char *str)
{
    size_t len = strlen(str);
    while (len > 0
           && (str[len - 1] == '\n' || str[len - 1] == '\r'
               || str[len - 1] == ' ' || str[len - 1] == '\t'))
        str[--len] = '\0';
}

static char *__Join(const char *parent, const char *name)
{
    size_t parent_len = strlen(parent);
    int need_slash = parent_len > 0 && parent[parent_len - 1] != '/';
    size_t size = parent_len + strlen(name) + 2;
    char *path = malloc(size);
    if (path == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(path, size, "%s%s%s", parent, need_slash ? "/" : "", name);
    return path;
}

static void __AddPath(const char *path)
{
    for (size_t i = 0; i < found_count; i++)
        if (strcmp(found_paths[i], path) == 0)
            return;

    if (found_count == found_capacity)
    {
        found_capacity = found_capacity == 0 ? 16 : found_capacity * 2;
        char **resized = realloc(found_paths, found_capacity * sizeof(char *));
        if (resized == NULL)
            err(EXIT_FAILURE, "realloc");
        found_paths = resized;
    }
    found_paths[found_count] = strdup(path);
    if (found_paths[found_count] == NULL)
        err(EXIT_FAILURE, "strdup");
    found_count++;
}

// Last component of a path / url, without a trailing ".git"
static const char *__BaseName(const char *path, size_t *length)
{
    const char *last = strrchr(path, '/');
    last = last == NULL ? path : last + 1;
    size_t len = strlen(last);
    if (len > 4 && strcmp(last + len - 4, ".git") == 0)
        len -= 4;
    *length = len;
    return last;
}

// Same rule as clone.c / find.c : <letters>-<3 digits>-<letter>-<2 digits>-<4
// digits> (ex : prog-208-p-05-2029). 0 on success, 1 otherwise.
static int __ParseCourseId(const char *url, int *bimester, char *stubborn)
{
    const char *start = url;

    while (1)
    {
        const char *end = start;
        while (*end != '\0' && *end != '/' && *end != ':')
            end++;

        size_t length = (size_t)(end - start);
        size_t i = 0;
        while (i < length && isalpha((unsigned char)start[i]))
            i++;

        const char *p = start + i;
        if (i > 0 && length - i == 14 && p[0] == '-'
            && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2])
            && isdigit((unsigned char)p[3]) && p[4] == '-'
            && isalpha((unsigned char)p[5]) && p[6] == '-'
            && isdigit((unsigned char)p[7]) && isdigit((unsigned char)p[8])
            && p[9] == '-' && isdigit((unsigned char)p[10])
            && isdigit((unsigned char)p[11]) && isdigit((unsigned char)p[12])
            && isdigit((unsigned char)p[13]))
        {
            int num_bimester = (p[2] - '0') * 10 + (p[3] - '0');
            if (num_bimester < 1)
                return 1;

            *bimester = num_bimester;

            size_t k = 0;
            stubborn[k++] = (char)toupper((unsigned char)p[5]);
            if (p[7] != '0')
                stubborn[k++] = p[7];
            stubborn[k++] = p[8];
            stubborn[k] = '\0';
            return 0;
        }

        if (*end == '\0')
            break;
        start = end + 1;
    }

    return 1;
}

static int __IsEpitaUrl(const char *url)
{
    return strstr(url, "@git.forge.epita.fr") != NULL;
}

static int __ContainsWord(const char *haystack, const char *word)
{
    size_t len_haystack = strlen(haystack);
    size_t len_word = strlen(word);
    if (len_word == 0 || len_word > len_haystack)
        return 0;

    for (size_t i = 0; i + len_word <= len_haystack; i++)
        if (strncasecmp(haystack + i, word, len_word) == 0)
            return 1;

    return 0;
}

// Every folder whose name contains "word" (does not go inside a match, nor
// inside .git folders)
static void __SearchTree(const char *path, const char *word)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
            || strcmp(entry->d_name, ".git") == 0)
            continue;

        char *full_path = __Join(path, entry->d_name);
        if (IsDirectory(full_path))
        {
            if (__ContainsWord(entry->d_name, word))
                __AddPath(full_path);
            else
                __SearchTree(full_path, word);
        }
        free(full_path);
    }

    closedir(dir);
}

static void __CollectCurrent(const char *root)
{
    char *current;
    if (ReadInfo("current", &current))
        errx(EXIT_FAILURE,
             "ERROR Trying to go to the current repository but no current "
             "repository found");
    __Trim(current);

    char *path = current[0] == '/' ? strdup(current) : __Join(root, current);
    free(current);
    if (path == NULL)
        err(EXIT_FAILURE, "strdup");

    if (access(path, F_OK) != 0)
    {
        size_t name_len;
        const char *name = __BaseName(path, &name_len);
        char *repo_name = strndup(name, name_len);
        if (repo_name == NULL)
            err(EXIT_FAILURE, "strndup");

        char *parent = FindFileBFS(repo_name);
        if (parent == NULL)
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the current repository %s locally "
                 "(try to re-clone it)",
                 repo_name);

        free(path);
        path = __Join(parent, repo_name);
        free(parent);
        free(repo_name);
        WriteInfo("current", path);
    }

    __AddPath(path);
    free(path);
}

// repo_arg is a git remote already registered in the info files
static int __CollectKnown(const char *repo_arg, const char *root)
{
    char *repo_name;
    if (ReadInfo((char *)repo_arg, &repo_name))
        return 0;
    __Trim(repo_name);

    char *path = __Join(root, repo_name);
    if (access(path, F_OK) != 0)
    {
        char *parent = FindFileBFS(repo_name);
        free(path);
        path = parent == NULL ? NULL : __Join(parent, repo_name);
        free(parent);
        if (path == NULL)
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the repository %s locally (try to "
                 "re-clone it)",
                 repo_name);
    }

    __AddPath(path);
    free(path);
    free(repo_name);
    return 1;
}

// EPITA url not registered : recompute its expected place in the tree
static void __CollectEpitaUrl(const char *url, const char *root)
{
    int bimester;
    char stubborn[64];

    if (__ParseCourseId(url, &bimester, stubborn) != 0)
    {
        // "Others" : the folder keeps the name of the git repository
        size_t name_len;
        const char *name = __BaseName(url, &name_len);
        char *folder_name = strndup(name, name_len);
        if (folder_name == NULL)
            err(EXIT_FAILURE, "strndup");

        char *others_name = GetOthersFolderName();
        char *others = __Join(root, others_name);
        free(others_name);
        char *repo = __Join(others, folder_name);
        if (IsDirectory(repo))
            __AddPath(repo);

        free(repo);
        free(others);
        free(folder_name);
        return;
    }

    // Layers chosen with "jarvis custom" (Year / Semester / Bimester), maybe none
    char *target = strdup(root);
    if (target == NULL)
        err(EXIT_FAILURE, "strdup");

    char *folders[3];
    size_t folder_count = GetLayerFolders(bimester, folders);
    for (size_t i = 0; i < folder_count; i++)
    {
        char *next = __Join(target, folders[i]);
        free(target);
        free(folders[i]);
        target = next;
    }

    DIR *dir = opendir(target);
    if (dir != NULL)
    {
        size_t len_stubborn = strlen(stubborn);
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strncasecmp(entry->d_name, stubborn, len_stubborn) != 0
                || entry->d_name[len_stubborn] != '-')
                continue;

            char *full_path = __Join(target, entry->d_name);
            if (IsDirectory(full_path))
                __AddPath(full_path);
            free(full_path);
        }
        closedir(dir);
    }

    free(target);
}

// Let the user pick one of the found folders. The menu is drawn on stderr so
// that stdout only carries the final path.
static const char *__ChooseOne(const char *root)
{
    if (found_count == 1)
        return found_paths[0];

    size_t root_len = strlen(root);
    while (root_len > 0 && root[root_len - 1] == '/')
        root_len--;

    const char **options = calloc(found_count, sizeof(char *));
    if (options == NULL)
        err(EXIT_FAILURE, "calloc");
    for (size_t i = 0; i < found_count; i++)
    {
        const char *display = found_paths[i];
        if (strncmp(display, root, root_len) == 0 && display[root_len] == '/')
            display += root_len + 1;
        options[i] = display;
    }

    UiPanel(stderr, UI_INFO, "Go", "Where do you want to go ?");

    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0 || dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
        err(EXIT_FAILURE, "dup");

    int choice = ChoiceMCQ(options, (int)found_count);

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    free(options);

    if (choice < 0 || (size_t)choice >= found_count)
        errx(EXIT_FAILURE, "ERROR Impossible to get the choice");

    return found_paths[choice];
}

int main(int argc, char *argv[])
{
    char *root;
    if (ReadRootPath(&root))
        errx(EXIT_FAILURE,
             "ERROR Impossible to find the root repository (did you clone "
             "anything yet?)");
    __Trim(root);

    if (argc < 2 || strcasecmp(argv[1], "root") == 0)
        __AddPath(root);
    else if (IsCurrentKeyword(argv[1]))
        __CollectCurrent(root);
    else if (!__CollectKnown(argv[1], root))
    {
        if (__IsEpitaUrl(argv[1]))
            __CollectEpitaUrl(argv[1], root);
        else
            __SearchTree(root, argv[1]);
    }

    if (found_count == 0)
        errx(EXIT_FAILURE,
             "ERROR No repository found for \"%s\" (it might not be cloned "
             "yet).",
             argc < 2 ? "root" : argv[1]);

    const char *target = __ChooseOne(root);
    if (!IsDirectory(target))
        errx(EXIT_FAILURE, "ERROR %s is not a folder.", target);

    if (isatty(STDOUT_FILENO))
    {
        char *destination = strdup(target);
        if (destination == NULL)
            err(EXIT_FAILURE, "strdup");

        char *previous_directory = getcwd(NULL, 0);
        if (previous_directory == NULL)
            err(EXIT_FAILURE, "getcwd");
        if (setenv("OLDPWD", previous_directory, 1) != 0)
            err(EXIT_FAILURE, "setenv(OLDPWD)");
        free(previous_directory);

        if (chdir(destination) != 0)
            err(EXIT_FAILURE, "chdir(%s)", destination);

        const char *shell = getenv("SHELL");
        if (shell == NULL || shell[0] == '\0')
            shell = "/bin/sh";

        UiPanel(stderr, UI_SUCCESS, "Jarvis",
                "You are now in:\n%s\n(type exit to come back)", destination);

        execl(shell, shell, (char *)NULL);
        err(EXIT_FAILURE, "execl(%s)", shell);
    }

    // Output captured by a shell function ($(jarvis go ...)) : the path is the
    // last line of stdout, the function does the real "cd" in the user's shell
    printf("%s\n", target);

    for (size_t i = 0; i < found_count; i++)
        free(found_paths[i]);
    free(found_paths);
    free(root);
    return EXIT_SUCCESS;
}