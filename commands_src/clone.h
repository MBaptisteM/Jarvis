#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <get_subject.h>

#include "info_file.h"
#include "choice.h"

#define SEMESTER "S"
#define BIMESTER "B"
#define MAIN_FOLDER "TPs"


int __IsEpitaRepo(char* repo_name);
char** __GetRelavitvePath(char *repo_name, size_t *size);
char* __GetOrCreateMainFolderPath();
int __OneLayerFindOrCreate(char *path, char *name, int is_folder);