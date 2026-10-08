#pragma once

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
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

void __TrimTrailing(char *str);
void __FindRoot(void);
void __FindCurrent(void);
void __FindRepository(char *repo_arg);
void __FindByKeyword(char *word);
int __ContainsWord(const char *haystack, const char *word);
void __SearchTree(const char *path, const char *word, int *found_any);

int __IsEpitaUrl(char *url);
void __ExtractRelativePath(char *url, char *semester_out, char *bimester_out);
int __ExtractStubborn(char *url, char *stubborn_out);
void __FindEpitaUrlInTree(char *url);