#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include<err.h>

#define NAME_JARVIS_FOLDER "/.jarvis"
#define INFORMATION_FILE_NAME "info_file"

const char *GetCommandPath();
const char *GetSubjectPath();
char *GetInfoPath();

int GetRootPath(char** path);
int GetLocalPath(char** path);
int GetDotJarvisPath(char** path);