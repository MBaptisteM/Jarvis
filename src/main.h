#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define COMMAND_HELP "help"


int main(int argc, char* argv[]);
const char *get_commands_path();