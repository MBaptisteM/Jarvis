CC = gcc
CFLAGS = -Wall -Wextra -O2

SRC_PATH = src
BIN_PATH = bin
CMD_SRC_PATH = commands_src
CMD_PATH = commands


MAIN_SRC = $(SRC_PATH)/main.c
MAIN_BIN = $(BIN_PATH)/jarvis


CMD_SRCS = $(wildcard $(CMD_SRC_PATH)/*.c)

CMD_BINS = $(patsubst $(CMD_SRC_PATH)/%.c,$(CMD_PATH)/%,$(CMD_SRCS))


all: $(MAIN_BIN) $(CMD_BINS)


$(MAIN_BIN): $(MAIN_SRC)
	mkdir -p $(BIN_PATH)
	$(CC) $(CFLAGS) $< -o $@

$(CMD_PATH)/%: $(CMD_SRC_PATH)/%.c
	mkdir -p $(CMD_PATH)
	$(CC) $(CFLAGS) $< -o $@


clean:
	rm -rf $(BIN_PATH)/* $(CMD_PATH)/*

.PHONY: all clean