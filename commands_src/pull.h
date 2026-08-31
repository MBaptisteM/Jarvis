#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "TPs_handler.h"
#include "info_file.h"

int main(int argc, char* argv[]);
void __PullRepo(char* repo_path);
void __PullMain(void);
void __PullCurrent(void);
void __PullRepository(char* repo_arg);
