#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "given_files_handling.h"
#include "subject_handling.h"
#include "info_file.h"
#include "choice.h"
#include "tree_structure.h"
#include "get_documents.h"
#include "TPs_handler.h"
#include "current_handling.h"

#define SEMESTER "S"
#define BIMESTER "B"

void __TrimTrailing(char* str);
void __FindRoot(void);
void __FindCurrent(void);
void __FindRepository(char* repo_arg);
void __FindByKeyword(char* word);
int __ContainsWord(const char* haystack, const char* word);
void __SearchTree(const char* path, const char* word, int* found_any);

int __IsEpitaUrl(char* url);
void __ExtractRelativePath(char* url, char* semester_out, char* bimester_out);
int __ExtractStubborn(char* url, char* stubborn_out);
void __FindEpitaUrlInTree(char* url);