#ifndef CUSTOM_H
#define CUSTOM_H

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "choice.h"
#include "info_file.h"


const char *GetDefaultLayerPrefix(const char *layer);
int IsLayerEnabled(const char *layer);
char *GetLayerPrefix(const char *layer);
size_t GetLayerFolders(int bimester, char **folders);

#endif /* CUSTOM_H */