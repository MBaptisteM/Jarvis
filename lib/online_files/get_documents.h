#pragma once

#include <err.h>
#include <get_jarvis_paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <ctype.h>

#include "choice.h"
#include "ui.h"

#define URL "https://intra.forge.epita.fr/"
#define BEGINING_REPO_LOCAL_PATH "forge.epita.fr:p/"

#define SUBJECT "EMBEDDED_subject.html"
#define GIVEN_FILES "assets.tar.gz"

#define JARVIS_DETACH_TTY "</dev/null 2>&1"

#define SPINNER_LOG_MAX 16384
#define SPINNER_FRAME_MS 100

int DowloadPage(char *url, char *file_name);
int GetSubject(char *repo_name);
int GetGivenFiles(char *repo_name);
int Auth();
int Reauth(void);
const char *GetLocalUrlRepo(char *repo_name);
int EnsureDependencies(void);