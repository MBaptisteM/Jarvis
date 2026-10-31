#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "format_handler.h"
#include "info_file.h"
#include "tree_structure.h"
#include "ui.h"

#define INFORMATION_FILE_NAME "info_file"

// What jarvis does about the tidy problems before a push
// (setting "auto_tidy" of .jarvis_config : off / warn / fix)
enum
{
    TIDY_MODE_OFF, // nothing
    TIDY_MODE_WARN, // check, then choose to push anyway or to cancel
    TIDY_MODE_FIX // check and fix what can be fixed (names, comments, newlines)
};

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
int IsAutoTidyEnabled(void);
int GetTidyMode(void);
int IsAutoFormatEnabled(void);

int __GetRootPath(char **path);
int __GetLocalPath(char **path);