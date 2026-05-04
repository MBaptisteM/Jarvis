#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <get_jarvis_paths.h>
#include <string.h>

#define URL "https://intra.forge.epita.fr/"
#define BEGINING_REPO_LOCAL_PATH "forge.epita.fr:p/"

int GetSubject(char* local_url_repo);
int Auth();
const char* GetLocalUrlRepo(char* repo_name);