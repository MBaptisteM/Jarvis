#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "info_file.h"
#include "choice.h"
#include "tree_structure.h"
#include "get_subject.h"

#define SEMESTER "S"
#define BIMESTER "B"
#define MAIN_FOLDER "TPs"


int __IsEpitaRepo(char* repo_name);
char** __GetRelavitvePath(char *repo_name, size_t *size);
char* __GetOrCreateMainFolderPath();
int __OneLayerFindOrCreate(char *path, char *name, int is_folder);
int __GivenFilesHandling(char* repo_name);
int __SubjectHandling(char* repo_name);