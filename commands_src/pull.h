#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>

#include "TPs_handler.h"
#include "info_file.h"
#include "ui.h"
#include "current_handling.h"

// Parallel pull of the submodules : one thread per top-level submodule
#define PULL_DEFAULT_JOBS 8
#define PULL_MAX_JOBS 64

// Pull of the repository we are in ($name is its name)
#define PULL_BODY                                                              \
    "if ! git pull; then "                                                     \
    "default_branch=$(git remote show origin 2>/dev/null | sed -n "            \
    "\"/HEAD branch/s/.*: //p\"); "                                            \
    "if [ -z \"$default_branch\" ] || [ \"$default_branch\" = "                \
    "\"(unknown)\" ]; then "                                                   \
    "echo \"$name: nothing to pull (empty or unreachable remote), "            \
    "skipped.\"; "                                                             \
    "elif [ -n \"$(git status --porcelain)\" ]; then "                         \
    "echo \"WARNING: $name could not be pulled and has local changes, "        \
    "not resyncing it, please fix it manually.\"; "                            \
    "else "                                                                    \
    "echo \"WARNING: submodule $name has a stale or unreachable commit, "      \
    "resyncing with its remote...\"; "                                         \
    "git fetch origin; "                                                       \
    "(git checkout \"$default_branch\" 2>/dev/null || git checkout -b "        \
    "\"$default_branch\" \"origin/$default_branch\"); "                        \
    "git reset --hard \"origin/$default_branch\" "                             \
    "&& echo \"Resynced $name to origin/$default_branch\" "                    \
    "|| echo \"ERROR: could not resync $name, please fix it manually.\"; "     \
    "fi; "                                                                     \
    "fi"

// Run from the root of the repository, $1 is the path of the submodule
static const char *PULL_SUBMODULE_SCRIPT =
    "cd \"$1\" || exit 1; name=\"$1\"; export top=\"$1\"; "
    "if [ ! -e .git ]; then "
    "echo \"I:$name: not initialized, skipped.\"; exit 0; fi; " PULL_BODY "; "
    "git submodule foreach --quiet --recursive "
    "'name=\"$top/$sm_path\"; " PULL_BODY "'";

typedef struct
{
    const char *repo_path;
    char *path; // path of the submodule, relative to repo_path
} PullJob;

static pthread_mutex_t pull_lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t pull_slots;
static size_t pull_total = 0;
static size_t pull_done = 0;
static size_t pull_problems = 0;
static size_t pull_running = 0;
static int pull_tty = 0;            // live progress line only on a terminal
static const char *pull_last = "";  // last submodule finished

int main(int argc, char *argv[]);
void __PullRepo(char *repo_path);
void __PullMain(void);
void __PullCurrent(void);
void __PullRepository(char *repo_arg);