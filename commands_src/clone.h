#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>

#include "given_files_handling.h"
#include "subject_handling.h"
#include "info_file.h"
#include "choice.h"
#include "tree_structure.h"
#include "get_documents.h"
#include "TPs_handler.h"

#define SEMESTER "S"
#define BIMESTER "B"


int __IsEpitaRepo(char* repo_name);
char** __GetRelavitvePath(char *repo_name, size_t *size);
int __OneLayerFindOrCreate(char *path, char *name, int is_folder);
int __RenameRepo(char *repo_path, char *repo_name);
void __PrintPages(const char *local_url_repo);