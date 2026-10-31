#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "choice.h"
#include "tidy_handler.h"
#include "ui.h"

#define USAGE "Usage: jarvis tidy [-f | --fix] [<path> | -p <path>]"