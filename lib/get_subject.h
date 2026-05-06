#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <get_jarvis_paths.h>
#include <string.h>

#define URL "https://intra.forge.epita.fr/"
#define BEGINING_REPO_LOCAL_PATH "forge.epita.fr:p/"

#define SUBJECT "EMBEDDED_subject.html"
#define GIVEN_FILES "assets.tar.gz"

int GetSubject(char* repo_name);
int GetGivenFiles(char* repo_name);
int Login();
const char* GetLocalUrlRepo(char* repo_name);