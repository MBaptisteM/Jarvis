#include "pull.h"


int main(int argc, char *argv[])
{
    // Ask once if the user accepts to be counted (GitHub star)
    CounterAsk();

    // Case main (or no argument): pull the root folder and every submodule
    if (argc < 2 || strcasecmp(argv[1], "all") == 0)
    {
        __PullMain();
        return EXIT_SUCCESS;
    }

    // Case current: pull the repository saved as "current" in the logs
    if (IsCurrentKeyword(argv[1]))
    {
        __PullCurrent();
        return EXIT_SUCCESS;
    }

    // Case a specific git repo: pull it if it has already been cloned
    __PullRepository(argv[1]);

    return EXIT_SUCCESS;
}

static int __IsGitRepositoryPath(const char *path)
{
    size_t git_path_size = strlen(path) + sizeof("/.git");
    char *git_path = malloc(git_path_size);
    if (git_path == NULL)
        err(EXIT_FAILURE, "malloc");

    snprintf(git_path, git_path_size, "%s/.git", path);
    int is_git_repository = access(git_path, F_OK) == 0;
    free(git_path);
    return is_git_repository;
}

static char *__FindRootRepository(char *recorded_path)
{
    if (__IsGitRepositoryPath(recorded_path))
        return recorded_path;

    size_t repo_path_size = strlen(recorded_path) + sizeof("/EPITA-TPs");
    char *repo_path = malloc(repo_path_size);
    if (repo_path == NULL)
        err(EXIT_FAILURE, "malloc");

    snprintf(repo_path, repo_path_size, "%s/EPITA-TPs", recorded_path);
    if (__IsGitRepositoryPath(repo_path))
    {
        UiWarning("Correcting the saved root repository path to %s", repo_path);
        WriteInfo("main_path", repo_path);
        free(recorded_path);
        return repo_path;
    }

    free(repo_path);
    return recorded_path;
}

// Shell script (run from the repository root) that repairs gitlinks present in
// the index but missing from .gitmodules, which makes every "git submodule"
// command fail with "No url found for submodule path". A nested repo that has
// a remote gets registered back in .gitmodules, anything else is removed from
// the index.
static const char *HEAL_SUBMODULES_SCRIPT =
    "git ls-files --stage | awk -F'\\t' '$1 ~ /^160000/ {print $2}' | "
    "while IFS= read -r p; do "
    "git config -f .gitmodules --get-regexp '^submodule\\..*\\.path$' "
    "2>/dev/null | sed 's/^[^ ]* //' | grep -qxF -- \"$p\" && continue; "
    "url=''; "
    "if [ -e \"$p/.git\" ]; then "
    "url=$(git -C \"$p\" remote get-url origin 2>/dev/null); fi; "
    "if [ -n \"$url\" ]; then "
    "echo \"Jarvis: registering submodule $p in .gitmodules\"; "
    "git config -f .gitmodules \"submodule.$p.path\" \"$p\" && "
    "git config -f .gitmodules \"submodule.$p.url\" \"$url\" && "
    "git add .gitmodules; "
    "else "
    "echo \"Jarvis: removing orphan submodule entry $p (no url, no git "
    "repo)\"; "
    "git rm --cached -q -f -- \"$p\"; "
    "fi; done; true";

static void __HealSubmodules(const char *repo_path)
{
    size_t command_size =
        strlen(repo_path) + strlen(HEAL_SUBMODULES_SCRIPT) + 16;
    char *command = malloc(command_size);
    if (command == NULL)
        err(EXIT_FAILURE, "malloc");

    snprintf(command, command_size, "cd \"%s\" && %s", repo_path,
             HEAL_SUBMODULES_SCRIPT);
    if (system(command) != 0)
        UiWarning("Impossible to check the submodules of %s.", repo_path);
    free(command);
}

// Pull the root repository and all its submodules
void __PullMain(void)
{
    char *root_folder;
    if (ReadRootPath(&root_folder))
        errx(EXIT_FAILURE,
             "ERROR Impossible to find the root repository (did you clone "
             "anything yet?)");

    root_folder = __FindRootRepository(root_folder);
    if (!__IsGitRepositoryPath(root_folder))
        errx(EXIT_FAILURE,
             "ERROR The saved root path %s is not a Git repository. "
             "Run 'jarvis auth' or 'jarvis clone all' to locate it again.",
             root_folder);

    UiPanel(stdout, UI_INFO, "Pull",
            "Pulling all repositories starting from:\n%s", root_folder);

    __PullRepo(root_folder);

    free(root_folder);
}

// Pull the repository saved as "current"
void __PullCurrent(void)
{
    char *path;
    if (ReadInfo("current", &path))
        errx(EXIT_FAILURE,
             "ERROR Trying to pull the current repository but no current "
             "repository found");

    char *recorded_current = strdup(path);
    if (recorded_current == NULL)
        err(EXIT_FAILURE, "strdup");

    char *root_folder = NULL;
    int current_was_root = 0;
    if (ReadRootPath(&root_folder) == 0)
    {
        current_was_root = strcmp(path, root_folder) == 0;
        root_folder = __FindRootRepository(root_folder);
        if (!current_was_root && __IsGitRepositoryPath(root_folder))
        {
            size_t nested_root_size = strlen(path) + sizeof("/EPITA-TPs");
            char *nested_root = malloc(nested_root_size);
            if (nested_root == NULL)
                err(EXIT_FAILURE, "malloc");
            snprintf(nested_root, nested_root_size, "%s/EPITA-TPs", path);
            current_was_root = strcmp(nested_root, root_folder) == 0;
            free(nested_root);
        }
    }

    if (!__IsGitRepositoryPath(path) && current_was_root && root_folder != NULL
        && __IsGitRepositoryPath(root_folder))
    {
        free(path);
        path = strdup(root_folder);
        if (path == NULL)
            err(EXIT_FAILURE, "strdup");
        WriteInfo("current", path);
    }
    free(recorded_current);
    free(root_folder);

    // Get the repo name from the path
    char repo_name[SIZE_OF_STRING];
    int i = 0;
    char *c = path;
    while (*c != 0)
    {
        if (*c != '/')
            repo_name[i++] = *c;
        else
            i = 0;
        c++;
    }
    repo_name[i] = 0;

    // Get the path again if the folder moved
    if (access(path, F_OK) != 0)
    {
        free(path);
        path = NULL;
        char *parent_path = FindFileBFS(repo_name);
        if (parent_path != NULL)
        {
            size_t path_size = strlen(parent_path) + strlen(repo_name) + 2;
            path = malloc(path_size);
            if (path == NULL)
            {
                free(parent_path);
                err(EXIT_FAILURE, "malloc");
            }
            snprintf(path, path_size, "%s/%s", parent_path, repo_name);
            free(parent_path);
        }

        // Fallback: if the stored value was actually the raw git remote
        // (e.g. left over from a failed rename in clone.c), the folder on
        // disk was created by "git submodule add" using the same last
        // path segment but WITHOUT the trailing ".git". Retry with that.
        if (path == NULL)
        {
            size_t len = strlen(repo_name);
            if (len > 4 && strcmp(repo_name + len - 4, ".git") == 0)
            {
                repo_name[len - 4] = 0;
                char *parent_path = FindFileBFS(repo_name);
                if (parent_path != NULL)
                {
                    size_t path_size =
                        strlen(parent_path) + strlen(repo_name) + 2;
                    path = malloc(path_size);
                    if (path == NULL)
                    {
                        free(parent_path);
                        err(EXIT_FAILURE, "malloc");
                    }
                    snprintf(path, path_size, "%s/%s", parent_path, repo_name);
                    free(parent_path);
                }
            }
        }

        if (path == NULL)
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the current repository %s locally "
                 "(try to re-clone it)",
                 repo_name);
    }
    else if (!__IsGitRepositoryPath(path))
    {
        errx(EXIT_FAILURE, "ERROR The current path %s is not a Git repository.",
             path);
    }

    __PullRepo(path);

    // Keep the current repository up to date in the logs
    WriteInfo("current", path);

    free(path);
}

// Pull one specific repository, given its git remote (must already be cloned)
void __PullRepository(char *repo_arg)
{
    // Get the relative repo name saved in the info files (same pattern as
    // clone.c)
    char *repo_name;
    if (ReadInfo(repo_arg, &repo_name))
        errx(EXIT_FAILURE,
             "ERROR This repository has not been cloned yet, use 'jarvis "
             "clone' first.");

    // Get the root repository path to rebuild the full path, like clone.c does
    char *main_folder_path;
    if (ReadRootPath(&main_folder_path))
        errx(EXIT_FAILURE,
             "ERROR Impossible to find the root repository (did you clone "
             "anything yet?)");

    size_t repo_path_size = strlen(main_folder_path) + strlen(repo_name) + 2;
    char repo_path[repo_path_size];
    snprintf(repo_path, repo_path_size, "%s%s", main_folder_path, repo_name);

    // Get the path again if the folder moved
    if (access(repo_path, F_OK) != 0)
    {
        char *found_path = FindFileBFS(repo_name);
        if (found_path == NULL)
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the repository %s locally (try to "
                 "re-clone it)",
                 repo_name);

        // FindFileBFS returns the parent folder of what was found
        size_t found_size = strlen(found_path) + strlen(repo_name) + 2;
        char *found_repo = malloc(found_size);
        if (found_repo == NULL)
            err(EXIT_FAILURE, "malloc");
        snprintf(found_repo, found_size, "%s/%s", found_path, repo_name);
        free(found_path);

        __PullRepo(found_repo);

        // Save this repository as the current one, same pattern as the other
        // commands
        WriteInfo("current", found_repo);

        free(found_repo);
    }
    else
    {
        __PullRepo(repo_path);

        // Save this repository as the current one, same pattern as the other
        // commands
        WriteInfo("current", repo_path);
    }

    free(repo_name);
    free(main_folder_path);
}


static unsigned __PullJobs(void)
{
    const char *value = getenv("JARVIS_JOBS");
    if (value == NULL || value[0] == '\0')
        return PULL_DEFAULT_JOBS;

    char *end = NULL;
    long jobs = strtol(value, &end, 10);
    if (*end != '\0' || jobs < 1)
        return PULL_DEFAULT_JOBS;
    return jobs > PULL_MAX_JOBS ? PULL_MAX_JOBS : (unsigned)jobs;
}

// Whole content of a log file (malloc'd), NULL if empty
static char *__ReadLog(int fd)
{
    struct stat info;
    if (fstat(fd, &info) != 0 || info.st_size <= 0)
        return NULL;

    size_t size = (size_t)info.st_size;
    char *buffer = malloc(size + 1);
    if (buffer == NULL)
        return NULL;

    size_t got = 0;
    while (got < size)
    {
        ssize_t n = pread(fd, buffer + got, size - got, (off_t)got);
        if (n <= 0)
            break;
        got += (size_t)n;
    }
    while (got > 0 && (buffer[got - 1] == '\n' || buffer[got - 1] == '\r'))
        got--;
    buffer[got] = '\0';
    return buffer;
}

// Pull one submodule (and its own submodules). Its output is kept in a log
// so that several pulls do not mix their lines in the terminal.
static int __RunPull(const PullJob *job, char **log)
{
    char log_path[] = "/tmp/jarvis-pull-XXXXXX";
    int log_fd = mkstemp(log_path);

    pid_t pid = fork();
    if (pid == 0)
    {
        // Only async-signal-safe calls between fork and exec (multithreaded
        // process)
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0)
            dup2(null_fd, STDIN_FILENO);

        int out_fd = log_fd >= 0 ? log_fd : open("/dev/null", O_WRONLY);
        if (out_fd >= 0)
        {
            dup2(out_fd, STDOUT_FILENO);
            dup2(out_fd, STDERR_FILENO);
        }

        if (chdir(job->repo_path) != 0)
            _exit(126);
        execlp("sh", "sh", "-c", PULL_SUBMODULE_SCRIPT, "sh", job->path,
               (char *)NULL);
        _exit(127);
    }

    int result = EXIT_FAILURE;
    if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) >= 0 && WIFEXITED(status)
            && WEXITSTATUS(status) == 0)
            result = EXIT_SUCCESS;
    }

    if (log_fd >= 0)
    {
        *log = __ReadLog(log_fd);
        close(log_fd);
        unlink(log_path);
    }
    return result;
}

// Live progress line (bottom of the terminal, redrawn in place). Both
// functions must be called with pull_lock held.
static void __ProgressClear(void)
{
    if (pull_tty)
    {
        fputs("\r\033[K", stderr);
        fflush(stderr);
    }
}

static void __ProgressDraw(void)
{
    if (!pull_tty || pull_total == 0)
        return;

    enum { BAR = 20 };
    size_t filled = pull_done * BAR / pull_total;
    char bar[BAR * 3 + 1];
    size_t used = 0;
    for (size_t i = 0; i < BAR; i++)
    {
        const char *cell = i < filled ? "█" : "░";
        memcpy(bar + used, cell, 3);
        used += 3;
    }
    bar[used] = '\0';

    fprintf(stderr, "\r\033[K[%zu/%zu] %s %3zu%%  %zu running", pull_done,
            pull_total, bar, pull_done * 100 / pull_total, pull_running);
    if (pull_problems > 0)
        fprintf(stderr, "  %zu issue(s)", pull_problems);
    if (pull_last[0] != '\0' && pull_done < pull_total)
        fprintf(stderr, "  last: %.40s", pull_last);
    fflush(stderr);
}

// Show the Jarvis messages of one submodule with the Ui panels. Only the lines
// written by the script (tagged "X:") are kept, anything else is ignored.
// Returns 1 if there was a warning or an error.
static int __ShowMessages(const PullJob *job, const char *log, int failed)
{
    size_t size = (log != NULL ? strlen(log) : 0) + strlen(job->path) + 32;
    char *messages = calloc(size, 1);
    if (messages == NULL)
        return failed;

    int has_error = failed;
    int has_warning = 0;
    int has_success = 0;
    int count = 0;
    size_t used = 0;

    for (const char *line = log; line != NULL && *line != '\0';)
    {
        size_t length = strcspn(line, "\n");
        if (length > 2 && line[1] == ':' && strchr("IWSE", line[0]) != NULL)
        {
            if (line[0] == 'E')
                has_error = 1;
            else if (line[0] == 'W')
                has_warning = 1;
            else if (line[0] == 'S')
            {
                has_success = 1;
                has_warning = 0; // The resync fixed the stale commit
            }

            if (count++ > 0)
                messages[used++] = '\n';
            memcpy(messages + used, line + 2, length - 2);
            used += length - 2;
        }
        line += length + (line[length] == '\n' ? 1 : 0);
    }
    messages[used] = '\0';

    if (failed && count == 0)
        snprintf(messages, size, "%s: pull failed.", job->path);

    UiKind kind = has_error ? UI_ERROR
        : has_warning       ? UI_WARNING
        : has_success       ? UI_SUCCESS
                            : UI_INFO;
    int problem = has_error || has_warning;

    if (messages[0] != '\0')
    {
        pthread_mutex_lock(&pull_lock);
        __ProgressClear(); // the panel is printed above the progress line
        UiPanel(kind == UI_ERROR || kind == UI_WARNING ? stderr : stdout, kind,
                NULL, "[%zu/%zu] %s", pull_done, pull_total, messages);
        pthread_mutex_unlock(&pull_lock);
    }

    free(messages);
    return problem;
}

static void *__PullWorker(void *argument)
{
    PullJob *job = argument;

    // One thread per submodule, but only a few pull at the same time
    while (sem_wait(&pull_slots) != 0 && errno == EINTR)
        ;
    pthread_mutex_lock(&pull_lock);
    pull_running++;
    __ProgressDraw();
    pthread_mutex_unlock(&pull_lock);

    char *log = NULL;
    int result = __RunPull(job, &log);
    sem_post(&pull_slots);

    pthread_mutex_lock(&pull_lock);
    pull_running--;
    pull_done++;
    pull_last = job->path;
    if (pull_tty)
        __ProgressDraw();
    else
        printf("[%zu/%zu] %s\n", pull_done, pull_total, job->path);
    pthread_mutex_unlock(&pull_lock);

    int problem = __ShowMessages(job, log, result != EXIT_SUCCESS);
    pthread_mutex_lock(&pull_lock);
    if (problem)
        pull_problems++;
    __ProgressDraw();
    pthread_mutex_unlock(&pull_lock);

    free(log);
    return NULL;
}

static void __PullSubmodules(const char *repo_path)
{
    // Top-level submodules only (the nested ones are pulled by their parent)
    size_t command_size = strlen(repo_path) + 128;
    char *command = malloc(command_size);
    if (command == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(command, command_size,
             "git -C \"%s\" submodule foreach --quiet 'echo \"$sm_path\"'",
             repo_path);

    FILE *pipe = popen(command, "r");
    free(command);
    if (pipe == NULL)
    {
        UiWarning("Impossible to list the submodules of %s.", repo_path);
        return;
    }

    char **paths = NULL;
    size_t count = 0;
    size_t capacity = 0;
    char line[4096];
    while (fgets(line, sizeof(line), pipe) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0')
            continue;

        if (count == capacity)
        {
            capacity = capacity == 0 ? 32 : capacity * 2;
            char **bigger = realloc(paths, capacity * sizeof(char *));
            if (bigger == NULL)
                err(EXIT_FAILURE, "realloc");
            paths = bigger;
        }
        paths[count] = strdup(line);
        if (paths[count] == NULL)
            err(EXIT_FAILURE, "strdup");
        count++;
    }
    pclose(pipe);

    if (count == 0)
    {
        free(paths);
        return;
    }

    unsigned jobs_limit = __PullJobs();
    if (sem_init(&pull_slots, 0, jobs_limit) != 0)
        err(EXIT_FAILURE, "sem_init");

    pull_total = count;
    pull_done = 0;
    pull_problems = 0;
    pull_running = 0;
    pull_last = "";
    pull_tty = isatty(STDERR_FILENO) && getenv("TERM") != NULL
        && strcmp(getenv("TERM"), "dumb") != 0;
    UiInfo("Pulling %zu submodules (%u at the same time)...", count,
           jobs_limit);
    pthread_mutex_lock(&pull_lock);
    __ProgressDraw();
    pthread_mutex_unlock(&pull_lock);

    PullJob *jobs = calloc(count, sizeof(PullJob));
    pthread_t *threads = calloc(count, sizeof(pthread_t));
    int *started = calloc(count, sizeof(int));
    if (jobs == NULL || threads == NULL || started == NULL)
        err(EXIT_FAILURE, "calloc");

    // One thread per submodule
    for (size_t i = 0; i < count; i++)
    {
        jobs[i].repo_path = repo_path;
        jobs[i].path = paths[i];
        if (pthread_create(&threads[i], NULL, __PullWorker, &jobs[i]) == 0)
            started[i] = 1;
        else
            __PullWorker(&jobs[i]); // No thread available : do it here
    }

    for (size_t i = 0; i < count; i++)
        if (started[i])
            pthread_join(threads[i], NULL);

    sem_destroy(&pull_slots);

    pthread_mutex_lock(&pull_lock);
    __ProgressClear();
    pthread_mutex_unlock(&pull_lock);

    if (pull_problems > 0)
        UiWarning("%zu of %zu submodules of %s could not be fully synced "
                  "automatically, check the messages above.",
                  pull_problems, count, repo_path);
    else
        UiSuccess("All the %zu submodules of %s are up to date.", count,
                  repo_path);

    for (size_t i = 0; i < count; i++)
        free(paths[i]);
    free(paths);
    free(jobs);
    free(threads);
    free(started);
}

// Run a shell command without showing what it writes (git is chatty : ssh
// banners, progress...). The output is only given back to be shown if the
// command failed. Returns 0 on success.
static int __RunQuiet(const char *command, char **output)
{
    *output = NULL;

    size_t full_size = strlen(command) + sizeof(" 2>&1");
    char *full = malloc(full_size);
    if (full == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(full, full_size, "%s 2>&1", command);

    FILE *pipe = popen(full, "r");
    free(full);
    if (pipe == NULL)
        return -1;

    char *buffer = NULL;
    size_t size = 0;
    char chunk[1024];
    size_t got;
    while ((got = fread(chunk, 1, sizeof(chunk), pipe)) > 0)
    {
        char *bigger = realloc(buffer, size + got + 1);
        if (bigger == NULL)
            break;
        buffer = bigger;
        memcpy(buffer + size, chunk, got);
        size += got;
    }
    if (buffer != NULL)
        buffer[size] = '\0';

    int status = pclose(pipe);
    *output = buffer;
    return status == 0 ? 0 : -1;
}

void __PullRepo(char *repo_path)
{
    UiPanel(stdout, UI_INFO, "Pull", "Pulling:\n%s", repo_path);

    // 1. Pull the repository itself, this must succeed
    size_t command_size = strlen(repo_path) + SIZE_OF_STRING;
    char command[command_size];
    snprintf(command, command_size, "git -C \"%s\" pull", repo_path);

    char *output;
    if (__RunQuiet(command, &output) != 0)
    {
        UiError("Impossible to pull the repository %s:\n%s", repo_path,
                output != NULL ? output : "");
        free(output);
        exit(EXIT_FAILURE);
    }
    free(output);

    // Repair gitlinks that have no url in .gitmodules
    __HealSubmodules(repo_path);

    // Make sure every submodule is initialized/checked out before pulling
    size_t init_command_size = strlen(repo_path) + SIZE_OF_STRING;
    char init_command[init_command_size];
    snprintf(init_command, init_command_size,
             "git -C \"%s\" submodule update --init --recursive --jobs %u",
             repo_path, __PullJobs());
    if (__RunQuiet(init_command, &output) != 0)
        UiWarning("Some submodules of %s could not be initialized:\n%s",
                  repo_path, output != NULL ? output : "");
    free(output);

    // Pull every submodule in parallel (one thread per submodule)
    __PullSubmodules(repo_path);
}