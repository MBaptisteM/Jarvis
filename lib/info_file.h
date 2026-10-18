#pragma once

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "get_jarvis_paths.h"
#include "ui.h"

#define NAME_PATH_INFO_FILE "main_path"

#define SIZE_OF_STRING 512

int ReadInfo(char *key, char **value);
int __CreateInfoFile();
int WriteInfo(char *key, char *value);
int JarvisLockAcquire(const char *name);
void JarvisLockRelease(int fd);