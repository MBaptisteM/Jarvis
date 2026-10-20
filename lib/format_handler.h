#pragma once

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include "TPs_handler.h"

// Format settings (stored in .jarvis_config, see get_jarvis_paths.c)
int IsAutoFormatEnabled(void);
// Comma separated list (ex : ".c,.h,.pb.txt"), malloc'd, the caller must free
// it
char *GetFormatExtensions(void);
const char *GetDefaultFormatExtensions(void);
// " .C ++ " -> ".c++", "Json" -> ".json" ; NULL (and nothing allocated) when
// the text can't be an extension. The result is malloc'd.
char *NormalizeExtension(const char *input);

// clang-format -i on every file of "folder" (and its sub-folders) having one of
// the configured extensions. EXIT_SUCCESS when everything was formatted.
int FormatTree(const char *folder);