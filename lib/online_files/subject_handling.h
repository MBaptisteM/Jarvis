#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "get_documents.h"

int SubjectDownload(char* repo_name);
int CreateSubjectElements(char *repo_path, pid_t pid);
char* GetSubjectModules(char *subject_path);