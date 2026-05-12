#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "get_documents.h"

#define SIZE_OF_STRING 512
#define TEMPORARY_FOLDER "tmp"

int GivenFilesDownload(char* repo_name);
int AddGivenFilesUsefulParts(char *folder_path, pid_t pid);
int UncompressGivenFiles(char *folder_path, pid_t pid);
int __GivenFilesCopy(char *path_folder, char *targer_folder);