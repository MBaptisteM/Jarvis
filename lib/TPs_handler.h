#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/wait.h>

#include "info_file.h"
#include "tree_structure.h"
#include "get_jarvis_paths.h"

#define TPs_FOLDER_NAME "TPs"
#define GITIGNORE_CONTENT "# This file should be filled depending on your Tps language"
#define README_CONTENT "All the TPs cloned with Jarvis"
#define REPO_NAME "Jarvis TPs"

char* GetOrCreateTPsPath();
void __RunCommand(const char *command);
int CreateRepoRoot();
void PushRepoRoot();
void InstallGH();
void RestoreGitFolders();
