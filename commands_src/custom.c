#include "custom.h"

#define PREFIX_MAX_LENGTH 32
#define OTHERS_MAX_LENGTH 64

static const char *LAYERS[3] = { "year", "semester", "bimester" };
static const char *LABELS[3] = { "Year", "Semester", "Bimester" };

// Name of the "Others" folder as it is on disk (read when the command starts,
// updated once the tree has been reorganized), so that a rename moves the
// repositories of the old folder
static char *others_on_disk = NULL;

static int __MatchLayer(const char *component, size_t length, int *layer,
                        long *number);

// Settings are stored in <root repository>/.jarvis_config (shared by every
// Jarvis using this root repository once it is pushed)
static void __Save(const char *key, const char *value)
{
    if (SetConfigValue(key, value) != EXIT_SUCCESS)
        errx(EXIT_FAILURE,
             "ERROR Impossible to save the setting %s in .jarvis_config", key);
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

// Append a formatted text to a growing report (malloc'd, freed by the caller)
static void __Report(char **report, size_t *size, const char *format, ...)
{
    va_list ap;
    va_list copy;
    va_start(ap, format);
    va_copy(copy, ap);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (length > 0)
    {
        char *grown = realloc(*report, *size + (size_t)length + 1);
        if (grown == NULL)
            err(EXIT_FAILURE, "realloc");
        *report = grown;
        vsnprintf(*report + *size, (size_t)length + 1, format, ap);
        *size += (size_t)length;
    }
    va_end(ap);
}

// Show where a repository of the 4th year / 2nd semester... would go
// (bimester 8 = semester 4 = year 2)
static void __PrintCurrent(void)
{
    char *folders[3];
    size_t count = GetLayerFolders(8, folders);

    char layout[512] = "EPITA-TPs/";
    for (size_t i = 0; i < count; i++)
    {
        strncat(layout, folders[i], sizeof(layout) - strlen(layout) - 1);
        strncat(layout, "/", sizeof(layout) - strlen(layout) - 1);
        free(folders[i]);
    }
    strncat(layout, "<repository>", sizeof(layout) - strlen(layout) - 1);

    char *others = GetOthersFolderName();
    UiPanel(stdout, UI_INFO, "Current organization",
            "%s\nEPITA-TPs/%s/<repository>  (when the course can't be "
            "deduced)",
            layout, others);
    free(others);

    if (count == 0)
        UiWarning("All the layers are disabled : the repositories go directly "
                  "in the root repository.");
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

    UiQuestion("Prefix of the %s folders (now \"%s\", ex : %s4)\n"
               "Type the new prefix (letters, digits, - _ .), empty to cancel.",
               LABELS[layer_index], current, current);
    free(current);

    char *prefix = __ReadLine("> ");
    if (prefix == NULL)
    {
        UiInfo("Nothing changed.");
        return 0;
    }

    if (!__IsValidPrefix(prefix))
    {
        UiError("Invalid prefix : use 1 to %d characters among letters, "
                "digits, - _ . (not starting with a dot).",
                PREFIX_MAX_LENGTH);
        free(prefix);
        return 0;
    }

    __SetLayerPrefix(layer, prefix);
    UiSuccess("%s folders will now be named %s<n>.", LABELS[layer_index],
              prefix);
    free(prefix);
    return 1;
}

// The name of the "Others" folder is a single folder name, and must not be
// mistaken for a layer folder (ex : S1)
static int __IsValidOthersName(const char *name)
{
    size_t length = strlen(name);
    if (length == 0 || length > OTHERS_MAX_LENGTH || name[0] == '.')
        return 0;

    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)name[i];
        if (c == '/' || c == '\\' || c == '"' || c == '\'' || c == '='
            || iscntrl(c))
            return 0;
    }

    int layer;
    long number;
    return !__MatchLayer(name, length, &layer, &number);
}

static int __AskOthers(void)
{
    char *current = GetOthersFolderName();

    UiQuestion("Name of the folder used when the course can't be deduced "
               "(now \"%s\")\n"
               "Type the new name, empty to cancel.",
               current);

    char *name = __ReadLine("> ");
    if (name == NULL)
    {
        UiInfo("Nothing changed.");
        free(current);
        return 0;
    }

    if (!__IsValidOthersName(name))
    {
        UiError("Invalid name : use 1 to %d characters, no '/', '\\', quotes "
                "or '=', not starting with a dot, and not looking like a "
                "layer folder (ex : S1).",
                OTHERS_MAX_LENGTH);
        free(name);
        free(current);
        return 0;
    }

    if (strcmp(name, current) == 0)
    {
        UiInfo("Nothing changed.");
        free(name);
        free(current);
        return 0;
    }

    __Save("others_name", name);
    UiSuccess("The repositories without a known course will now go in %s.",
              name);
    free(name);
    free(current);
    return 1;
}

static int __PrefixesMenu(void)
{
    int changed = 0;

    UiQuestion("Which prefix do you want to change ?");
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
    }

    return changed;
}

static int __LayersMenu(void)
{
    int changed = 0;

    UiQuestion("Which layers should be used ?\n"
               "(select a layer to enable or disable it)");
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

// ---------------------------------------------------------------------------
// Format : automatic "jarvis format" before each push, and its extensions
// ---------------------------------------------------------------------------

typedef struct
{
    char **items;
    size_t count;
} ExtensionList;

// Split "a,b,c" (items are malloc'd)
static ExtensionList __SplitList(const char *text, const char *separators)
{
    ExtensionList list = { NULL, 0 };
    char *copy = strdup(text);
    if (copy == NULL)
        err(EXIT_FAILURE, "strdup");

    list.items = malloc((strlen(text) + 1) * sizeof(char *));
    if (list.items == NULL)
        err(EXIT_FAILURE, "malloc");

    char *save = NULL;
    for (char *token = strtok_r(copy, separators, &save); token != NULL;
         token = strtok_r(NULL, separators, &save))
    {
        list.items[list.count] = strdup(token);
        if (list.items[list.count] == NULL)
            err(EXIT_FAILURE, "strdup");
        list.count++;
    }

    free(copy);
    return list;
}

static void __FreeList(ExtensionList *list)
{
    for (size_t i = 0; i < list->count; i++)
        free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static int __ListIndex(const ExtensionList *list, const char *extension)
{
    for (size_t i = 0; i < list->count; i++)
        if (strcmp(list->items[i], extension) == 0)
            return (int)i;
    return -1;
}

// Save the list as ".a,.b,.c"
static int __SaveExtensions(const ExtensionList *list)
{
    size_t size = 1;
    for (size_t i = 0; i < list->count; i++)
        size += strlen(list->items[i]) + 1;

    char *joined = malloc(size);
    if (joined == NULL)
        err(EXIT_FAILURE, "malloc");
    joined[0] = '\0';
    for (size_t i = 0; i < list->count; i++)
    {
        if (i > 0)
            strcat(joined, ",");
        strcat(joined, list->items[i]);
    }

    int result = SetConfigValue("format_extensions", joined);
    free(joined);
    if (result != EXIT_SUCCESS)
        UiError("Impossible to save the extensions (is the list too long ?).");
    return result == EXIT_SUCCESS;
}

static void __PrintExtensions(void)
{
    char *current = GetFormatExtensions();
    ExtensionList list = __SplitList(current, ",");
    free(current);

    if (list.count == 0)
    {
        UiPanel(stdout, UI_INFO, "Format extensions",
                "(none : jarvis format does nothing)");
        __FreeList(&list);
        return;
    }

    size_t size = 1;
    for (size_t i = 0; i < list.count; i++)
        size += strlen(list.items[i]) + 2;
    char *text = malloc(size);
    if (text == NULL)
        err(EXIT_FAILURE, "malloc");
    text[0] = '\0';
    for (size_t i = 0; i < list.count; i++)
    {
        if (i > 0)
            strcat(text, ", ");
        strcat(text, list.items[i]);
    }

    UiPanel(stdout, UI_INFO, "Format extensions", "%zu extension(s) :\n%s",
            list.count, text);
    free(text);
    __FreeList(&list);
}

// Ask extensions (separated by commas), return the normalized ones. The
// invalid entries are reported. NULL list items when nothing was typed.
static ExtensionList __AskExtensions(const char *question)
{
    ExtensionList result = { NULL, 0 };

    UiQuestion("%s\n"
               "Case, spaces and the leading dot don't matter (ex : \"C++, "
               ".Json,  pb.txt\"). Separate several with commas, empty to "
               "cancel.",
               question);
    char *line = __ReadLine("> ");
    if (line == NULL)
        return result;

    ExtensionList typed = __SplitList(line, ",");
    free(line);

    result.items = malloc((typed.count + 1) * sizeof(char *));
    if (result.items == NULL)
        err(EXIT_FAILURE, "malloc");

    for (size_t i = 0; i < typed.count; i++)
    {
        char *normalized = NormalizeExtension(typed.items[i]);
        if (normalized == NULL)
            UiWarning("\"%s\" is not a valid extension (letters, digits, + - _ "
                      "and inner dots only), ignored.",
                      typed.items[i]);
        else if (__ListIndex(&result, normalized) >= 0)
            free(normalized);
        else
            result.items[result.count++] = normalized;
    }

    __FreeList(&typed);
    return result;
}

static int __AddExtensions(void)
{
    ExtensionList added = __AskExtensions("Extensions to add to jarvis format");
    if (added.count == 0)
    {
        UiInfo("Nothing changed.");
        __FreeList(&added);
        return 0;
    }

    char *current = GetFormatExtensions();
    ExtensionList list = __SplitList(current, ",");
    free(current);

    size_t new_count = 0;
    for (size_t i = 0; i < added.count; i++)
    {
        if (__ListIndex(&list, added.items[i]) >= 0)
        {
            UiInfo("%s is already in the list.", added.items[i]);
            continue;
        }

        char **grown = realloc(list.items, (list.count + 1) * sizeof(char *));
        if (grown == NULL)
            err(EXIT_FAILURE, "realloc");
        list.items = grown;
        list.items[list.count++] = added.items[i];
        added.items[i] = NULL;
        new_count++;
    }

    int saved = new_count > 0 && __SaveExtensions(&list);
    if (saved)
        UiSuccess("%zu extension(s) added.", new_count);
    else if (new_count == 0)
        UiInfo("Nothing changed.");

    // Items moved to the list were set to NULL in "added"
    for (size_t i = 0; i < added.count; i++)
        free(added.items[i]);
    free(added.items);
    __FreeList(&list);
    return saved;
}

static int __RemoveExtensions(void)
{
    __PrintExtensions();
    ExtensionList removed = __AskExtensions("Extensions to remove from jarvis "
                                            "format");
    if (removed.count == 0)
    {
        UiInfo("Nothing changed.");
        __FreeList(&removed);
        return 0;
    }

    char *current = GetFormatExtensions();
    ExtensionList list = __SplitList(current, ",");
    free(current);

    size_t removed_count = 0;
    for (size_t i = 0; i < removed.count; i++)
    {
        int index = __ListIndex(&list, removed.items[i]);
        if (index < 0)
        {
            UiInfo("%s is not in the list.", removed.items[i]);
            continue;
        }

        free(list.items[index]);
        for (size_t j = (size_t)index; j + 1 < list.count; j++)
            list.items[j] = list.items[j + 1];
        list.count--;
        removed_count++;
    }

    int saved = removed_count > 0 && __SaveExtensions(&list);
    if (saved)
        UiSuccess("%zu extension(s) removed.", removed_count);
    else if (removed_count == 0)
        UiInfo("Nothing changed.");

    __FreeList(&removed);
    __FreeList(&list);
    return saved;
}

static int __ResetExtensions(void)
{
    UiQuestion("Restore the default extensions of jarvis format ?");

    const char *options[] = { "No, keep my extensions",
                              "Yes, restore the default extensions" };
    if (ChoiceMCQ(options, 2) != 1)
    {
        UiInfo("Nothing changed.");
        return 0;
    }

    if (SetConfigValue("format_extensions", GetDefaultFormatExtensions())
        != EXIT_SUCCESS)
    {
        UiError("Impossible to save the extensions.");
        return 0;
    }
    UiSuccess("Default extensions restored.");
    return 1;
}

// Returns 1 when a setting changed
static int __FormatMenu(void)
{
    int changed = 0;

    UiQuestion("What do you want to change in jarvis format ?");
    while (1)
    {
        char label_auto[128];
        snprintf(label_auto, sizeof(label_auto),
                 "[%c] Automatic format before each jarvis push",
                 IsAutoFormatEnabled() ? 'x' : ' ');

        const char *options[] = { label_auto,
                                  "Extensions : add",
                                  "Extensions : remove",
                                  "Extensions : show the list",
                                  "Extensions : restore the default ones",
                                  "Back" };

        int choice = ChoiceMCQ(options, 6);
        if (choice < 0 || choice >= 5)
            break;

        switch (choice)
        {
        case 0:
            __Save("auto_format", IsAutoFormatEnabled() ? "off" : "on");
            UiSuccess("Automatic format %s.",
                      IsAutoFormatEnabled() ? "enabled" : "disabled");
            changed = 1;
            break;
        case 1:
            changed |= __AddExtensions();
            break;
        case 2:
            changed |= __RemoveExtensions();
            break;
        case 3:
            __PrintExtensions();
            break;
        default:
            changed |= __ResetExtensions();
            break;
        }
    }

    return changed;
}

static int __Reset(void)
{
    UiQuestion("Reset the organization to the default one ?");

    const char *options[] = { "No, keep my settings",
                              "Yes, reset (Semester and Bimester only, "
                              "default prefixes, Others folder)" };
    if (ChoiceMCQ(options, 2) != 1)
    {
        UiInfo("Nothing changed.");
        return 0;
    }

    for (int i = 0; i < 3; i++)
    {
        __SetLayerEnabled(LAYERS[i], i != 0);
        __SetLayerPrefix(LAYERS[i], GetDefaultLayerPrefix(LAYERS[i]));
    }
    __Save("others_name", "Others");

    UiSuccess("Default organization restored.");
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

// A repository sitting in the previous "Others" folder goes in the new one.
// Returns the new parent folder (malloc'd), NULL if the repository is not
// concerned.
// A repository is either directly in the folder (Others/<repo>) or in the
// folder of its project (Others/<project>/<repo>, made by "jarvis fill").
static char *__OthersTarget(const char *root, const char *repo)
{
    if (others_on_disk == NULL)
        return NULL;

    char *old_parent = __JoinPath(root, others_on_disk);
    size_t length = strlen(old_parent);

    int inside = 0;
    const char *project = NULL;
    size_t project_length = 0;
    if (strncmp(repo, old_parent, length) == 0 && repo[length] == '/')
    {
        const char *rest = repo + length + 1;
        const char *slash = strchr(rest, '/');
        if (slash == NULL)
            inside = 1;
        else if (strchr(slash + 1, '/') == NULL)
        {
            inside = 1;
            project = rest;
            project_length = (size_t)(slash - rest);
        }
    }
    free(old_parent);

    char *current = GetOthersFolderName();
    char *target = NULL;
    if (inside && strcmp(current, others_on_disk) != 0)
    {
        char *others = __JoinPath(root, current);
        if (project == NULL)
            target = others;
        else
        {
            char *project_name = malloc(project_length + 1);
            if (project_name == NULL)
                err(EXIT_FAILURE, "malloc");
            memcpy(project_name, project, project_length);
            project_name[project_length] = '\0';

            target = __JoinPath(others, project_name);
            free(project_name);
            free(others);
        }
    }
    free(current);
    return target;
}

// Move every cloned repository to the place given by the current layers.
// Returns how many repositories were moved, -1 on a fatal error.
static int __ApplyOrganization(void)
{
    char *root = NULL;
    if (ReadRootPath(&root) != EXIT_SUCCESS || root == NULL)
    {
        UiWarning("Root repository not found, the tree was not "
                  "reorganized.");
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
    char *report = NULL;
    size_t report_size = 0;
    __Report(&report, &report_size, "%zu repositories found in %s", repos.count,
             root);

    for (size_t i = 0; i < repos.count; i++)
    {
        const char *repo = repos.paths[i];
        char *url = __OriginUrl(repo);
        int bimester = url == NULL ? 0 : __CourseBimester(url);
        free(url);

        // The url has no course id : use the place of the repository now
        if (bimester < 1)
            bimester = __InferBimester(root, repo);

        char *target_parent = NULL;
        if (bimester < 1)
        {
            // Not deducible (Others folder, classic clone...) : only moved
            // when the "Others" folder has been renamed
            target_parent = __OthersTarget(root, repo);
            if (target_parent == NULL)
            {
                __Report(&report, &report_size,
                         "\nskipped (Others folder or unknown layout) : %s",
                         repo);
                continue;
            }
        }
        else
        {
            target_parent = strdup(root);
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
        }

        const char *name = strrchr(repo, '/');
        name = name == NULL ? repo : name + 1;
        char *destination = __JoinPath(target_parent, name);

        if (strcmp(destination, repo) != 0)
        {
            if (access(destination, F_OK) == 0)
            {
                UiWarning("%s already exists, %s was not moved.", destination,
                          repo);
            }
            else if (__MakeDirs(target_parent) != EXIT_SUCCESS)
            {
                UiWarning("Impossible to create %s.", target_parent);
            }
            else
            {
                char *old_parent = strdup(repo);
                if (old_parent == NULL)
                    err(EXIT_FAILURE, "strdup");
                *strrchr(old_parent, '/') = '\0';

                // "git mv" keeps .gitmodules and the index valid, plain
                // rename for repositories that are not submodules
                char *git_mv[] = { "git", "-C",         root,        "mv",
                                   "--",  (char *)repo, destination, NULL };
                if (__RunProgram(git_mv) != EXIT_SUCCESS
                    && rename(repo, destination) != 0)
                {
                    UiWarning("Impossible to move %s.", repo);
                }
                else
                {
                    char *absorb[] = {
                        "git",           "-C", root,        "submodule",
                        "absorbgitdirs", "--", destination, NULL
                    };
                    __RunProgram(absorb);

                    __Report(&report, &report_size, "\n%s\n  -> %s", repo,
                             destination);
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

    // The "Others" folder on disk now follows the configuration
    free(others_on_disk);
    others_on_disk = GetOthersFolderName();

    if (moved == 0)
        __Report(&report, &report_size,
                 "\n\nNothing to move, the tree already matches the "
                 "organization.");
    UiPanel(stdout, UI_INFO, "Organization", "%s", report);
    free(report);
    if (moved > 0)
        UiSuccess("%d repositories moved.", moved);

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
            UiWarning("The changes could not be pushed, run 'jarvis push "
                      "root'.");
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

    // Reading the settings creates .jarvis_config (default values) in the
    // root repository when it doesn't exist yet
    EnsureConfigFile();
    others_on_disk = GetOthersFolderName();

    UiPanel(stdout, UI_INFO, "Custom",
            "Customize the tree structure of your repositories");
    __PrintCurrent();

    int changed = 0;
    int format_changed = 0;
    int running = 1;
    while (running)
    {
        UiQuestion("What do you want to change ?");

        const char *options[] = {
            "Layers : enable or disable Year / Semester / Bimester folders",
            "Prefixes : choose the text before the number (ex : S<n>)",
            "Others : rename the folder used when the course is unknown",
            "Format : automatic jarvis format before push, extensions",
            "Reset : go back to the default organization",
            "Save : apply to the tree and push",
            "Quit : apply to the tree (no push)"
        };

        int updated = 0;
        switch (ChoiceMCQ(options, 7))
        {
        case 0:
            updated = __LayersMenu();
            break;
        case 1:
            updated = __PrefixesMenu();
            break;
        case 2:
            updated = __AskOthers();
            break;
        case 3:
            // Saved at once in .jarvis_config, no change in the tree
            if (__FormatMenu())
                format_changed = 1;
            break;
        case 4:
            updated = __Reset();
            break;
        case 5:
            // Save : apply the organization, then push the root repository
            if (__Finish(1) == EXIT_SUCCESS)
                changed = format_changed = 0;
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
        UiWarning("Not pushed : choose \"Save\" next time, or run 'jarvis "
                  "push root'.");
        if (format_changed)
            UiWarning("The format settings are in the same file.");
        return result;
    }

    if (format_changed)
        UiWarning("Format settings not pushed : choose \"Save\" next time, or "
                  "run 'jarvis push root'.");

    return EXIT_SUCCESS;
}