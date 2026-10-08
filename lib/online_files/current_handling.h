#pragma once

#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "choice.h"
#include "get_documents.h"

void *ThreadExtractPages(void *arg);
void *ThreadExtractURL(void *arg);

char *GetCurrentRepo();

int PageDownload(char *url, char *file_name);
char **ExtractProjects(char *file_path, int *size);
char *ExtractRepo(char *url);