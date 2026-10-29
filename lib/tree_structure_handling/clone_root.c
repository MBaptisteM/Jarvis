#include "clone_root.h"

char *FindRootRepoNear(const char *name, int max_depth);

// Number of submodules cloned at the same time : JARVIS_JOBS, 8 by default
static unsigned __CloneJobs(void)
{
    const char *value = getenv("JARVIS_JOBS");
    if (value == NULL || value[0] == '\0')
        return 8;

    char *end = NULL;
    long jobs = strtol(value, &end, 10);
    if (*end != '\0' || jobs < 1)
        return 8;
    return jobs > 64 ? 64 : (unsigned)jobs;
}

static int __IsGitRepositoryPath(const char *path)
{
    size_t git_path_size = strlen(path) + sizeof("/.git");
    char *git_path = malloc(git_path_size);
    if (git_path == NULL)
        err(EXIT_FAILURE, "malloc");

    snprintf(git_path, git_path_size, "%s/.git", path);
    struct stat info;
    int is_git_repository = stat(git_path, &info) == 0;
    free(git_path);
    return is_git_repository;
}

static char *__JoinRepoPath(const char *parent, const char *repo_name)
{
    size_t path_size = strlen(parent) + strlen(repo_name) + 2;
    char *path = malloc(path_size);
    if (path == NULL)
        err(EXIT_FAILURE, "malloc");

    snprintf(path, path_size, "%s/%s", parent, repo_name);
    return path;
}

// Try to detect the currently connected GitHub account by parsing the
// greeting ssh sends back when authenticating over the git protocol.
// Returns a malloc'd string (caller must free), or NULL if it couldn't be
// determined.
char *__GetGithubUsername(void)
{
    FILE *fp = popen("GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=no' "
                     "ssh -o StrictHostKeyChecking=no -T git@github.com 2>&1",
                     "r");
    if (fp == NULL)
        return NULL;

    char line[256];
    char *username = NULL;
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char buffer[256];
        if (sscanf(line, "Hi %255[^!]!", buffer) == 1)
        {
            username = strdup(buffer);
            if (username == NULL)
            {
                pclose(fp);
                err(EXIT_FAILURE, "strdup");
            }
            break;
        }
    }
    pclose(fp);
    return username;
}

// Check whether a git remote actually exists and is reachable
int __RemoteRepoExists(const char *url)
{
    size_t cmd_size = strlen(url) + SIZE_OF_STRING;
    char cmd[cmd_size];
    snprintf(cmd, cmd_size,
             "GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=no' git ls-remote "
             "\"%s\" > /dev/null 2>&1",
             url);
    return system(cmd) == 0;
}

// Ask the user to manually enter the repository URL, e.g. when the
// currently connected account couldn't be detected or the expected
// repository doesn't exist.
char *__PromptForRepoUrl(void)
{
    UiWarning("Impossible to find your EPITA-TPs repository automatically.");
    UiQuestion("Please enter the git URL of your EPITA-TPs repository.");
    printf("> ");
    fflush(stdout);

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        errx(EXIT_FAILURE, "ERROR No repository URL provided");

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
        buffer[len - 1] = 0;

    if (strlen(buffer) == 0)
        errx(EXIT_FAILURE, "ERROR No repository URL provided");

    char *repo_url = strdup(buffer);
    if (repo_url == NULL)
        err(EXIT_FAILURE, "strdup");
    return repo_url;
}

// Clone (or reuse) the root EPITA-TPs repository, using the currently
// connected git/GitHub account by default. This is what "jarvis clone"
// (no argument), "jarvis clone all" and "jarvis auth" trigger.
void __CloneParentRepo(void)
{
    // Already cloned and registered ?
    char *root_folder;
    if (ReadInfo("main_path", &root_folder) == 0)
    {
        if (__IsGitRepositoryPath(root_folder))
        {
            UiInfo("The root repository is already cloned at:\n%s",
                   root_folder);
            free(root_folder);
            return;
        }
        free(root_folder);
    }

    // Not registered yet (or the registered path is gone) : it might
    // already be cloned locally under its expected name (REPO_NAME),
    // just unknown to Jarvis (e.g. after a failed previous run, or a
    // manual clone) -> look for it in the arborescence before trying to
    // clone it again from scratch
    char *found_path = FindRootRepoNear(REPO_NAME, ROOT_SEARCH_DEPTH);
    if (found_path != NULL)
    {
        char *repo_path = __JoinRepoPath(found_path, REPO_NAME);
        free(found_path);

        if (__IsGitRepositoryPath(repo_path))
        {
            UiSuccess("Found an existing root repository at:\n%s", repo_path);

            WriteInfo("main_path", repo_path);
            WriteInfo("current", repo_path);
            free(repo_path);
            return;
        }

        free(repo_path);
    }

    // Really not found anywhere : clone it, assuming it lives under
    // REPO_NAME on the currently active git account
    char *username = __GetGithubUsername();
    char *url = NULL;

    if (username != NULL)
    {
        size_t url_size = strlen(username) + SIZE_OF_STRING;
        url = malloc(url_size);
        if (url == NULL)
        {
            free(username);
            err(EXIT_FAILURE, "malloc");
        }
        snprintf(url, url_size, "git@github.com:%s/%s.git", username,
                 REPO_NAME);
        free(username);

        if (!__RemoteRepoExists(url))
        {
            UiWarning("Impossible to find %s at %s.", REPO_NAME, url);
            free(url);
            url = __PromptForRepoUrl();
        }
    }
    else
    {
        url = __PromptForRepoUrl();
    }

    UiInfo("Cloning the root repository:\n%s", url);

    size_t command_size = strlen(url) + strlen(REPO_NAME) + SIZE_OF_STRING;
    char command[command_size];
    snprintf(command, command_size,
             "GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=no' git clone "
             "--recursive --jobs %u \"%s\" %s",
             __CloneJobs(), url, REPO_NAME);

    if (system(command))
        errx(EXIT_FAILURE, "ERROR Impossible to clone the root repository %s",
             url);

    free(url);

    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL)
        errx(EXIT_FAILURE, "ERROR Impossible to get the current directory");

    size_t path_size = strlen(cwd) + strlen(REPO_NAME) + 2;
    char *full_path = malloc(path_size);
    snprintf(full_path, path_size, "%s/%s", cwd, REPO_NAME);
    free(cwd);

    WriteInfo("main_path", full_path);
    WriteInfo("current", full_path);

    UiSuccess("Root repository cloned at:\n%s", full_path);

    free(full_path);
}