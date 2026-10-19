#pragma once

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "get_jarvis_paths.h"
#include "info_file.h"
#include "tree_structure.h"
#include "ui.h"

#define TPs_FOLDER_NAME "EPITA-TPs"
#define GITIGNORE_CONTENT                                                      \
    "# This file should be filled depending on your Tps language"
#define README_CONTENT "All the TPs cloned with Jarvis"
#define REPO_NAME "EPITA-TPs"


// Result of the push functions
#ifndef JARVIS_PUSH_PUSHED
#define JARVIS_PUSH_PUSHED 0  // commits were really pushed
#define JARVIS_PUSH_FAILED 1  // a step failed (the error was displayed)
#define JARVIS_PUSH_NOTHING 2 // nothing to commit nor push, already up to date
#define JARVIS_PUSH_PARTIAL 3 // pushed, but a part of the work was left out
#endif


char *GetTPsPath();
void __RunCommand(const char *command);
char *GetOrCreateRepoRoot();
int PushRepoRoot(char *commit_name);
int PushRepoRootForce(char *commit_name);
void InstallGH();
// void RestoreGitFolders();
int PushRepo(char *repo_path, char *commit_name, char *tag);
void CleanupOnExit();
int AddRepoRoot();
void __AddOneToTag(char **tag);
void MergeFodldersClone(char *Tps_path, char *clone_path);