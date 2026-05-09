#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "get_documents.h"

int SubjectDownload(char* repo_name);
int CreateSubjectElements(char *repo_path, char *repo_name);
char* GetSubjectModules(char *subject_path);