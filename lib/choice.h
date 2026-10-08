#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

void __EnableRawMode(struct termios *orig);
void __DisableRawMode(struct termios *orig);
int ChoiceMCQ(const char *options[], int num_choices);