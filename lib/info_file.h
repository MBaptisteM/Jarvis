#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#define INFORMATION_FILE_PATH "/.jarvis/"
#define INFORMATION_FILE_NAME "info_file"
#define NAME_PATH_INFO_FILE "main_path"

#define SIZE_OF_STRING 512



int ReadInfo(char* key, char *value);
int CreateInfoFile();
int WriteInfo(char* key, char* value);
char *GetInfoFullPath();

/*  Info file structure :
<Path of the main folder>
*/