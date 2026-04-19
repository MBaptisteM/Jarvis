#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#define SEMESTER "S"
#define BIMESTER "B"
#define MAIN_FOLDER "TPs"

#define SIZE_OF_STRING 512

int IsEpitaRepo(char* repo_name);
char* GetRelavitvePath(char* repo_name);
char* GetOrCreateMainFolderPath();