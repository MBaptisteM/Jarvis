#pragma once

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "TPs_handler.h"
#include "choice.h"
#include "current_handling.h"
#include "get_documents.h"
#include "given_files_handling.h"
#include "info_file.h"
#include "subject_handling.h"
#include "tree_structure.h"

#define SEMESTER "S"
#define BIMESTER "B"

int __SameStr(char *s1, char *s2);
int __IsEpitaRepo(char *repo_name);
char **__GetRelavitvePath(char *repo_name, size_t *size);
int __OneLayerFindOrCreate(char *path, char *name, int is_folder);
int __RenameRepo(char *repo_path, char *repo_name);
void __PrintPages(const char *local_url_repo);