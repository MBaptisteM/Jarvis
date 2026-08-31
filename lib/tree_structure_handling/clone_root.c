#include "clone_root.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "info_file.h"
#include "get_documents.h"
#include "TPs_handler.h"

#define SIZE_OF_STRING 512

// Try to detect the currently connected GitHub account by parsing the
// greeting ssh sends back when authenticating over the git protocol.
// Returns a malloc'd string (caller must free), or NULL if it couldn't be
// determined.
char* __GetGithubUsername(void){
    FILE* fp = popen("GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=no' "
                      "ssh -o StrictHostKeyChecking=no -T git@github.com 2>&1", "r");
    if (fp == NULL)
        return NULL;

    char line[256];
    char* username = NULL;
    while (fgets(line, sizeof(line), fp) != NULL){
        char buffer[256];
        if (sscanf(line, "Hi %255[^!]!", buffer) == 1){
            username = strdup(buffer);
            break;
        }
    }
    pclose(fp);
    return username;
}

// Check whether a git remote actually exists and is reachable
int __RemoteRepoExists(const char* url){
    size_t cmd_size = strlen(url) + SIZE_OF_STRING;
    char cmd[cmd_size];
    snprintf(cmd, cmd_size,
            "GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=no' git ls-remote \"%s\" > /dev/null 2>&1",
            url);
    return system(cmd) == 0;
}

// Ask the user to manually enter the repository URL, e.g. when the
// currently connected account couldn't be detected or the expected
// repository doesn't exist.
char* __PromptForRepoUrl(void){
    printf("\033[33mWARNING Impossible to find your EPITA-TPs repository automatically.\033[0m\n");
    printf("Please enter the git URL of your EPITA-TPs repository:\n> ");
    fflush(stdout);

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        errx(EXIT_FAILURE, "ERROR No repository URL provided");

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
        buffer[len - 1] = 0;

    if (strlen(buffer) == 0)
        errx(EXIT_FAILURE, "ERROR No repository URL provided");

    return strdup(buffer);
}

// Clone (or reuse) the root EPITA-TPs repository, using the currently
// connected git/GitHub account by default. This is what "jarvis clone"
// (no argument), "jarvis clone all" and "jarvis auth" trigger.
void __CloneParentRepo(void){
    // Already cloned and registered ?
    char* root_folder;
    if (ReadInfo("main_path", &root_folder) == 0){
        if (access(root_folder, F_OK) == 0){
            printf("\033[1;32mThe root repository is already cloned at :\033[0m\n\033[1m%s\033[0m\n\n", root_folder);
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
    char* found_path = FindFileBFS(REPO_NAME);
    if (found_path != NULL){
        printf("\033[1;32mFound an existing root repository at :\033[0m\n\033[1m%s\033[0m\n\n", found_path);

        WriteInfo("main_path", found_path);
        WriteInfo("current", found_path);

        free(found_path);
        return;
    }

    // Really not found anywhere : clone it, assuming it lives under
    // REPO_NAME on the currently active git account
    char* username = __GetGithubUsername();
    char* url = NULL;

    if (username != NULL){
        size_t url_size = strlen(username) + SIZE_OF_STRING;
        url = malloc(url_size);
        snprintf(url, url_size, "git@github.com:%s/%s.git", username, REPO_NAME);
        free(username);

        if (!__RemoteRepoExists(url)){
            printf("\033[33mWARNING Impossible to find %s at %s.\033[0m\n", REPO_NAME, url);
            free(url);
            url = __PromptForRepoUrl();
        }
    }
    else{
        url = __PromptForRepoUrl();
    }

    printf("\033[1mCloning the root repository :\033[0m\n\033[1m%s\033[0m\n\n", url);

    size_t command_size = strlen(url) + strlen(REPO_NAME) + SIZE_OF_STRING;
    char command[command_size];
    snprintf(command, command_size,
            "GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=no' git clone --recursive \"%s\" %s",
            url, REPO_NAME);

    if (system(command))
        errx(EXIT_FAILURE, "ERROR Impossible to clone the root repository %s", url);

    free(url);

    char* cwd = getcwd(NULL, 0);
    if (cwd == NULL)
        errx(EXIT_FAILURE, "ERROR Impossible to get the current directory");

    size_t path_size = strlen(cwd) + strlen(REPO_NAME) + 2;
    char* full_path = malloc(path_size);
    snprintf(full_path, path_size, "%s/%s", cwd, REPO_NAME);
    free(cwd);

    WriteInfo("main_path", full_path);
    WriteInfo("current", full_path);

    printf("\033[1;32mRoot repository cloned at :\033[0m\n\033[1m%s\033[0m\n\n", full_path);

    free(full_path);
}