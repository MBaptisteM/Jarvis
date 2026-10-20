#include "TPs_handler.h"

// Get the path to the main folder or create it
// Example : ~/Jarvis/TPs/
char *CreateTPsPath()
{
    if (mkdir(TPs_FOLDER_NAME, 0755) != 0 && !IsDirectory(TPs_FOLDER_NAME))
        errx(EXIT_FAILURE, "ERROR Impossible to create the folder : %s",
             TPs_FOLDER_NAME);

    char command_mark[SIZE_OF_STRING];
    snprintf(command_mark, sizeof(command_mark), "touch %s/%s", TPs_FOLDER_NAME,
             MARKED_FILE_NAME);

    if (system(command_mark))
        errx(EXIT_FAILURE, "ERROR Impossible to create the file : %s/%s",
             TPs_FOLDER_NAME, MARKED_FILE_NAME);

    char *path = getcwd(NULL, 0);
    if (path == NULL)
        return NULL;

    size_t path_size = 0;
    while (path[path_size] != '\0')
        path_size++;

    size_t main_folder_size = 0;
    while (TPs_FOLDER_NAME[main_folder_size])
        main_folder_size++;

    size_t total_size = path_size + main_folder_size + 3;

    char *resized_path = realloc(path, total_size);
    if (resized_path == NULL)
    {
        free(path);
        err(EXIT_FAILURE, "realloc");
    }
    path = resized_path;

    char *final_path = malloc(total_size);
    if (final_path == NULL)
    {
        free(path);
        err(EXIT_FAILURE, "malloc");
    }
    snprintf(final_path, total_size, "%s/%s/", path, TPs_FOLDER_NAME);

    free(path);

    WriteInfo(NAME_PATH_INFO_FILE, final_path);

    return final_path;
}

// Look for an already cloned root repository (REPO_NAME) in the user's tree.
// Returns its path with a trailing '/', or NULL if there is none.
static char *FindExistingRepoRoot(void)
{
    char *parent = FindFileBFS(REPO_NAME);
    if (parent == NULL)
        return NULL;

    size_t path_size = strlen(parent) + strlen(REPO_NAME) + 8;
    char *root_path = malloc(path_size);
    if (root_path == NULL)
    {
        free(parent);
        err(EXIT_FAILURE, "malloc");
    }
    snprintf(root_path, path_size, "%s/%s/", parent, REPO_NAME);
    free(parent);

    char git_path[path_size + sizeof(".git")];
    snprintf(git_path, sizeof(git_path), "%s.git", root_path);
    if (access(git_path, F_OK) != 0)
    {
        free(root_path);
        return NULL;
    }

    return root_path;
}

// 0 : create | 1 : already created | 2 : cloned
char *GetOrCreateRepoRoot()
{
    char *path = GetTPsPath();

    if (path == NULL)
    {
        // Already cloned somewhere (e.g. the info file was lost) : reuse it
        // instead of trying to clone it again
        char *existing_path = FindExistingRepoRoot();
        if (existing_path != NULL)
        {
            WriteInfo(NAME_PATH_INFO_FILE, existing_path);
            return existing_path;
        }

        // Already exist check
        char cmd_exist[SIZE_OF_STRING];
        snprintf(cmd_exist, sizeof(cmd_exist),
                 "gh repo view \"%s\" > /dev/null 2>&1", REPO_NAME);

        if (system(cmd_exist) == 0)
        {
            snprintf(cmd_exist, sizeof(cmd_exist),
                     "gh repo clone \"%s\" -- --recurse-submodules", REPO_NAME);
            if (system(cmd_exist))
                errx(EXIT_FAILURE, "ERROR Impossible to clone the root repo");

            char *cwd = getcwd(NULL, 0);
            if (cwd == NULL)
                err(EXIT_FAILURE, "getcwd");
            size_t new_path_size = strlen(cwd) + strlen(REPO_NAME) + 5;
            char *new_path = malloc(new_path_size);
            if (new_path == NULL)
            {
                free(cwd);
                err(EXIT_FAILURE, "malloc");
            }
            snprintf(new_path, new_path_size, "%s/%s/", cwd, REPO_NAME);
            free(cwd);

            WriteInfo(NAME_PATH_INFO_FILE, new_path);

            return new_path;
        }

        path = CreateTPsPath();
        if (path == NULL)
            errx(EXIT_FAILURE, "ERROR Unable to determine the TPs folder path");

        InstallGH();

        if (chdir(path) != 0)
            err(EXIT_FAILURE, "chdir(%s)", path);

        // Initialise
        if (access(".git", F_OK) != 0)
            __RunCommand("git init -b main");

        // Auth GitHub
        if (system("gh auth status > /dev/null 2>&1") != 0)
        {
            UiInfo("GitHub authentication required.");

            if (system("gh auth login") != 0)
                errx(EXIT_FAILURE, "GitHub authentication failed");
        }

        // README.md
        FILE *f = fopen("README.md", "w");
        if (!f)
            err(EXIT_FAILURE, "README.md");

        fputs(README_CONTENT, f);
        fclose(f);

        // .gitignore
        f = fopen(".gitignore", "w");
        if (!f)
            err(EXIT_FAILURE, ".gitignore");

        fputs(GITIGNORE_CONTENT, f);
        fputs("\n**/.git\n", f);
        fclose(f);

        // .jarvis_config (tree configuration, created with the default values)
        EnsureConfigFile();

        // Add files
        if (system("find . "
                   "-path './.git' -prune -o "
                   "-path '*/.git' -prune -o "
                   "-type f -print0 | xargs -0 git add -- "
                   "> /dev/null 2>&1"))
        {
        }

        // Commit if necessary
        if (system("git diff --cached --quiet") != 0)
            __RunCommand("git commit -m \"Initial commit\"");

        char command[4096];

        snprintf(command, sizeof(command),
                 "gh repo create \"%s\" "
                 "--private "
                 "--source=. "
                 "--remote=origin "
                 "--push",
                 REPO_NAME);

        __RunCommand(command);
    }

    return path;
}

static void AddNixProfileToPath()
{
    const char *home = getenv("HOME");
    if (home == NULL)
        return;

    const char *path = getenv("PATH");
    if (path == NULL)
        path = "";

    size_t profile_path_size = 2 * strlen(home) + strlen(path)
        + sizeof("/.nix-profile/bin:/.local/state/nix/profiles/profile/bin:");
    char *profile_path = malloc(profile_path_size);
    if (profile_path == NULL)
        err(EXIT_FAILURE, "Unable to allocate PATH for Nix profile");

    snprintf(profile_path, profile_path_size,
             "%s/.nix-profile/bin:%s/.local/state/nix/profiles/profile/bin:%s",
             home, home, path);
    int set_path_result = setenv("PATH", profile_path, 1);
    free(profile_path);
    if (set_path_result != 0)
        err(EXIT_FAILURE, "Unable to update PATH for Nix profile");
}

void InstallGH()
{
    AddNixProfileToPath();
    // Check if gh exists
    if (system("command -v gh > /dev/null 2>&1") == 0)
        return;

    if (system("command -v nix > /dev/null 2>&1") != 0)
        errx(EXIT_FAILURE,
             "GitHub CLI is required; install it with: nix profile add "
             "nixpkgs#gh");

    __RunCommand("nix profile add nixpkgs#gh");
    if (system("command -v gh > /dev/null 2>&1") != 0)
        errx(EXIT_FAILURE,
             "GitHub CLI was installed with Nix but is not available in PATH");
}

// // Rename .git.backup to .git
// void RestoreGitFolders(){
//     if (system(
//     "find . -type d -name '.temp_renames_git' "
//     "-exec sh -c 'mv \"$1\" \"$(dirname \"$1\")/.git\"' _ {} \\;"
//     "> /dev/null 2>&1"
//     )){}
// }

void CleanupOnExit()
{
    // Wait for sub process
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    // Reset the terminal
    if (system("stty sane") == -1)
        errx(EXIT_FAILURE, "ERROR Impossible to reset the terminal");
}

// Shell script (run from the repository root) that repairs gitlinks present in
// the index but missing from .gitmodules.
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

// Every step reports its own problem and never stops the program

// Run "git <argv>" without a shell. EXIT_SUCCESS if git succeeded.
static int __RunGit(char *const argv[], int quiet)
{
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0)
        return EXIT_FAILURE;
    if (pid == 0)
    {
        if (quiet)
        {
            int null_fd = open("/dev/null", O_WRONLY);
            if (null_fd >= 0)
            {
                dup2(null_fd, STDOUT_FILENO);
                dup2(null_fd, STDERR_FILENO);
            }
        }
        execvp("git", argv);
        _exit(127);
    }

    int status;
    while (waitpid(pid, &status, 0) < 0)
        if (errno != EINTR)
            return EXIT_FAILURE;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? EXIT_SUCCESS
                                                         : EXIT_FAILURE;
}

// First line printed by a command. EXIT_SUCCESS if it succeeded and printed
// something.
static int __FirstLine(const char *command, char *buffer, size_t size)
{
    FILE *pipe = popen(command, "r");
    if (pipe == NULL)
        return EXIT_FAILURE;

    int has_output = fgets(buffer, (int)size, pipe) != NULL;
    int result = pclose(pipe);
    if (!has_output || result != 0)
        return EXIT_FAILURE;

    buffer[strcspn(buffer, "\r\n")] = '\0';
    return EXIT_SUCCESS;
}

// Number of local commits that are not on the upstream branch.
// EXIT_FAILURE when there is no upstream (new branch, remote branch gone...).
static int __UnpushedCommits(long *count)
{
    char line[64];
    if (__FirstLine("git rev-list --count '@{u}..HEAD' 2>/dev/null", line,
                    sizeof(line))
        != EXIT_SUCCESS)
        return EXIT_FAILURE;

    char *end = NULL;
    long value = strtol(line, &end, 10);
    if (end == line || *end != '\0' || value < 0)
        return EXIT_FAILURE;

    *count = value;
    return EXIT_SUCCESS;
}

// Commit what is staged in the current repository.
// -1 : error | 0 : nothing was staged | 1 : committed
static int __CommitStaged(const char *label, const char *commit_name)
{
    int staged = system("git diff --cached --quiet");
    if (staged == -1 || !WIFEXITED(staged) || WEXITSTATUS(staged) > 1)
    {
        UiError("Impossible to check the changes of the %s.", label);
        return -1;
    }
    if (WEXITSTATUS(staged) == 0)
        return 0;

    char *commit[] = { "git", "commit", "-q", "-m", (char *)commit_name, NULL };
    if (__RunGit(commit, 0) != EXIT_SUCCESS)
    {
        UiError("Impossible to commit the %s.", label);
        return -1;
    }
    return 1;
}

// Push HEAD of the current repository, then check that the remote has it.
// pushed = number of commits sent (-1 if unknown).
static int __PushHead(const char *label, int force_push, long *pushed)
{
    *pushed = -1;

    // Empty repository : nothing was ever committed
    if (system("git rev-parse --verify -q HEAD > /dev/null 2>&1") != 0)
        return JARVIS_PUSH_NOTHING;

    long ahead = 0;
    int tracked = __UnpushedCommits(&ahead) == EXIT_SUCCESS;
    if (tracked && ahead == 0)
        return JARVIS_PUSH_NOTHING;

    int result;
    if (tracked)
        result =
            system(force_push ? "git push --force-with-lease" : "git push");
    else
        result =
            system(force_push ? "git push --force-with-lease -u origin HEAD"
                              : "git push -u origin HEAD");
    if (result != 0)
    {
        UiError("Impossible to push the %s (see the git message above). If "
                "the remote has new commits, run 'jarvis pull' first.",
                label);
        return JARVIS_PUSH_FAILED;
    }

    long remaining = -1;
    if (__UnpushedCommits(&remaining) != EXIT_SUCCESS || remaining != 0)
    {
        UiError("git did not report an error, but the %s still has commits "
                "that are not on the remote.",
                label);
        return JARVIS_PUSH_FAILED;
    }

    if (tracked)
        *pushed = ahead;
    return JARVIS_PUSH_PUSHED;
}

// Tell what really happened. Returns the final status.
static int __ReportPush(const char *label, int status, long pushed,
                        int incomplete)
{
    switch (status)
    {
    case JARVIS_PUSH_PUSHED:
        if (incomplete)
        {
            UiWarning("The %s was pushed, but some elements could not be "
                      "added (see above) : they are NOT on the remote.",
                      label);
            return JARVIS_PUSH_PARTIAL;
        }
        if (pushed > 0)
            UiSuccess("The %s was pushed (%ld commit%s).", label, pushed,
                      pushed > 1 ? "s" : "");
        else
            UiSuccess("The %s was pushed.", label);
        return JARVIS_PUSH_PUSHED;

    case JARVIS_PUSH_NOTHING:
        if (incomplete)
        {
            UiWarning("Nothing was pushed for the %s because some elements "
                      "could not be added (see above).",
                      label);
            return JARVIS_PUSH_FAILED;
        }
        UiInfo("The %s has nothing to push, it is already up to date.", label);
        return JARVIS_PUSH_NOTHING;

    default:
        return JARVIS_PUSH_FAILED;
    }
}

static int __StatusToExit(int status)
{
    return status == JARVIS_PUSH_PUSHED || status == JARVIS_PUSH_NOTHING
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}

// A nested repository that has no commit yet can't be added by git : it refuses
// ("does not have a commit checked out") and stages nothing at all. Its .git is
// hidden during the add, and put back right after.
static const char *PARK_COMMAND =
    "find . -path './.git' -prune -o -name .git -prune "
    "-exec sh -c 'for git_dir do repo=${git_dir%/.git}; "
    "if ! git -C \"$repo\" rev-parse --verify HEAD >/dev/null 2>&1; "
    "then test ! -e \"$git_dir.jarvis-tmp\" && "
    "mv -- \"$git_dir\" \"$git_dir.jarvis-tmp\" || exit 1; "
    "fi; done' _ {} +";
static const char *RESTORE_COMMAND =
    "find . -path './.git' -prune -o -name .git.jarvis-tmp -prune "
    "-exec sh -c 'for saved do mv -- \"$saved\" "
    "\"${saved%.jarvis-tmp}\" || exit 1; done' _ {} +";

// Stage everything of the current repository. EXIT_SUCCESS if all went well,
// the problems are displayed as warnings.
static int __StageAll(void)
{
    if (system(PARK_COMMAND) != 0)
        UiWarning("Impossible to hide the empty nested repositories, trying "
                  "to add anyway.");

    int add_result = system("git add -A -- . ':(exclude)**/.git.jarvis-tmp' "
                            "':(exclude)**/.git.jarvis-tmp/**'");

    // Always put the metadata back, even if the add failed
    int restore_result = system(RESTORE_COMMAND);
    if (restore_result != 0)
        UiWarning("Unable to restore nested Git metadata (look for "
                  ".git.jarvis-tmp folders).");

    if (add_result != 0)
        UiWarning("Some elements could not be added (see the git message "
                  "above).");

    return add_result != 0 || restore_result != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

int AddRepoRoot()
{
    atexit(CleanupOnExit);

    char *path = GetOrCreateRepoRoot();

    if (chdir(path) != 0)
    {
        UiError("Impossible to go in the root repository %s.", path);
        free(path);
        return EXIT_FAILURE;
    }
    free(path);

    // The tree configuration is shared through the root repository : make
    // sure it exists (default values) so that it is pushed with the rest
    EnsureConfigFile();

    int failed = __StageAll() != EXIT_SUCCESS;

    // Always track the config, even if a .gitignore pattern matches it
    if (system("test ! -e .jarvis_config || git add -f -- .jarvis_config "
               "> /dev/null 2>&1")
        != 0)
    {
        UiWarning("Impossible to add .jarvis_config to the root repository.");
        failed = 1;
    }

    if (system("git ls-files -z -- ':(glob)**/.git.jarvis-tmp' | xargs -0 -r "
               "git add -u --")
        != 0)
    {
        UiWarning("Impossible to clean the stale nested Git metadata of the "
                  "root repository.");
        failed = 1;
    }

    // Every nested repo with a commit must be declared in .gitmodules
    if (system(HEAL_SUBMODULES_SCRIPT) != 0)
    {
        UiWarning("Impossible to check the submodules of the root "
                  "repository.");
        failed = 1;
    }

    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

// Commit and push the root repository (what AddRepoRoot staged).
// staging_failed : AddRepoRoot reported a problem, so the push can't be
// called complete.
static int __PushRepoRootStatus(char *commit_name, int force_push,
                                int staging_failed)
{
    char *path = GetOrCreateRepoRoot();

    if (chdir(path) != 0)
    {
        UiError("Impossible to go in the root repository %s.", path);
        free(path);
        return JARVIS_PUSH_FAILED;
    }
    free(path);

    if (__CommitStaged("root repository", commit_name) < 0)
        return JARVIS_PUSH_FAILED;

    long pushed;
    int status = __PushHead("root repository", force_push, &pushed);
    return __ReportPush("root repository", status, pushed, staging_failed);
}

// JARVIS_PUSH_* result
int PushRepoRootStatus(char *commit_name, int staging_failed)
{
    return __PushRepoRootStatus(commit_name, 0, staging_failed);
}

// EXIT_SUCCESS when the root is pushed or already up to date
int PushRepoRoot(char *commit_name)
{
    return __StatusToExit(__PushRepoRootStatus(commit_name, 0, 0));
}

int PushRepoRootForce(char *commit_name)
{
    return __StatusToExit(__PushRepoRootStatus(commit_name, 1, 0));
}

// Add, commit and push the repository (and its tag). JARVIS_PUSH_* result.
int PushRepoStatus(char *repo_path, char *commit_name, char *tag)
{
    atexit(CleanupOnExit);

    if (repo_path != NULL && chdir(repo_path) != 0)
    {
        UiError("Impossible to go in %s.", repo_path);
        return JARVIS_PUSH_FAILED;
    }

    if (system("git rev-parse --git-dir > /dev/null 2>&1") != 0)
    {
        UiError("This folder is not a Git repository : %s",
                repo_path != NULL ? repo_path : ".");
        return JARVIS_PUSH_FAILED;
    }

    char current_folder[SIZE_OF_STRING * 2];
    if (getcwd(current_folder, sizeof(current_folder)) != NULL)
        WriteInfo("current", current_folder);

    int staging_failed = __StageAll() != EXIT_SUCCESS;

    if (__CommitStaged("repository", commit_name) < 0)
        return JARVIS_PUSH_FAILED;

    // The tag is created after the commit, so that it points at it
    int tag_created = 0;
    if (tag != NULL)
    {
        for (int attempt = 0; attempt < 50 && !tag_created; attempt++)
        {
            char *tag_command[] = { "git", "tag",       "-a", tag,
                                    "-m",  commit_name, NULL };
            if (__RunGit(tag_command, 1) == EXIT_SUCCESS)
                tag_created = 1;
            else
                __AddOneToTag(&tag);
        }
        if (!tag_created)
            UiWarning("Impossible to create the tag of the repository.");
    }

    long pushed;
    int status = __PushHead("repository", 0, &pushed);
    int result = __ReportPush("repository", status, pushed, staging_failed);

    if (tag != NULL && result != JARVIS_PUSH_FAILED)
    {
        char *push_tag[] = { "git", "push", "origin", "tag", tag, NULL };
        if (tag_created && __RunGit(push_tag, 0) == EXIT_SUCCESS)
            UiSuccess("The tag %s was pushed.", tag);
        else
        {
            if (tag_created)
                UiWarning("Impossible to push the tag %s.", tag);
            result = JARVIS_PUSH_PARTIAL;
        }
    }

    return result;
}

// Old interface : -1 if the folder does not exist, else EXIT_SUCCESS when
// pushed or already up to date
int PushRepo(char *repo_path, char *commit_name, char *tag)
{
    if (repo_path != NULL && !IsDirectory(repo_path))
        return -1;

    return __StatusToExit(PushRepoStatus(repo_path, commit_name, tag));
}

void __RunCommand(const char *command)
{
    int ret = system(command);

    if (ret != 0)
        errx(EXIT_FAILURE, "ERROR while running : %s", command);
}

void __AddOneToTag(char **tag)
{
    char *c = *tag;

    char stubborn[SIZE_OF_STRING] = "";

    int i = 0;
    char num_str[SIZE_OF_STRING];
    while (*c != 0)
    {
        if (*c != '-')
        {
            num_str[i++] = *c;
        }
        else
        {
            num_str[i] = 0;
            i = 0;
            strcat(stubborn, num_str);
            strcat(stubborn, "-");
        }

        c++;
    }
    num_str[i] = 0;

    i = 0;

    int num = 0;
    while (num_str[i] != 0)
    {
        num = num * 10 + num_str[i++] - '0';
    }
    num++;

    // free(*tag);
    // *tag = malloc(strlen(stubborn) + log10(num) + 2);
    snprintf(*tag, strlen(stubborn) + log10(num) + 2, "%s%d", stubborn, num);
}

void MergeFodldersClone(char *Tps_path, char *clone_path)
{
    Queue q = { 0 };

    Enqueue(&q, Tps_path);

    while (q.front)
    {
        char *current_dir = Dequeue(&q);

        if (!current_dir)
            continue;

        DIR *dir = opendir(current_dir);

        if (!dir)
        {
            free(current_dir);
            continue;
        }

        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL)
        {
            // Ignore . and ..
            if (strcmp(entry->d_name, ".") == 0
                || strcmp(entry->d_name, "..") == 0)
                continue;

            char full_path[MAX_PATH_SIZE];

            snprintf(full_path, sizeof(full_path), "%s/%s", current_dir,
                     entry->d_name);

            // Folder cloned
            if (strcmp(entry->d_name, REPO_NAME) == 0)
                continue;

            // Check if present in clone_folder else add it
            char path_in_clone[MAX_PATH_SIZE];
            snprintf(path_in_clone, MAX_PATH_SIZE, "%s/%s", clone_path,
                     entry->d_name);

            if (access(path_in_clone, F_OK) != 0)
            {
                if (IsDirectory(full_path))
                    mkdir(path_in_clone, 0755);
                else
                {
                    FILE *f = fopen(path_in_clone, "w");
                    if (f)
                        fclose(f);
                }
            }

            // Add subfolders
            if (IsDirectory(full_path))
            {
                Enqueue(&q, full_path);
            }
        }

        closedir(dir);
        free(current_dir);
    }
}