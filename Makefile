CC = gcc
CFLAGS = -Wall -Wextra -O2 -Ilib

SRC_PATH = src
BIN_PATH = bin
CMD_SRC_PATH = commands_src
CMD_PATH = commands
LIB_PATH = lib

MAIN_SRC = $(SRC_PATH)/main.c
MAIN_BIN = $(BIN_PATH)/jarvis

LIB_SRCS = $(wildcard $(LIB_PATH)/*.c)
LIB_OBJS = $(LIB_SRCS:.c=.o)

CMD_SRCS = $(wildcard $(CMD_SRC_PATH)/*.c)
CMD_BINS = $(patsubst $(CMD_SRC_PATH)/%.c,$(CMD_PATH)/%,$(CMD_SRCS))


all: $(MAIN_BIN) $(CMD_BINS)


$(MAIN_BIN): $(MAIN_SRC) $(LIB_OBJS)
	mkdir -p $(BIN_PATH)
	$(CC) $(CFLAGS) $^ -o $@


$(LIB_PATH)/%.o: $(LIB_PATH)/%.c
	$(CC) $(CFLAGS) -c $< -o $@


$(CMD_PATH)/%: $(CMD_SRC_PATH)/%.c $(LIB_OBJS)
	mkdir -p $(CMD_PATH)
	$(CC) $(CFLAGS) $^ -o $@


clean:
	rm -f $(MAIN_BIN)
	rm -f $(CMD_BINS)
	rm -f $(LIB_OBJS)

.PHONY: all clean