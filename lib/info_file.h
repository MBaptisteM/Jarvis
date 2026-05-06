#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>


#define NAME_PATH_INFO_FILE "main_path"

#define SIZE_OF_STRING 512



int ReadInfo(char* key, char *value);
int __CreateInfoFile();
int WriteInfo(char* key, char* value);