#include "pull.h"

int main(int argc, char *argv[])
{
    // Case main (or no argument): pull the root folder and every submodule
    if (argc < 2 || strcasecmp(argv[1], "all") == 0)
    {
        __PullMain();
        return EXIT_SUCCESS;
    }

    // Case current: pull the repository saved as "current" in the logs
    if (strcasecmp(argv[1], "current") == 0)
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
        fprintf(stderr,
                "WARNING: correcting the saved root repository path to %s\n",
                repo_path);
        WriteInfo("main_path", repo_path);
        free(recorded_path);
        return repo_path;
    }

    free(repo_path);
    return recorded_path;
}

// Pull the root repository and all its submodules
void __PullMain(void)
{
    char *root_folder;
    if (ReadInfo("main_path", &root_folder))
        errx(EXIT_FAILURE,
             "ERROR Impossible to find the root repository (did you clone "
             "anything yet?)");

    root_folder = __FindRootRepository(root_folder);
    if (!__IsGitRepositoryPath(root_folder))
        errx(EXIT_FAILURE,
             "ERROR The saved root path %s is not a Git repository. "
             "Run 'jarvis auth' or 'jarvis clone all' to locate it again.",
             root_folder);

    printf("\033[1mPulling all repositories starting from "
           ":\033[0m\n\033[1m%s\033[0m\n\n",
           root_folder);

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
    if (ReadInfo("main_path", &root_folder) == 0)
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

    if (!__IsGitRepositoryPath(path) && current_was_root
        && root_folder != NULL && __IsGitRepositoryPath(root_folder))
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
        errx(EXIT_FAILURE,
             "ERROR The current path %s is not a Git repository.", path);
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
    if (ReadInfo("main_path", &main_folder_path))
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

        __PullRepo(found_path);

        // Save this repository as the current one, same pattern as the other
        // commands
        WriteInfo("current", found_path);

        free(found_path);
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

// Actually run "git pull" on a repository and show what got pulled.
// Also recurses into every submodule (nested ones included) and, if a
// submodule's recorded commit no longer exists on its remote (e.g. after
// a force-push/rebase upstream), automatically resyncs it to the remote's
// current default branch instead of aborting the whole pull.
void __PullRepo(char *repo_path)
{
    printf("\033[1;32mPulling :\033[0m\n\033[1m%s\033[0m\n\n", repo_path);

    // 1. Pull the repository itself, this must succeed
    size_t command_size = strlen(repo_path) + SIZE_OF_STRING;
    char command[command_size];
    snprintf(command, command_size, "git -C \"%s\" pull", repo_path);

    if (system(command))
        errx(EXIT_FAILURE, "ERROR Impossible to pull the repository %s",
             repo_path);

    // 2. Make sure every submodule is initialized/checked out before pulling
    size_t init_command_size = strlen(repo_path) + SIZE_OF_STRING;
    char init_command[init_command_size];
    snprintf(init_command, init_command_size,
             "git -C \"%s\" submodule update --init --recursive", repo_path);
    if (system(init_command) == -1)
        errx(EXIT_FAILURE, "ERROR impossible to update the submodules");

    // 3. Pull every submodule recursively. If a submodule's own "git pull"
    // fails (typically because the commit pinned by the parent repo is no
    // longer reachable on its remote), fetch it and resync it on top of
    // its remote's current default branch instead of stopping everything.
    size_t submodule_command_size = strlen(repo_path) + 1024;
    char submodule_command[submodule_command_size];
    snprintf(
        submodule_command, submodule_command_size,
        "git -C \"%s\" submodule foreach --recursive '"
        "if ! git pull; then "
        "echo \"WARNING: submodule $name has a stale or unreachable commit, "
        "resyncing with its remote...\"; "
        "git fetch origin; "
        "default_branch=$(git remote show origin | sed -n \"/HEAD branch/s/.*: "
        "//p\"); "
        "if [ -n \"$default_branch\" ]; then "
        "(git checkout \"$default_branch\" 2>/dev/null || git checkout -b "
        "\"$default_branch\" \"origin/$default_branch\"); "
        "git reset --hard \"origin/$default_branch\" "
        "&& echo \"Resynced $name to origin/$default_branch\" "
        "|| echo \"ERROR: could not resync $name, please fix it manually.\"; "
        "else "
        "echo \"ERROR: could not determine the default branch for $name, "
        "please fix it manually.\"; "
        "fi; "
        "fi'",
        repo_path);

    if (system(submodule_command))
        printf("\033[33mWARNING: some submodules of %s could not be fully "
               "synced automatically, check the output above.\033[0m\n",
               repo_path);

    printf("\n");
}