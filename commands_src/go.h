#ifndef GO_H
#define GO_H

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "choice.h"
#include "info_file.h"
#include "tree_structure.h"

#ifndef SEMESTER
#    define SEMESTER "S"
#endif
#ifndef BIMESTER
#    define BIMESTER "B"
#endif

#define OTHERS_FOLDER_NAME "Others"

// 0 : found | 1 : not found (searches the root repo again if it moved)
int ReadRootPath(char **value);

// Folders of the enabled layers (see "jarvis custom") for a bimester number
size_t GetLayerFolders(int bimester, char **folders);

#endif /* GO_H */