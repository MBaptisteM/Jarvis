#ifndef CLONE_ROOT_H
#define CLONE_ROOT_H

#include "ui.h"

char *__GetGithubUsername(void);
int __RemoteRepoExists(const char *url);
char *__PromptForRepoUrl(void);
void __CloneParentRepo(void);

#endif