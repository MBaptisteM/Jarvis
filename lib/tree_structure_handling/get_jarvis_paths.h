#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "info_file.h"
#include "tree_structure.h"

#define NAME_JARVIS_FOLDER "/.jarvis"
#define INFORMATION_FILE_NAME "info_file"

const char *GetCommandPath();
const char *GetSubjectFolderPath();
char *GetInfoPath();
char *GetTPsPath();

int __GetRootPath(char** path);
int __GetLocalPath(char** path);
int __GetDotJarvisPath(char** path);