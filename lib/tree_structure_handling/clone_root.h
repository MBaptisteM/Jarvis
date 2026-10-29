#ifndef CLONE_ROOT_H
#define CLONE_ROOT_H

#include "ui.h"
#include "info_file.h"

#define SIZE_OF_STRING 512
#define ROOT_SEARCH_DEPTH 3

char *FindRootRepoNear(const char *name, int max_depth);
char *__GetGithubUsername(void);
int __RemoteRepoExists(const char *url);
char *__PromptForRepoUrl(void);
void __CloneParentRepo(void);

#endif