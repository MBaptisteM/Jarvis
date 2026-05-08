#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include "info_file.h"
#include "choice.h"
#include "tree_structure.h"
#include "get_subject.h"

#define SEMESTER "S"
#define BIMESTER "B"
#define MAIN_FOLDER "TPs"
#define TEMPORARY_FOLDER "tmp"


int __IsEpitaRepo(char* repo_name);
char** __GetRelavitvePath(char *repo_name, size_t *size);
char* __GetOrCreateMainFolderPath();
int __OneLayerFindOrCreate(char *path, char *name, int is_folder);
int __GivenFilesHandling(char* repo_name);
int __SubjectHandling(char* repo_name);
int __UncompressGivenFiles(char *folder_path, char* repo_name);
int __GivenFilesCopy(char *path_folder, char *targer_folder);
int __AddGivenFilesUsefulParts(char *folder_path, char* repo_name);
int __CreateSubjectElements(char *repo_path, char *repo_name);
char* __GetSubjectModules(char *subject_path);
int __RenameRepo(char *repo_path, char *repo_name);
void __PrintPages(const char *local_url_repo);