#pragma once

#include <err.h>
#include <get_jarvis_paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define URL "https://intra.forge.epita.fr/"
#define BEGINING_REPO_LOCAL_PATH "forge.epita.fr:p/"

#define SUBJECT "EMBEDDED_subject.html"
#define GIVEN_FILES "assets.tar.gz"

int DowloadPage(char *url, char *file_name);
int GetSubject(char *repo_name);
int GetGivenFiles(char *repo_name);
int Auth();
const char *GetLocalUrlRepo(char *repo_name);
