#ifndef CUSTOM_H
#define CUSTOM_H

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "choice.h"
#include "info_file.h"
#include "ui.h"
#include "TPs_handler.h"


const char *GetDefaultLayerPrefix(const char *layer);
int IsLayerEnabled(const char *layer);
char *GetLayerPrefix(const char *layer);

#endif /* CUSTOM_H */