#ifndef TIDY_HANDLER_H
#define TIDY_HANDLER_H

#ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#endif

#include <ctype.h>
#include <err.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "tidy_handler.h"
#include "ui.h"

#define TIDY_MAX_ARGS 4
#define TIDY_MAX_LINES 30
#define TIDY_MAX_EXPORTED_FUNCTIONS 10
#define TIDY_MAX_EXPORTED_VARIABLES 1

// Check only : nothing is modified. Returns the number of problems found.
int TidyTree(const char *folder);

// Fix what can be fixed (include guard names, #endif comments, final newline),
// then report the others (functions too long or with too many arguments are
// never modified). Returns the number of problems left.
int TidyTreeFix(const char *folder);

// After TidyTree or TidyTreeFix : how many of the problems left can be fixed by
// TidyTreeFix, and how many can't (functions with too many lines or
// arguments, #if never closed...)
size_t TidyLastFixable(void);
size_t TidyLastUnfixable(void);

#endif /* TIDY_HANDLER_H */
