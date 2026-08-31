#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <strings.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define BOLD    "\033[1m"

#define COMMAND_HELP "help"
#define COMMAND_CLONE "clone"
#define COMMAND_AUTH "auth"
#define COMMAND_UNINSTALL "uninstall"
#define COMMAND_PULL "pull"
#define COMMAND_FIND "find"

void Help();
void HelpClone();
void HelpUninstall();
void HelpAuth();
void HelpPull();
void HelpFind();