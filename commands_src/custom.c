#include "custom.h"

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "TPs_handler.h"

int ReadRootPath(char **value);
size_t GetLayerFolders(int bimester, char **folders);

#define PREFIX_MAX_LENGTH 32

static const char *LAYERS[3] = { "year", "semester", "bimester" };
static const char *LABELS[3] = { "Year", "Semester", "Bimester" };

static void __Save(const char *key, const char *value)
{
    if (WriteInfo((char *)key, (char *)value) != EXIT_SUCCESS)
        errx(EXIT_FAILURE, "ERROR Impossible to save the setting %s", key);
}

static void __SetLayerEnabled(const char *layer, int enabled)
{
    char key[64];
    snprintf(key, sizeof(key), "layer_%s", layer);
    __Save(key, enabled ? "on" : "off");
}

static void __SetLayerPrefix(const char *layer, const char *prefix)
{
    char key[64];
    snprintf(key, sizeof(key), "prefix_%s", layer);
    __Save(key, prefix);
}


// Show where a repository of the 4th year / 2nd semester... would go
// (bimester 8 = semester 4 = year 2)
static void __PrintCurrent(void)
{
    char *folders[3];
    size_t count = GetLayerFolders(8, folders);

    printf("\n\033[1mCurrent organization :\033[0m\n");
    printf("  EPITA-TPs/");
    for (size_t i = 0; i < count; i++)
    {
        printf("%s/", folders[i]);
        free(folders[i]);
    }
    printf("<repository>\n");
    printf("  EPITA-TPs/Others/<repository>  (when the course can't be "
           "deduced)\n");

    if (count == 0)
        printf("\033[33mAll the layers are disabled : the repositories go "
               "directly in the root repository.\033[0m\n");
}


// A prefix is written in folder names, so it must stay simple
static int __IsValidPrefix(const char *prefix)
{
    size_t length = strlen(prefix);
    if (length == 0 || length > PREFIX_MAX_LENGTH || prefix[0] == '.')
        return 0;

    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)prefix[i];
        if (!isalnum(c) && c != '-' && c != '_' && c != '.')
            return 0;
    }

    return 1;
}

// Ask a line on stdin, NULL if empty or stdin is closed
static char *__ReadLine(const char *message)
{
    printf("%s", message);
    fflush(stdout);

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        return NULL;

    buffer[strcspn(buffer, "\r\n")] = '\0';
    if (buffer[0] == '\0')
        return NULL;

    char *line = strdup(buffer);
    if (line == NULL)
        err(EXIT_FAILURE, "strdup");
    return line;
}

static int __AskPrefix(int layer_index)
{
    const char *layer = LAYERS[layer_index];
    char *current = GetLayerPrefix(layer);

    char message[256];
    snprintf(message, sizeof(message),
             "\nPrefix of the %s folders (now \"%s\", ex : %s4)\n"
             "Type the new prefix (letters, digits, - _ .), empty to "
             "cancel : ",
             LABELS[layer_index], current, current);
    free(current);

    char *prefix = __ReadLine(message);
    if (prefix == NULL)
    {
        printf("Nothing changed.\n");
        return 0;
    }

    if (!__IsValidPrefix(prefix))
    {
        printf("\033[31mInvalid prefix : use 1 to %d characters among letters, "
               "digits, - _ . (not starting with a dot).\033[0m\n",
               PREFIX_MAX_LENGTH);
        free(prefix);
        return 0;
    }

    __SetLayerPrefix(layer, prefix);
    printf("\033[32m%s folders will now be named %s<n>.\033[0m\n",
           LABELS[layer_index], prefix);
    free(prefix);
    return 1;
}

static int __PrefixesMenu(void)
{
    int changed = 0;

    printf("\n\033[1mWhich prefix do you want to change ?\033[0m\n\n");
    while (1)
    {
        char labels[4][128];
        const char *options[4];

        for (int i = 0; i < 3; i++)
        {
            char *prefix = GetLayerPrefix(LAYERS[i]);
            snprintf(labels[i], sizeof(labels[i]), "%-9s folders : %s<n>",
                     LABELS[i], prefix);
            free(prefix);
            options[i] = labels[i];
        }
        snprintf(labels[3], sizeof(labels[3]), "Back");
        options[3] = labels[3];

        int choice = ChoiceMCQ(options, 4);
        if (choice < 0 || choice >= 3)
            break;

        if (__AskPrefix(choice))
            changed = 1;
        printf("\n");
    }

    return changed;
}


static int __LayersMenu(void)
{
    int changed = 0;

    printf("\n\033[1mWhich layers should be used ?\033[0m (select a layer to "
           "enable or disable it)\n\n");
    while (1)
    {
        char labels[4][128];
        const char *options[4];

        for (int i = 0; i < 3; i++)
        {
            char *prefix = GetLayerPrefix(LAYERS[i]);
            snprintf(labels[i], sizeof(labels[i]), "[%c] %-9s (%s<n>)",
                     IsLayerEnabled(LAYERS[i]) ? 'x' : ' ', LABELS[i], prefix);
            free(prefix);
            options[i] = labels[i];
        }
        snprintf(labels[3], sizeof(labels[3]), "Back");
        options[3] = labels[3];

        int choice = ChoiceMCQ(options, 4);
        if (choice < 0 || choice >= 3)
            break;

        __SetLayerEnabled(LAYERS[choice], !IsLayerEnabled(LAYERS[choice]));
        changed = 1;
    }

    return changed;
}


static int __Reset(void)
{
    printf("\n\033[1mReset the organization to the default one ?\033[0m\n\n");

    const char *options[] = { "No, keep my settings",
                              "Yes, reset (Semester and Bimester only, "
                              "default prefixes)" };
    if (ChoiceMCQ(options, 2) != 1)
    {
        printf("Nothing changed.\n");
        return 0;
    }

    for (int i = 0; i < 3; i++)
    {
        __SetLayerEnabled(LAYERS[i], i != 0);
        __SetLayerPrefix(LAYERS[i], GetDefaultLayerPrefix(LAYERS[i]));
    }

    printf("\033[32mDefault organization restored.\033[0m\n");
    return 1;
}


// ---------------------------------------------------------------------------
// Apply the organization to the existing tree
// ---------------------------------------------------------------------------

typedef struct
{
    char **paths;
    size_t count;
    size_t capacity;
} PathList;

static char *__JoinPath(const char *parent, const char *name)
{
    size_t size = strlen(parent) + strlen(name) + 2;
    char *path = malloc(size);
    if (path == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(path, size, "%s/%s", parent, name);
    return path;
}

static int __IsDir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void __PathListAdd(PathList *list, const char *path)
{
    if (list->count == list->capacity)
    {
        list->capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        char **resized = realloc(list->paths, list->capacity * sizeof(char *));
        if (resized == NULL)
            err(EXIT_FAILURE, "realloc");
        list->paths = resized;
    }
    list->paths[list->count] = strdup(path);
    if (list->paths[list->count] == NULL)
        err(EXIT_FAILURE, "strdup");
    list->count++;
}

// Every nested repository (folder containing a .git) of the root repository.
// The root's own .git is ignored and a nested repository is not explored.
static void __CollectRepos(const char *dir_path, PathList *list)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
            || strcmp(entry->d_name, ".git") == 0)
            continue;

        char *full_path = __JoinPath(dir_path, entry->d_name);
        if (__IsDir(full_path))
        {
            char *git_path = __JoinPath(full_path, ".git");
            int is_repo = access(git_path, F_OK) == 0;
            free(git_path);

            if (is_repo)
                __PathListAdd(list, full_path);
            else
                __CollectRepos(full_path, list);
        }
        free(full_path);
    }

    closedir(dir);
}

// Same rule as clone.c : <letters>-<3 digits>-<letter>-<2 digits>-<4 digits>
// Returns the bimester number, or 0 if the url is not an EPITA course url.
static int __CourseBimester(const char *url)
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
            return (p[2] - '0') * 10 + (p[3] - '0');

        if (*end == '\0')
            break;
        start = end + 1;
    }

    return 0;
}

// Origin url of a repository, malloc'd, NULL if it has none
static char *__OriginUrl(const char *repo_path)
{
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0)
        return NULL;

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return NULL;
    }
    if (pid == 0)
    {
        close(pipe_fds[0]);
        dup2(pipe_fds[1], STDOUT_FILENO);
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0)
            dup2(null_fd, STDERR_FILENO);
        execlp("git", "git", "-C", repo_path, "remote", "get-url", "origin",
               (char *)NULL);
        _exit(EXIT_FAILURE);
    }
    close(pipe_fds[1]);

    char buffer[1024];
    ssize_t total = 0;
    ssize_t n;
    while (total < (ssize_t)sizeof(buffer) - 1
           && (n = read(pipe_fds[0], buffer + total,
                        sizeof(buffer) - 1 - (size_t)total))
               > 0)
        total += n;
    close(pipe_fds[0]);

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || total <= 0)
        return NULL;

    buffer[total] = '\0';
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return buffer[0] == '\0' ? NULL : strdup(buffer);
}

static int __RunProgram(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return EXIT_FAILURE;
    if (pid == 0)
    {
        execvp(argv[0], argv);
        _exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status))
        return EXIT_FAILURE;
    return WEXITSTATUS(status) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

// mkdir -p
static int __MakeDirs(const char *path)
{
    char *copy = strdup(path);
    if (copy == NULL)
        err(EXIT_FAILURE, "strdup");

    for (char *c = copy + 1; *c != '\0'; c++)
    {
        if (*c != '/')
            continue;
        *c = '\0';
        if (mkdir(copy, 0755) != 0 && !__IsDir(copy))
        {
            free(copy);
            return EXIT_FAILURE;
        }
        *c = '/';
    }

    int result = EXIT_SUCCESS;
    if (mkdir(copy, 0755) != 0 && !__IsDir(copy))
        result = EXIT_FAILURE;
    free(copy);
    return result;
}

// Remove the empty folders left behind, from "dir" up to (excluding) root
static void __RemoveEmptyParents(const char *dir, const char *root)
{
    char *copy = strdup(dir);
    if (copy == NULL)
        err(EXIT_FAILURE, "strdup");

    size_t root_len = strlen(root);
    while (strlen(copy) > root_len && strncmp(copy, root, root_len) == 0)
    {
        if (rmdir(copy) != 0)
            break;
        char *slash = strrchr(copy, '/');
        if (slash == NULL)
            break;
        *slash = '\0';
    }
    free(copy);
}

// Match a folder name against the layers. A folder is exactly <prefix><number>
// where the prefix is the default one (Y / S / B) or the one currently
// configured, so a tree organized with an older configuration is still read
// correctly. Unexpected names (ex : S-14) never match.
static int __MatchLayer(const char *component, size_t length, int *layer,
                        long *number)
{
    for (int i = 0; i < 3; i++)
    {
        char *candidates[2];
        candidates[0] = strdup(GetDefaultLayerPrefix(LAYERS[i]));
        candidates[1] = GetLayerPrefix(LAYERS[i]);
        if (candidates[0] == NULL)
            err(EXIT_FAILURE, "strdup");

        int matched = 0;
        for (int c = 0; c < 2 && !matched; c++)
        {
            size_t prefix_len = strlen(candidates[c]);
            if (length <= prefix_len || length - prefix_len > 6
                || strncmp(component, candidates[c], prefix_len) != 0)
                continue;

            long value = 0;
            int valid = 1;
            for (size_t k = prefix_len; k < length; k++)
            {
                if (!isdigit((unsigned char)component[k]))
                {
                    valid = 0;
                    break;
                }
                value = value * 10 + (component[k] - '0');
            }

            if (valid)
            {
                *layer = i;
                *number = value;
                matched = 1;
            }
        }

        free(candidates[0]);
        free(candidates[1]);
        if (matched)
            return 1;
    }

    return 0;
}

// Deduce the bimester of a repository from where it currently is in the tree
// (ex : root/S1/B2/repo -> 2, root/Y1/S1/B2/repo -> 2). 0 if it can't be
// deduced (Others folder, unknown layout...).
static int __InferBimester(const char *root, const char *repo)
{
    size_t root_len = strlen(root);
    if (strncmp(repo, root, root_len) != 0 || repo[root_len] != '/')
        return 0;

    char *relative = strdup(repo + root_len + 1);
    if (relative == NULL)
        err(EXIT_FAILURE, "strdup");

    // Remove the name of the repository, keep the layer folders
    char *last_slash = strrchr(relative, '/');
    if (last_slash == NULL)
    {
        free(relative);
        return 0;
    }
    *last_slash = '\0';

    int previous_layer = -1;
    int finest = -1;
    long number = 0;

    const char *cursor = relative;
    while (*cursor != '\0')
    {
        const char *end = strchr(cursor, '/');
        size_t length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);

        int layer;
        long value;
        if (!__MatchLayer(cursor, length, &layer, &value)
            || layer <= previous_layer)
        {
            free(relative);
            return 0;
        }

        previous_layer = layer;
        finest = layer;
        number = value;
        cursor = end == NULL ? cursor + length : end + 1;
    }
    free(relative);

    if (finest < 0 || number < 1)
        return 0;

    int multiplier = finest == 2 ? 1 : (finest == 1 ? 2 : 4);
    return (int)number * multiplier;
}

static int __PathListHas(const PathList *list, const char *path)
{
    for (size_t i = 0; i < list->count; i++)
        if (strcmp(list->paths[i], path) == 0)
            return 1;
    return 0;
}

// Repositories found by the shape of the tree : inside a chain of layer
// folders (ex : S1/B2), every folder that is not a deeper layer is a
// repository, whether or not it contains a .git
static void __CollectByLayout(const char *dir_path, int previous_layer,
                              PathList *list)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
            || strcmp(entry->d_name, ".git") == 0)
            continue;

        char *full_path = __JoinPath(dir_path, entry->d_name);
        if (__IsDir(full_path))
        {
            int layer;
            long value;
            if (__MatchLayer(entry->d_name, strlen(entry->d_name), &layer,
                             &value)
                && layer > previous_layer)
                __CollectByLayout(full_path, layer, list);
            else if (previous_layer >= 0 && !__PathListHas(list, full_path))
                __PathListAdd(list, full_path);
        }
        free(full_path);
    }

    closedir(dir);
}

// Submodules declared in .gitmodules : they count as repositories even when
// their folder has no .git (submodule not initialized)
static void __CollectSubmodules(const char *root, PathList *list)
{
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0)
        return;

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return;
    }
    if (pid == 0)
    {
        close(pipe_fds[0]);
        dup2(pipe_fds[1], STDOUT_FILENO);
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0)
            dup2(null_fd, STDERR_FILENO);
        execlp("git", "git", "-C", root, "config", "-f", ".gitmodules",
               "--get-regexp", "^submodule\\..*\\.path$", (char *)NULL);
        _exit(EXIT_FAILURE);
    }
    close(pipe_fds[1]);

    FILE *stream = fdopen(pipe_fds[0], "r");
    if (stream != NULL)
    {
        char line[2048];
        while (fgets(line, sizeof(line), stream) != NULL)
        {
            line[strcspn(line, "\r\n")] = '\0';

            // "submodule.<name>.path <relative path>"
            char *space = strchr(line, ' ');
            if (space == NULL || space[1] == '\0')
                continue;

            char *full_path = __JoinPath(root, space + 1);
            if (__IsDir(full_path))
            {
                int known = 0;
                for (size_t i = 0; i < list->count; i++)
                    if (strcmp(list->paths[i], full_path) == 0)
                        known = 1;
                if (!known)
                    __PathListAdd(list, full_path);
            }
            free(full_path);
        }
        fclose(stream);
    }
    else
        close(pipe_fds[0]);

    int status;
    waitpid(pid, &status, 0);
}

// Move every cloned repository to the place given by the current layers.
// Returns how many repositories were moved, -1 on a fatal error.
static int __ApplyOrganization(void)
{
    char *root = NULL;
    if (ReadRootPath(&root) != EXIT_SUCCESS || root == NULL)
    {
        printf("\033[33mWARNING Root repository not found, the tree was not "
               "reorganized.\033[0m\n");
        return -1;
    }

    size_t root_len = strlen(root);
    while (root_len > 1 && root[root_len - 1] == '/')
        root[--root_len] = '\0';

    PathList repos = { 0 };
    __CollectRepos(root, &repos);
    __CollectByLayout(root, -1, &repos);
    __CollectSubmodules(root, &repos);

    char *current = NULL;
    if (ReadInfo("current", &current) != EXIT_SUCCESS)
        current = NULL;

    int moved = 0;
    printf("\n\033[1mApplying the organization to the current tree "
           "(%zu repositories found in %s)...\033[0m\n",
           repos.count, root);

    for (size_t i = 0; i < repos.count; i++)
    {
        const char *repo = repos.paths[i];
        char *url = __OriginUrl(repo);
        int bimester = url == NULL ? 0 : __CourseBimester(url);
        free(url);

        // The url has no course id : use the place of the repository now
        if (bimester < 1)
            bimester = __InferBimester(root, repo);

        // Not deducible (Others folder, classic clone...) : left untouched
        if (bimester < 1)
        {
            printf("  skipped (Others folder or unknown layout) : %s\n",
                   repo);
            continue;
        }

        char *target_parent = strdup(root);
        if (target_parent == NULL)
            err(EXIT_FAILURE, "strdup");

        char *folders[3];
        size_t folder_count = GetLayerFolders(bimester, folders);
        for (size_t j = 0; j < folder_count; j++)
        {
            char *next = __JoinPath(target_parent, folders[j]);
            free(target_parent);
            free(folders[j]);
            target_parent = next;
        }

        const char *name = strrchr(repo, '/');
        name = name == NULL ? repo : name + 1;
        char *destination = __JoinPath(target_parent, name);

        if (strcmp(destination, repo) != 0)
        {
            if (access(destination, F_OK) == 0)
            {
                printf("\033[33mWARNING %s already exists, %s was not "
                       "moved.\033[0m\n",
                       destination, repo);
            }
            else if (__MakeDirs(target_parent) != EXIT_SUCCESS)
            {
                printf("\033[33mWARNING Impossible to create %s.\033[0m\n",
                       target_parent);
            }
            else
            {
                char *old_parent = strdup(repo);
                if (old_parent == NULL)
                    err(EXIT_FAILURE, "strdup");
                *strrchr(old_parent, '/') = '\0';

                // "git mv" keeps .gitmodules and the index valid, plain
                // rename for repositories that are not submodules
                char *git_mv[] = { "git", "-C",   root,        "mv",
                                   "--",  (char *)repo, destination, NULL };
                if (__RunProgram(git_mv) != EXIT_SUCCESS
                    && rename(repo, destination) != 0)
                {
                    printf("\033[33mWARNING Impossible to move %s.\033[0m\n",
                           repo);
                    }
                else
                {
                    char *absorb[] = { "git", "-C",      root,
                                       "submodule",      "absorbgitdirs",
                                       "--",  destination, NULL };
                    __RunProgram(absorb);

                    printf("  %s\n    -> %s\n", repo, destination);
                    moved++;

                    if (current != NULL && strcmp(current, repo) == 0)
                    {
                        WriteInfo("current", destination);
                        free(current);
                        current = strdup(destination);
                    }

                    __RemoveEmptyParents(old_parent, root);
                }
                free(old_parent);
            }
        }

        free(destination);
        free(target_parent);
    }

    for (size_t i = 0; i < repos.count; i++)
        free(repos.paths[i]);
    free(repos.paths);
    free(current);
    free(root);

    if (moved == 0)
        printf("Nothing to move, the tree already matches the "
               "organization.\n");

    return moved;
}

// Apply the organization to the tree, and push the root repository if asked
static int __Finish(int push)
{
    int moved = __ApplyOrganization();
    if (moved < 0)
        return EXIT_FAILURE;

    if (push)
    {
        if (AddRepoRoot() != EXIT_SUCCESS
            || PushRepoRoot("Reorganize tree structure") != EXIT_SUCCESS)
        {
            printf("\033[33mWARNING The changes could not be pushed, run "
                   "'jarvis push root'.\033[0m\n");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    (void)argv;
    if (argc > 1)
        errx(EXIT_FAILURE, "Usage: jarvis custom");

    // Reading a setting creates the info file when it doesn't exist yet
    IsLayerEnabled("year");

    printf("\033[1mCustomize the tree structure of your repositories\033[0m\n");
    __PrintCurrent();

    int changed = 0;
    int running = 1;
    while (running)
    {
        printf("\n\033[1mWhat do you want to change ?\033[0m\n\n");

        const char *options[] = {
            "Layers : enable or disable Year / Semester / Bimester folders",
            "Prefixes : choose the text before the number (ex : S<n>)",
            "Reset : go back to the default organization",
            "Save : apply to the tree and push",
            "Quit : apply to the tree (no push)"
        };

        int updated = 0;
        switch (ChoiceMCQ(options, 5))
        {
        case 0:
            updated = __LayersMenu();
            break;
        case 1:
            updated = __PrefixesMenu();
            break;
        case 2:
            updated = __Reset();
            break;
        case 3:
            // Save : apply the organization, then push the root repository
            if (__Finish(1) == EXIT_SUCCESS)
                changed = 0;
            break;
        default:
            running = 0;
            break;
        }

        if (updated)
        {
            changed = 1;
            __PrintCurrent();
        }
    }

    // Quit : apply the pending changes to the current tree (no push)
    if (changed)
    {
        int result = __Finish(0);
        printf("\033[33mNot pushed : choose \"Save\" next time, or run "
               "'jarvis push root'.\033[0m\n");
        return result;
    }

    return EXIT_SUCCESS;
}