#include "pull.h"

int main(int argc, char* argv[]){
    // Case main (or no argument): pull the root folder and every submodule
    if (argc < 2 || strcasecmp(argv[1], "all") == 0){
        __PullMain();
        return EXIT_SUCCESS;
    }

    // Case current: pull the repository saved as "current" in the logs
    if (strcasecmp(argv[1], "current") == 0){
        __PullCurrent();
        return EXIT_SUCCESS;
    }

    // Case a specific git repo: pull it if it has already been cloned
    __PullRepository(argv[1]);

    return EXIT_SUCCESS;
}


// Pull the root repository and all its submodules
void __PullMain(void){
    char* root_folder;
    if (ReadInfo("main_path", &root_folder))
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repository (did you clone anything yet?)");

    printf("\033[1mPulling all repositories starting from :\033[0m\n\033[1m%s\033[0m\n\n", root_folder);

    __PullRepo(root_folder);

    free(root_folder);
}


// Pull the repository saved as "current"
void __PullCurrent(void){
    char* path;
    if (ReadInfo("current", &path))
        errx(EXIT_FAILURE, "ERROR Trying to pull the current repository but no current repository found");

    // Get the repo name from the path
    char repo_name[SIZE_OF_STRING];
    int i = 0;
    char* c = path;
    while (*c != 0){
        if (*c != '/')
            repo_name[i++] = *c;
        else
            i = 0;
        c++;
    }
    repo_name[i] = 0;

    // Get the path again if the folder moved
    if (access(path, F_OK) != 0){
        free(path);
        path = FindFileBFS(repo_name);

        // Fallback: if the stored value was actually the raw git remote
        // (e.g. left over from a failed rename in clone.c), the folder on
        // disk was created by "git submodule add" using the same last
        // path segment but WITHOUT the trailing ".git". Retry with that.
        if (path == NULL){
            size_t len = strlen(repo_name);
            if (len > 4 && strcmp(repo_name + len - 4, ".git") == 0){
                repo_name[len - 4] = 0;
                path = FindFileBFS(repo_name);
            }
        }

        if (path == NULL)
            errx(EXIT_FAILURE, "ERROR Impossible to find the current repository %s locally (try to re-clone it)", repo_name);
    }

    __PullRepo(path);

    // Keep the current repository up to date in the logs
    WriteInfo("current", path);

    free(path);
}


// Pull one specific repository, given its git remote (must already be cloned)
void __PullRepository(char* repo_arg){
    // Get the relative repo name saved in the info files (same pattern as clone.c)
    char* repo_name;
    if (ReadInfo(repo_arg, &repo_name))
        errx(EXIT_FAILURE, "ERROR This repository has not been cloned yet, use 'jarvis clone' first.");

    // Get the root repository path to rebuild the full path, like clone.c does
    char* main_folder_path;
    if (ReadInfo("main_path", &main_folder_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repository (did you clone anything yet?)");

    size_t repo_path_size = strlen(main_folder_path) + strlen(repo_name) + 2;
    char repo_path[repo_path_size];
    snprintf(repo_path, repo_path_size, "%s%s", main_folder_path, repo_name);

    // Get the path again if the folder moved
    if (access(repo_path, F_OK) != 0){
        char* found_path = FindFileBFS(repo_name);
        if (found_path == NULL)
            errx(EXIT_FAILURE, "ERROR Impossible to find the repository %s locally (try to re-clone it)", repo_name);

        __PullRepo(found_path);

        // Save this repository as the current one, same pattern as the other commands
        WriteInfo("current", found_path);

        free(found_path);
    }
    else{
        __PullRepo(repo_path);

        // Save this repository as the current one, same pattern as the other commands
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
void __PullRepo(char* repo_path){
    printf("\033[1;32mPulling :\033[0m\n\033[1m%s\033[0m\n\n", repo_path);

    // 1. Pull the repository itself, this must succeed
    size_t command_size = strlen(repo_path) + SIZE_OF_STRING;
    char command[command_size];
    snprintf(command, command_size, "git -C \"%s\" pull", repo_path);

    if (system(command))
        errx(EXIT_FAILURE, "ERROR Impossible to pull the repository %s", repo_path);

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
    snprintf(submodule_command, submodule_command_size,
        "git -C \"%s\" submodule foreach --recursive '"
        "if ! git pull; then "
            "echo \"WARNING: submodule $name has a stale or unreachable commit, resyncing with its remote...\"; "
            "git fetch origin; "
            "default_branch=$(git remote show origin | sed -n \"/HEAD branch/s/.*: //p\"); "
            "if [ -n \"$default_branch\" ]; then "
                "(git checkout \"$default_branch\" 2>/dev/null || git checkout -b \"$default_branch\" \"origin/$default_branch\"); "
                "git reset --hard \"origin/$default_branch\" "
                "&& echo \"Resynced $name to origin/$default_branch\" "
                "|| echo \"ERROR: could not resync $name, please fix it manually.\"; "
            "else "
                "echo \"ERROR: could not determine the default branch for $name, please fix it manually.\"; "
            "fi; "
        "fi'",
        repo_path);

    if (system(submodule_command))
        printf("\033[33mWARNING: some submodules of %s could not be fully synced automatically, check the output above.\033[0m\n", repo_path);

    printf("\n");
}