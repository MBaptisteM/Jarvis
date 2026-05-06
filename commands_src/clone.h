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


int IsEpitaRepo(char* repo_name);
char** GetRelavitvePath(char *repo_name, size_t *size);
char* GetOrCreateMainFolderPath();
int OneLayerFindOrCreate(char *path, char *name, int is_folder);