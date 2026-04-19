#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#define INFORMATION_FILE_PATH "/.jarvis/"
#define INFORMATION_FILE_NAME "info_file"

int GetAbsolutePath(char *main_folder_path);
int CreateInfoFile();
int SetMainFolderPath(char* main_folder_path, size_t size);
char *GetInfoFullPath();

/*  Info file structure :
<Path of the main folder>
*/