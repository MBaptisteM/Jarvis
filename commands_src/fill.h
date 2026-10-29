#pragma once

#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "TPs_handler.h"
#include "get_documents.h"
#include "get_jarvis_paths.h"
#include "ui.h"
#include "counter.h"

#define FILL_BUFFER 1024
#define FILL_NAME_MAX 128
#define FILL_SEARCH_DEPTH 6
#define FILL_BAR_WIDTH 30
#define FILL_DEFAULT_JOBS 8
#define FILL_MAX_JOBS 64