#pragma once

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

void EnableRawMode(struct termios *orig);
void DisableRawMode(struct termios *orig);
int ChoiceMCQ(const char *options[], size_t num_choices);