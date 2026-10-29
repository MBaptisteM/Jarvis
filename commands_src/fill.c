#include "fill.h"

// jarvis fill [old | all | current | <project>]

typedef enum
{
    SCOPE_OLD,
    SCOPE_ALL,
    SCOPE_CURRENT,
    SCOPE_PROJECT
} Scope;

typedef enum
{
    STATE_PENDING,
    STATE_CLONED,
    STATE_ALREADY,
    STATE_NO_REMOTE,
    STATE_FAILED,
    STATE_IGNORED
} State;

typedef struct
{
    char *tenant;
    char *id;
    char *title;
    int finished;
    State state;
    char *log; // log of the clone, kept only when it failed
} Repo;

// One job = one repository = one thread
typedef struct
{
    Repo *repo;
    const char *login;
    const char *clone_path;
    const char *root;
    size_t position; // unique number, names the private working folder
} Job;

// Shared by all the threads : every access is done with the lock held
typedef struct
{
    pthread_mutex_t lock;
    size_t total;
    size_t done;
    int interactive; // stdout is a terminal : the bar is redrawn in place
    int colors;
} Progress;

static Progress progress = { .lock = PTHREAD_MUTEX_INITIALIZER };

// Limits the number of clones running at the same time
static sem_t slots;

// Folder of the subject files of Jarvis, and the folder where every clone gets
// its own private copy of it
static char *real_subject_dir = NULL;
static char *private_base_dir = NULL;

typedef struct
{
    Repo *items;
    size_t count;
    size_t capacity;
} RepoList;

typedef struct
{
    char **items;
    size_t count;
    size_t capacity;
} NameList;

static char *__Duplicate(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL)
        err(EXIT_FAILURE, "malloc");
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static char *__Join(const char *parent, const char *name)
{
    size_t size = strlen(parent) + strlen(name) + 2;
    char *path = malloc(size);
    if (path == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(path, size, "%s/%s", parent, name);
    return path;
}

static char *__ReadFile(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return NULL;

    size_t capacity = 1 << 20;
    size_t length = 0;
    char *buffer = malloc(capacity + 1);
    if (buffer == NULL)
        err(EXIT_FAILURE, "malloc");

    size_t read_bytes;
    while ((read_bytes = fread(buffer + length, 1, capacity - length, file))
           > 0)
    {
        length += read_bytes;
        if (length == capacity)
        {
            capacity *= 2;
            char *bigger = realloc(buffer, capacity + 1);
            if (bigger == NULL)
                err(EXIT_FAILURE, "realloc");
            buffer = bigger;
        }
    }
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

static void __NamesAdd(NameList *list, const char *name)
{
    for (size_t i = 0; i < list->count; i++)
        if (strcmp(list->items[i], name) == 0)
            return;

    if (list->count == list->capacity)
    {
        list->capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        char **bigger = realloc(list->items, list->capacity * sizeof(char *));
        if (bigger == NULL)
            err(EXIT_FAILURE, "realloc");
        list->items = bigger;
    }
    list->items[list->count++] = __Duplicate(name, strlen(name));
}

static void __NamesFree(NameList *list)
{
    for (size_t i = 0; i < list->count; i++)
        free(list->items[i]);
    free(list->items);
}

static void __ReposAdd(RepoList *list, const char *tenant, const char *id,
                       const char *title, int finished)
{
    for (size_t i = 0; i < list->count; i++)
        if (strcmp(list->items[i].tenant, tenant) == 0
            && strcmp(list->items[i].id, id) == 0)
            return;

    if (list->count == list->capacity)
    {
        list->capacity = list->capacity == 0 ? 64 : list->capacity * 2;
        Repo *bigger = realloc(list->items, list->capacity * sizeof(Repo));
        if (bigger == NULL)
            err(EXIT_FAILURE, "realloc");
        list->items = bigger;
    }

    Repo *repo = &list->items[list->count++];
    repo->tenant = __Duplicate(tenant, strlen(tenant));
    repo->id = __Duplicate(id, strlen(id));
    repo->title = __Duplicate(title, strlen(title));
    repo->finished = finished;
    repo->state = STATE_PENDING;
    repo->log = NULL;
}

static void __ReposFree(RepoList *list)
{
    for (size_t i = 0; i < list->count; i++)
    {
        free(list->items[i].tenant);
        free(list->items[i].id);
        free(list->items[i].title);
        free(list->items[i].log);
    }
    free(list->items);
}

static int __IsOneOf(const char *argument, const char *const *names)
{
    for (int i = 0; names[i] != NULL; i++)
        if (strcasecmp(argument, names[i]) == 0)
            return 1;
    return 0;
}

// The project name ends up in a shell command (page download) and in a folder
// name : keep it strictly to letters, digits, '-', '_' and '.'. Accepts
// "/epita-ing", "epita-ing/" or the full url of the project page.
static void __ParseProject(const char *argument, char *project, size_t size)
{
    const char *start = argument;

    const char *scheme = strstr(start, "://");
    if (scheme != NULL)
    {
        start = strchr(scheme + 3, '/');
        if (start == NULL)
            errx(EXIT_FAILURE, "ERROR %s is not a project name", argument);
    }
    while (*start == '/')
        start++;

    size_t length = strcspn(start, "/");
    if (length == 0 || length >= size)
        errx(EXIT_FAILURE, "ERROR %s is not a project name", argument);

    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)start[i];
        if (!isalnum(c) && c != '-' && c != '_' && c != '.')
            errx(EXIT_FAILURE, "ERROR %s is not a project name", argument);
        project[i] = (char)tolower(c);
    }
    project[length] = '\0';

    if (project[0] == '.')
        errx(EXIT_FAILURE, "ERROR %s is not a project name", argument);
}

static Scope __ParseScope(const char *argument, char *project, size_t size)
{
    static const char *const old_words[] = { "old", "anc", "ancient", "past",
                                             NULL };
    static const char *const current_words[] = { "current", "now",  "c", "cu",
                                                 "cur",     "curr", NULL };

    if (__IsOneOf(argument, old_words))
        return SCOPE_OLD;
    if (strcasecmp(argument, "all") == 0)
        return SCOPE_ALL;
    if (__IsOneOf(argument, current_words))
        return SCOPE_CURRENT;

    __ParseProject(argument, project, size);
    return SCOPE_PROJECT;
}

// Download a page of the forge in ~/.jarvis/<file_name> and return its content
// (malloc'd), NULL if it could not be downloaded. Never stops the program.
static char *__FetchPage(const char *url, const char *file_name)
{
    char *dot_jarvis = NULL;
    if (GetDotJarvisPath(&dot_jarvis) != EXIT_SUCCESS)
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis directory");

    char *file_path = __Join(dot_jarvis, file_name);
    free(dot_jarvis);

    // Never read a page left by a previous run
    remove(file_path);

    char url_copy[FILL_BUFFER];
    char name_copy[FILL_BUFFER];
    snprintf(url_copy, sizeof(url_copy), "%s", url);
    snprintf(name_copy, sizeof(name_copy), "%s", file_name);

    int result = DowloadPage(url_copy, name_copy);
    if (result != EXIT_SUCCESS)
        result = DowloadPage(url_copy, name_copy);

    char *content = result == EXIT_SUCCESS ? __ReadFile(file_path) : NULL;
    if (content == NULL)
        UiWarning("Impossible to download the page \"%s\". [Retry or Auth "
                  "again]",
                  url);

    remove(file_path);
    free(file_path);
    return content;
}

// The "Finished" title of a page (a text node alone in its tag). Everything
// written after it is a finished activity. NULL if there is none.
static const char *__FinishedMarker(const char *page)
{
    const char *cursor = page;
    while ((cursor = strstr(cursor, "Finished")) != NULL)
    {
        const char *before = cursor;
        while (before > page && isspace((unsigned char)before[-1]))
            before--;

        const char *after = cursor + strlen("Finished");
        while (isspace((unsigned char)*after))
            after++;

        if (before > page && before[-1] == '>' && *after == '<')
            return cursor;
        cursor += strlen("Finished");
    }
    return NULL;
}

// Value of an attribute in the tag [start, end), malloc'd. NULL if absent.
static char *__TagAttribute(const char *start, const char *end,
                            const char *attribute)
{
    size_t length = (size_t)(end - start);
    if (length > FILL_BUFFER)
        return NULL;

    char tag[FILL_BUFFER + 1];
    memcpy(tag, start, length);
    tag[length] = '\0';

    char pattern[64];
    snprintf(pattern, sizeof(pattern), " %s=\"", attribute);
    char *value = strstr(tag, pattern);
    if (value == NULL)
        return NULL;
    value += strlen(pattern);

    size_t value_length = strcspn(value, "\"");
    return __Duplicate(value, value_length);
}

// Next <a class="project" href="..."> of the page. Returns its position (NULL
// if there is no more), with the href and the title (malloc'd) of the project.
static const char *__NextProject(const char *from, char **href, char **title)
{
    const char *cursor = from;
    while ((cursor = strstr(cursor, "<a ")) != NULL)
    {
        const char *end = strchr(cursor, '>');
        if (end == NULL)
            return NULL;

        size_t length = (size_t)(end - cursor);
        char tag[FILL_BUFFER + 1];
        int is_project = 0;
        if (length <= FILL_BUFFER)
        {
            memcpy(tag, cursor, length);
            tag[length] = '\0';
            is_project = strstr(tag, " class=\"project\"") != NULL;
        }

        char *value = is_project ? __TagAttribute(cursor, end, "href") : NULL;
        if (value == NULL)
        {
            cursor = end;
            continue;
        }

        *href = value;
        *title = NULL;

        // <div class="project__title">TITLE</div>, before the next anchor
        const char *next_anchor = strstr(end, "<a ");
        const char *label = strstr(end, "class=\"project__title\">");
        if (label != NULL && (next_anchor == NULL || label < next_anchor))
        {
            label += strlen("class=\"project__title\">");
            size_t label_length = strcspn(label, "<");
            while (label_length > 0 && isspace((unsigned char)label[0]))
            {
                label++;
                label_length--;
            }
            while (label_length > 0
                   && isspace((unsigned char)label[label_length - 1]))
                label_length--;
            *title = __Duplicate(label, label_length);
        }
        if (*title == NULL)
            *title = __Duplicate("", 0);

        return cursor;
    }
    return NULL;
}

// "/tenant/id/root" -> tenant, id. 0 if the href is not a repository page.
static int __SplitRepoHref(const char *href, char *tenant, char *id,
                           size_t size)
{
    char copy[FILL_BUFFER];
    snprintf(copy, sizeof(copy), "%s", href);

    char *parts[8];
    int count = 0;
    char *save = NULL;
    for (char *token = strtok_r(copy, "/", &save); token != NULL && count < 8;
         token = strtok_r(NULL, "/", &save))
        parts[count++] = token;

    if (count != 3 || strcmp(parts[2], "root") != 0 || strlen(parts[0]) >= size
        || strlen(parts[1]) >= size)
        return 0;

    snprintf(tenant, size, "%s", parts[0]);
    snprintf(id, size, "%s", parts[1]);
    return 1;
}

// Names of the projects (tenants) written on the home page : <a
// class="project" href="epita-ing">
static void __ParseTenants(const char *page, NameList *tenants)
{
    const char *cursor = page;
    char *href;
    char *title;
    while ((cursor = __NextProject(cursor, &href, &title)) != NULL)
    {
        const char *name = href;
        while (*name == '/')
            name++;

        if (*name != '\0' && strchr(name, '/') == NULL)
            __NamesAdd(tenants, name);

        free(href);
        free(title);
        cursor++;
    }
}

// Every repository listed on the page of a project, with its status
static void __ParseRepos(const char *page, RepoList *repos)
{
    const char *finished_from = __FinishedMarker(page);

    const char *cursor = page;
    char *href;
    char *title;
    while ((cursor = __NextProject(cursor, &href, &title)) != NULL)
    {
        char tenant[FILL_NAME_MAX];
        char id[FILL_NAME_MAX];
        if (__SplitRepoHref(href, tenant, id, sizeof(tenant)))
            __ReposAdd(repos, tenant, id, title,
                       finished_from != NULL && cursor > finished_from);

        free(href);
        free(title);
        cursor++;
    }
}

// The login of the connected user : <a href="/users/firstname.name">
static char *__LoginFromPage(const char *page)
{
    const char *marker = strstr(page, "href=\"/users/");
    if (marker == NULL)
        return NULL;
    marker += strlen("href=\"/users/");

    size_t length = strcspn(marker, "\"/");
    if (length == 0 || length >= FILL_NAME_MAX)
        return NULL;
    return __Duplicate(marker, length);
}

// Fallback : firstname.name@epita.fr in the git configuration
static char *__LoginFromGit(void)
{
    FILE *pipe = popen("git config --get user.email 2>/dev/null", "r");
    if (pipe == NULL)
        return NULL;

    char *login = NULL;
    char email[256];
    if (fgets(email, sizeof(email), pipe) != NULL)
    {
        email[strcspn(email, "\r\n")] = '\0';
        char *at = strchr(email, '@');
        if (at != NULL && at != email && strcmp(at, "@epita.fr") == 0)
            login = __Duplicate(email, (size_t)(at - email));
    }
    pclose(pipe);
    return login;
}

// Same rule as clone.c : <letters>-<3 digits>-<letter>-<2 digits>-<4 digits>
// (ex : prog-205-p-01-2030). Only used to tell the user where the repository
// goes, clone.c decides for real.
static int __HasCourseId(const char *id)
{
    size_t i = 0;
    while (isalpha((unsigned char)id[i]))
        i++;

    const char *p = id + i;
    return i > 0 && strlen(p) == 14 && p[0] == '-'
        && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2])
        && isdigit((unsigned char)p[3]) && p[4] == '-'
        && isalpha((unsigned char)p[5]) && p[6] == '-'
        && isdigit((unsigned char)p[7]) && isdigit((unsigned char)p[8])
        && p[9] == '-' && isdigit((unsigned char)p[10])
        && isdigit((unsigned char)p[11]) && isdigit((unsigned char)p[12])
        && isdigit((unsigned char)p[13])
        && ((p[2] - '0') * 10 + (p[3] - '0')) >= 1;
}

// login@git.forge.epita.fr:p/<tenant>/<id>/<tenant>-<id>-<login>.git
static char *__BuildGitUrl(const Repo *repo, const char *login)
{
    size_t size = 2 * strlen(login) + 2 * strlen(repo->tenant)
        + 2 * strlen(repo->id) + 64;
    char *url = malloc(size);
    if (url == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(url, size, "%s@git.forge.epita.fr:p/%s/%s/%s-%s-%s.git", login,
             repo->tenant, repo->id, repo->tenant, repo->id, login);
    return url;
}

// Does the remote exist (and can we reach it) ?
static int __RemoteExists(const char *url)
{
    pid_t pid = fork();
    if (pid < 0)
        return 0;

    if (pid == 0)
    {
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0)
        {
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
        }
        // No setenv here : this runs in a multithreaded process, so only
        // async-signal-safe calls are allowed between fork and exec
        execlp("env", "env", "GIT_SSH_COMMAND=ssh -o StrictHostKeyChecking=no",
               "git", "ls-remote", url, (char *)NULL);
        _exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Is there a folder called <name> in the tree (without entering the
// repositories) ?
static int __FindFolder(const char *directory, const char *name, int depth)
{
    DIR *dir = opendir(directory);
    if (dir == NULL)
        return 0;

    int found = 0;
    struct dirent *entry;
    while (!found && (entry = readdir(dir)) != NULL)
    {
        // Also skips ".", ".." and ".git"
        if (entry->d_name[0] == '.')
            continue;

        char *path = __Join(directory, entry->d_name);
        struct stat info;
        if (stat(path, &info) == 0 && S_ISDIR(info.st_mode))
        {
            if (strcmp(entry->d_name, name) == 0)
                found = 1;
            else if (depth > 1)
            {
                char *git_path = __Join(path, ".git");
                int is_repository = access(git_path, F_OK) == 0;
                free(git_path);
                if (!is_repository)
                    found = __FindFolder(path, name, depth - 1);
            }
        }
        free(path);
    }

    closedir(dir);
    return found;
}

// Is there, in the tree, a folder holding this repository ? It is either still
// called like the clone (<tenant>-<id>-<login>) or has been renamed by clone
// (<course core>-<Name>-<modules>, ex : 205-p-05-...), in which case it is a
// git repository. Repositories are not entered.
typedef struct
{
    const char *original; // <tenant>-<id>-<login>
    char prefix[32]; // 207-p-05-
} Target;

static int __IsTarget(const char *name, const char *path, const Target *target)
{
    if (strcmp(name, target->original) == 0)
        return 1;

    if (target->prefix[0] == '\0'
        || strncmp(name, target->prefix, strlen(target->prefix)) != 0)
        return 0;

    char *git_path = __Join(path, ".git");
    int is_repository = access(git_path, F_OK) == 0;
    free(git_path);
    return is_repository;
}

static int __ScanTree(const char *directory, const Target *target, int depth)
{
    DIR *dir = opendir(directory);
    if (dir == NULL)
        return 0;

    int found = 0;
    struct dirent *entry;
    while (!found && (entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        char *path = __Join(directory, entry->d_name);
        struct stat info;
        if (stat(path, &info) == 0 && S_ISDIR(info.st_mode))
        {
            if (__IsTarget(entry->d_name, path, target))
                found = 1;
            else if (depth > 1)
            {
                char *git_path = __Join(path, ".git");
                int is_repository = access(git_path, F_OK) == 0;
                free(git_path);
                if (!is_repository)
                    found = __ScanTree(path, target, depth - 1);
            }
        }
        free(path);
    }

    closedir(dir);
    return found;
}

// Submodules declared in the .gitmodules of the root repository (url -> path).
// Loaded once before the threads start, then only read.
typedef struct
{
    char *url;
    char *path;
} Submodule;

static Submodule *submodules = NULL;
static size_t submodule_count = 0;

static void __SubmoduleFlush(size_t *capacity, char **url, char **path)
{
    if (*url != NULL && *path != NULL)
    {
        if (submodule_count == *capacity)
        {
            *capacity = *capacity == 0 ? 64 : *capacity * 2;
            Submodule *bigger =
                realloc(submodules, *capacity * sizeof(Submodule));
            if (bigger == NULL)
                err(EXIT_FAILURE, "realloc");
            submodules = bigger;
        }
        submodules[submodule_count].url = *url;
        submodules[submodule_count].path = *path;
        submodule_count++;
    }
    else
    {
        free(*url);
        free(*path);
    }
    *url = NULL;
    *path = NULL;
}

static void __LoadSubmodules(const char *root)
{
    char *file_path = __Join(root, ".gitmodules");
    char *content = __ReadFile(file_path);
    free(file_path);
    if (content == NULL)
        return;

    size_t capacity = 0;
    char *url = NULL;
    char *path = NULL;
    char *save = NULL;
    for (char *line = strtok_r(content, "\n", &save); line != NULL;
         line = strtok_r(NULL, "\n", &save))
    {
        while (isspace((unsigned char)*line))
            line++;

        if (line[0] == '[')
        {
            __SubmoduleFlush(&capacity, &url, &path);
            continue;
        }

        char *equal = strchr(line, '=');
        if (equal == NULL)
            continue;

        // "key = value", both sides trimmed
        char *value = equal + 1;
        while (isspace((unsigned char)*value))
            value++;
        size_t value_length = strlen(value);
        while (value_length > 0
               && isspace((unsigned char)value[value_length - 1]))
            value_length--;

        size_t key_length = (size_t)(equal - line);
        while (key_length > 0 && isspace((unsigned char)line[key_length - 1]))
            key_length--;

        if (key_length == strlen("url")
            && strncmp(line, "url", key_length) == 0)
        {
            free(url);
            url = __Duplicate(value, value_length);
        }
        else if (key_length == strlen("path")
                 && strncmp(line, "path", key_length) == 0)
        {
            free(path);
            path = __Duplicate(value, value_length);
        }
    }
    __SubmoduleFlush(&capacity, &url, &path);
    free(content);
}

static void __FreeSubmodules(void)
{
    for (size_t i = 0; i < submodule_count; i++)
    {
        free(submodules[i].url);
        free(submodules[i].path);
    }
    free(submodules);
    submodules = NULL;
    submodule_count = 0;
}

// Already cloned : the url is a submodule of the root repository and its folder
// is still there (this does not depend on the info file), or it is registered
// in the info file and its folder is still in the tree.
static int __AlreadyCloned(const Repo *repo, const char *login, const char *url,
                           const char *root)
{
    if (root == NULL)
        return 0;

    // 1. Look at the tree itself, whatever the info file says
    char original[FILL_BUFFER];
    snprintf(original, sizeof(original), "%s-%s-%s", repo->tenant, repo->id,
             login);

    Target target = { .original = original };
    target.prefix[0] = '\0';
    if (__HasCourseId(repo->id))
    {
        const char *p = repo->id;
        while (isalpha((unsigned char)*p))
            p++;
        // -207-p-05-2029 -> 207-p-05-
        snprintf(target.prefix, sizeof(target.prefix), "%.3s-%c-%.2s-", p + 1,
                 tolower((unsigned char)p[5]), p + 7);
    }
    if (__ScanTree(root, &target, FILL_SEARCH_DEPTH))
        return 1;

    // 2. Registered submodule whose folder is still there

    for (size_t i = 0; i < submodule_count; i++)
    {
        if (strcmp(submodules[i].url, url) != 0)
            continue;

        char *full_path = __Join(root, submodules[i].path);
        struct stat info;
        int is_folder = stat(full_path, &info) == 0 && S_ISDIR(info.st_mode);
        free(full_path);
        if (is_folder)
            return 1;
    }

    char key[FILL_BUFFER];
    snprintf(key, sizeof(key), "%s", url);

    char *name = NULL;
    if (ReadInfo(key, &name) != EXIT_SUCCESS || name == NULL)
        return 0;

    int found = __FindFolder(root, name, FILL_SEARCH_DEPTH);
    free(name);
    return found;
}

// Progress bar : [██████░░░░░░░░░░░░░░░░░░░░░░░░] 12/40 (30%)
// The messages are printed above the bar, which is always the last line.

// The lock must be held
static void __DrawBar(void)
{
    if (!progress.interactive)
        return;

    size_t filled = progress.total == 0
        ? 0
        : progress.done * FILL_BAR_WIDTH / progress.total;
    unsigned percent = progress.total == 0
        ? 0
        : (unsigned)(progress.done * 100 / progress.total);

    printf("\r\033[2K%s[", progress.colors ? "\033[32m" : "");
    for (size_t i = 0; i < FILL_BAR_WIDTH; i++)
        printf("%s", i < filled ? "█" : "░");
    printf("]%s %zu/%zu (%u%%)", progress.colors ? "\033[0m" : "",
           progress.done, progress.total, percent);
    fflush(stdout);
}

// Print one line above the bar. The lock must be held.
static void __PrintLine(const char *color, const char *symbol, const char *fmt,
                        ...)
{
    if (progress.interactive)
        printf("\r\033[2K");

    printf("%s%s%s ", progress.colors ? color : "", symbol,
           progress.colors ? "\033[0m" : "");

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    __DrawBar();
    fflush(stdout);
}

// A repository is processed : store its state, advance the bar, say what
// happened. Called by the thread of the repository.
static void __Finish(Job *job, State state)
{
    Repo *repo = job->repo;
    const char *label = repo->title[0] != '\0' ? repo->title : repo->id;

    pthread_mutex_lock(&progress.lock);
    repo->state = state;
    progress.done++;

    switch (state)
    {
    case STATE_CLONED:
        __PrintLine("\033[32m", "✔", "%s", label);
        break;
    case STATE_ALREADY:
        __PrintLine("\033[90m", "•", "%s (already cloned)", label);
        break;
    case STATE_NO_REMOTE:
        __PrintLine("\033[33m", "!", "%s (no repository found for you)", label);
        break;
    default:
        __PrintLine("\033[31m", "✘", "%s (failed%s%s)", label,
                    repo->log != NULL ? ", log : " : "",
                    repo->log != NULL ? repo->log : "");
        break;
    }

    // Without a terminal there is no bar : give the progress in the line
    if (!progress.interactive)
        printf("[%zu/%zu]\n", progress.done, progress.total);
    fflush(stdout);
    pthread_mutex_unlock(&progress.lock);
}

// ---------------------------------------------------------------------------
// Private working folder of a clone
//
// clone writes subject.html and the given files in the subject folder of
// Jarvis : two clones running at the same time would overwrite each other. So
// every clone gets its own folder (JARVIS_SUBJECT_DIR) holding links to the
// files it needs (setup_get_subject, auth.json...), but not to the files that
// the clone produces.
// ---------------------------------------------------------------------------

static int __RemoveEntry(const char *path, const struct stat *info, int flag,
                         struct FTW *position)
{
    (void)info;
    (void)flag;
    (void)position;
    return remove(path);
}

// rm -rf, links are removed and never followed
static void __RemoveTree(const char *path)
{
    nftw(path, __RemoveEntry, 16, FTW_DEPTH | FTW_PHYS);
}

// Files written by a clone : never shared
static int __IsProducedFile(const char *name)
{
    static const char *const words[] = { "temp", "tmp", "given", NULL };
    static const char *const suffixes[] = { ".html", ".tar", ".gz",  ".tgz",
                                            ".zip",  ".xz",  ".bz2", NULL };

    for (int i = 0; words[i] != NULL; i++)
        if (strcasestr(name, words[i]) != NULL)
            return 1;

    size_t length = strlen(name);
    for (int i = 0; suffixes[i] != NULL; i++)
    {
        size_t suffix_length = strlen(suffixes[i]);
        if (length >= suffix_length
            && strcasecmp(name + length - suffix_length, suffixes[i]) == 0)
            return 1;
    }
    return 0;
}

static int __PrepareSubjectDir(size_t position, char *path, size_t size)
{
    snprintf(path, size, "%s/%zu", private_base_dir, position);

    __RemoveTree(path);
    if (mkdir(path, 0755) != 0)
        return EXIT_FAILURE;

    DIR *dir = opendir(real_subject_dir);
    if (dir == NULL)
        return EXIT_FAILURE;

    int result = EXIT_SUCCESS;
    struct dirent *entry;
    while (result == EXIT_SUCCESS && (entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
            || __IsProducedFile(entry->d_name))
            continue;

        char *source = __Join(real_subject_dir, entry->d_name);
        struct stat info;
        if (stat(source, &info) == 0 && S_ISREG(info.st_mode))
        {
            char *destination = __Join(path, entry->d_name);
            if (symlink(source, destination) != 0)
                result = EXIT_FAILURE;
            free(destination);
        }
        free(source);
    }

    closedir(dir);
    return result;
}

// Run "clone <url>" without any question, in its own working folder. Its output
// goes in a log file (several clones writing in the terminal would be
// unreadable and would break the bar) : the log is kept only when it fails.
static int __RunClone(const Job *job, const char *url)
{
    Repo *repo = job->repo;

    char subject_dir[FILL_BUFFER];
    if (__PrepareSubjectDir(job->position, subject_dir, sizeof(subject_dir))
        != EXIT_SUCCESS)
    {
        __RemoveTree(subject_dir);
        return EXIT_FAILURE;
    }

    char log_path[] = "/tmp/jarvis-fill-XXXXXX";
    int log_fd = mkostemp(log_path, O_CLOEXEC);

    char project_variable[FILL_BUFFER];
    char subject_variable[FILL_BUFFER + 32];
    snprintf(project_variable, sizeof(project_variable), "JARVIS_PROJECT=%s",
             repo->tenant);
    snprintf(subject_variable, sizeof(subject_variable),
             "JARVIS_SUBJECT_DIR=%s", subject_dir);

    int result = EXIT_FAILURE;
    pid_t pid = fork();
    if (pid == 0)
    {
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0)
            dup2(null_fd, STDIN_FILENO);

        int out_fd = log_fd >= 0 ? log_fd : open("/dev/null", O_WRONLY);
        if (out_fd >= 0)
        {
            dup2(out_fd, STDOUT_FILENO);
            dup2(out_fd, STDERR_FILENO);
        }

        // env instead of setenv : only async-signal-safe calls after a fork
        // in a multithreaded process
        execlp("env", "env", "JARVIS_FILL=1", project_variable,
               subject_variable, job->clone_path, url, (char *)NULL);
        _exit(127);
    }

    if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) >= 0 && WIFEXITED(status)
            && WEXITSTATUS(status) == 0)
            result = EXIT_SUCCESS;
    }

    if (log_fd >= 0)
        close(log_fd);

    if (log_fd >= 0 && result == EXIT_SUCCESS)
        unlink(log_path);
    else if (log_fd >= 0)
        repo->log = __Duplicate(log_path, strlen(log_path));

    __RemoveTree(subject_dir);
    return result;
}

// ---------------------------------------------------------------------------
// One thread per repository
// ---------------------------------------------------------------------------

static void __Process(Job *job)
{
    char *url = __BuildGitUrl(job->repo, job->login);

    if (__AlreadyCloned(job->repo, job->login, url, job->root))
        __Finish(job, STATE_ALREADY);
    else if (!__RemoteExists(url))
        __Finish(job, STATE_NO_REMOTE);
    else if (__RunClone(job, url) == EXIT_SUCCESS)
        __Finish(job, STATE_CLONED);
    else
        __Finish(job, STATE_FAILED);

    free(url);
}

static void *__Worker(void *argument)
{
    // A thread exists for every repository, but only a few of them work at the
    // same time (the forge and the machine do not like 100 simultaneous
    // connections)
    while (sem_wait(&slots) != 0 && errno == EINTR)
        ;
    __Process(argument);
    sem_post(&slots);
    return NULL;
}

// Number of simultaneous clones : JARVIS_FILL_JOBS, 8 by default
static unsigned __JobsLimit(void)
{
    const char *value = getenv("JARVIS_FILL_JOBS");
    if (value == NULL || value[0] == '\0')
        return FILL_DEFAULT_JOBS;

    char *end = NULL;
    long jobs = strtol(value, &end, 10);
    if (*end != '\0' || jobs < 1)
        return FILL_DEFAULT_JOBS;
    return jobs > FILL_MAX_JOBS ? FILL_MAX_JOBS : (unsigned)jobs;
}

static int __IsSelected(const Repo *repo, Scope scope)
{
    switch (scope)
    {
    case SCOPE_OLD:
        return repo->finished;
    case SCOPE_CURRENT:
        return !repo->finished;
    default:
        return 1;
    }
}

static const char *__ScopeLabel(Scope scope)
{
    switch (scope)
    {
    case SCOPE_OLD:
        return "finished";
    case SCOPE_CURRENT:
        return "not finished";
    case SCOPE_ALL:
        return "past and current";
    default:
        return "listed on the project page";
    }
}

int main(int argc, char *argv[])
{
    if (argc > 2)
        errx(EXIT_FAILURE,
             "ERROR Too many arguments. Usage : jarvis fill [old | all | "
             "current | <project>]");

    // Ask once if the user accepts to be counted (GitHub star)
    CounterAsk();

    Scope scope = SCOPE_OLD;
    char project[FILL_NAME_MAX] = "";
    if (argc == 2)
        scope = __ParseScope(argv[1], project, sizeof(project));

    // Home page : the login of the user and the list of his projects
    UiInfo("Reading your projects on the forge...");
    char *home = __FetchPage("", "FillHome.html");
    if (home == NULL)
        errx(EXIT_FAILURE,
             "ERROR Impossible to read the forge home page. [Retry or Auth "
             "again]");

    char *login = __LoginFromPage(home);
    if (login == NULL)
        login = __LoginFromGit();
    if (login == NULL)
    {
        free(home);
        errx(EXIT_FAILURE, "ERROR Impossible to find your EPITA login");
    }

    NameList tenants = { 0 };
    if (scope == SCOPE_PROJECT)
    {
        NameList known = { 0 };
        __ParseTenants(home, &known);

        int is_known = 0;
        for (size_t i = 0; i < known.count; i++)
            if (strcmp(known.items[i], project) == 0)
                is_known = 1;
        if (!is_known)
            UiWarning("%s is not one of your projects on the home page, "
                      "trying anyway.",
                      project);

        __NamesFree(&known);
        __NamesAdd(&tenants, project);
    }
    else
        __ParseTenants(home, &tenants);
    free(home);

    if (tenants.count == 0)
        errx(EXIT_FAILURE, "ERROR No project found on the home page");

    // Page of every project : its repositories, ongoing then finished
    RepoList repos = { 0 };
    for (size_t i = 0; i < tenants.count; i++)
    {
        char file_name[FILL_NAME_MAX];
        snprintf(file_name, sizeof(file_name), "FillProject%zu.html", i);

        UiInfo("Reading the project %s...", tenants.items[i]);
        char *page = __FetchPage(tenants.items[i], file_name);
        if (page == NULL)
            continue;

        __ParseRepos(page, &repos);
        free(page);
    }
    __NamesFree(&tenants);

    if (scope == SCOPE_PROJECT && repos.count == 0)
        errx(EXIT_FAILURE, "ERROR No repository found for the project %s",
             project);

    size_t total = 0;
    for (size_t i = 0; i < repos.count; i++)
    {
        if (__IsSelected(&repos.items[i], scope))
            total++;
        else
            repos.items[i].state = STATE_IGNORED;
    }

    if (total == 0)
    {
        UiInfo("No %s repository found, nothing to clone.",
               __ScopeLabel(scope));
        __ReposFree(&repos);
        free(login);
        return EXIT_SUCCESS;
    }

    UiInfo("%zu %s repositories found.", total, __ScopeLabel(scope));

    char clone_path[FILL_BUFFER];
    snprintf(clone_path, sizeof(clone_path), "%s/clone", GetCommandPath());
    if (access(clone_path, X_OK) != 0)
        errx(EXIT_FAILURE,
             "ERROR Impossible to find the clone command (%s), try to install "
             "again.",
             clone_path);

    // The clone command marks each repository as the current one : keep the
    // real one
    char *previous_current = NULL;
    if (ReadInfo("current", &previous_current) != EXIT_SUCCESS)
        previous_current = NULL;

    // The root repository must exist before the clones start : they would all
    // try to create it at the same time otherwise
    char *root = NULL;
    if (ReadRootPath(&root) != EXIT_SUCCESS || root == NULL)
        root = GetOrCreateRepoRoot();
    if (root == NULL)
        errx(EXIT_FAILURE,
             "ERROR Impossible to find or create the root "
             "repository");

    // What the root repository already contains (read once, before the threads)
    __LoadSubmodules(root);

    // Folders of the subject files : the real one, and where each clone gets
    // its private copy
    const char *subject_dir = GetSubjectFolderPath();
    real_subject_dir = realpath(subject_dir, NULL);
    if (real_subject_dir == NULL)
        real_subject_dir = __Duplicate(subject_dir, strlen(subject_dir));

    char *dot_jarvis = NULL;
    if (GetDotJarvisPath(&dot_jarvis) != EXIT_SUCCESS)
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis directory");
    private_base_dir = __Join(dot_jarvis, "fill");
    free(dot_jarvis);
    __RemoveTree(private_base_dir);
    if (mkdir(private_base_dir, 0755) != 0 && errno != EEXIST)
        err(EXIT_FAILURE, "mkdir(%s)", private_base_dir);

    unsigned jobs_limit = __JobsLimit();
    if (sem_init(&slots, 0, jobs_limit) != 0)
        err(EXIT_FAILURE, "sem_init");

    progress.total = total;
    progress.done = 0;
    progress.interactive = isatty(STDOUT_FILENO);
    progress.colors = progress.interactive && getenv("NO_COLOR") == NULL;

    UiInfo("Cloning %zu repositories (%u at the same time).", total,
           jobs_limit);

    Job *jobs = calloc(total, sizeof(Job));
    pthread_t *threads = calloc(total, sizeof(pthread_t));
    int *started = calloc(total, sizeof(int));
    if (jobs == NULL || threads == NULL || started == NULL)
        err(EXIT_FAILURE, "calloc");

    pthread_mutex_lock(&progress.lock);
    __DrawBar();
    pthread_mutex_unlock(&progress.lock);

    // One thread per repository
    size_t launched = 0;
    for (size_t i = 0; i < repos.count; i++)
    {
        if (repos.items[i].state == STATE_IGNORED)
            continue;

        Job *job = &jobs[launched];
        job->repo = &repos.items[i];
        job->login = login;
        job->clone_path = clone_path;
        job->root = root;
        job->position = launched;

        if (pthread_create(&threads[launched], NULL, __Worker, job) == 0)
            started[launched] = 1;
        else
            __Worker(job); // No thread available : do it here
        launched++;
    }

    for (size_t i = 0; i < launched; i++)
        if (started[i])
            pthread_join(threads[i], NULL);

    // Leave the bar (100%) on its line
    pthread_mutex_lock(&progress.lock);
    __DrawBar();
    if (progress.interactive)
        printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&progress.lock);

    sem_destroy(&slots);
    free(started);
    free(threads);
    free(jobs);
    free(root);
    __FreeSubmodules();
    __RemoveTree(private_base_dir);
    free(private_base_dir);
    free(real_subject_dir);

    if (previous_current != NULL)
    {
        WriteInfo("current", previous_current);
        free(previous_current);
    }

    size_t counts[6] = { 0 };
    for (size_t i = 0; i < repos.count; i++)
        counts[repos.items[i].state]++;

    // One push of the root repository for all the clones
    if (counts[STATE_CLONED] > 0)
    {
        char *root = NULL;
        if (ReadRootPath(&root) == EXIT_SUCCESS && root != NULL)
        {
            if (chdir(root) != 0)
                UiWarning("Impossible to go in %s.", root);

            char commit_name[FILL_NAME_MAX];
            snprintf(commit_name, sizeof(commit_name), "Fill %zu repositories",
                     counts[STATE_CLONED]);

            if (AddRepoRoot() != EXIT_SUCCESS
                || PushRepoRoot(commit_name) != EXIT_SUCCESS)
                UiWarning("The root repository could not be pushed, run "
                          "'jarvis push root'.");
        }
        free(root);
    }

    char *report = NULL;
    size_t report_size = 0;
    FILE *stream = open_memstream(&report, &report_size);
    if (stream != NULL)
    {
        fprintf(stream,
                "%zu cloned\n%zu already cloned\n%zu without "
                "repository for you\n%zu failed",
                counts[STATE_CLONED], counts[STATE_ALREADY],
                counts[STATE_NO_REMOTE], counts[STATE_FAILED]);

        for (size_t i = 0; i < repos.count; i++)
        {
            if (repos.items[i].state != STATE_FAILED)
                continue;
            fprintf(stream, "\nfailed : %s/%s%s", repos.items[i].tenant,
                    repos.items[i].id,
                    __HasCourseId(repos.items[i].id) ? "" : " (Others)");
            if (repos.items[i].log != NULL)
                fprintf(stream, "\n  log : %s", repos.items[i].log);
        }
        fclose(stream);

        UiPanel(stdout, counts[STATE_FAILED] > 0 ? UI_WARNING : UI_SUCCESS,
                "Fill", "%s", report);
        free(report);
    }

    int result = counts[STATE_FAILED] > 0 ? EXIT_FAILURE : EXIT_SUCCESS;

    __ReposFree(&repos);
    free(login);
    return result;
}