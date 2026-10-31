#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>

#include "ui.h"

#define CHOICE_TICK_MS 100 // how often the state of the terminal is checked
#define CHOICE_ESC_MS 50 // wait for the rest of an arrow key (ESC [ A)

void __EnableRawMode(struct termios *orig);
void __DisableRawMode(struct termios *orig);
int ChoiceMCQ(const char *options[], int num_choices);