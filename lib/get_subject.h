#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <get_jarvis_paths.h>

#define URL "https://intra.forge.epita.fr/"

int GetSubject(char* local_url_repo);
int Auth();