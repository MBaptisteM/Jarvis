#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "info_file.h"
#include "tree_structure.h"
#include "ui.h"

#define INFORMATION_FILE_NAME "info_file"

const char *GetCommandPath();
const char *GetSubjectFolderPath();
char *GetInfoPath();
char *GetTPsPath();
int GetDotJarvisPath(char **path);
size_t GetLayerFolders(int bimester, char **folders);
int ReadRootPath(char **value);
int SetConfigValue(const char *key, const char *value);
char *GetOthersFolderName(void);
void EnsureConfigFile(void);

int __GetRootPath(char **path);
int __GetLocalPath(char **path);
