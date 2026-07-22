#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>

#include "TPs_handler.h"
#include "info_file.h"

#define COMMAND_TAG "tag="
#define COMMAND_NAME "name="

int main(int argc, char* argv[]);
void __GetNameCommit(char** name_commit, char* path);
void __GetTagID(char** tag, char* path);
void __GetId(char** entry, char* path);