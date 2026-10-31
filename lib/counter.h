#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "choice.h"
#include "info_file.h"
#include "ui.h"

// The first time a user is connected to GitHub (gh) uses Jarvis in a
// terminal, he is asked whether he agrees to be counted.

#define COUNTER_REPO "MBaptisteM/Jarvis"
#define COUNTER_KEY "counter_asked"