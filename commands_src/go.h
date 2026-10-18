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
#include "ui.h"
#include "get_jarvis_paths.h"
#include "current_handling.h"

#ifndef SEMESTER
#    define SEMESTER "S"
#endif
#ifndef BIMESTER
#    define BIMESTER "B"
#endif

#define OTHERS_FOLDER_NAME "Others"


#endif /* GO_H */