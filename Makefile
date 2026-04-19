CC = gcc
CFLAGS = -Wall -Wextra -O2 -Ilib -Wextra

SRC_PATH = src
BIN_PATH = bin
CMD_SRC_PATH = commands_src
CMD_PATH = commands

LIB_SRC = lib/info_file.c
LIB_HDR = lib/info_file.h

LIB_OBJ = lib/info_file.o


MAIN_SRC = $(SRC_PATH)/main.c
MAIN_BIN = $(BIN_PATH)/jarvis


CMD_SRCS = $(wildcard $(CMD_SRC_PATH)/*.c)

CMD_BINS = $(patsubst $(CMD_SRC_PATH)/%.c,$(CMD_PATH)/%,$(CMD_SRCS))


all: $(MAIN_BIN) $(CMD_BINS)


$(MAIN_BIN): $(MAIN_SRC) $(SRC_PATH)/main.h
	mkdir -p $(BIN_PATH)
	$(CC) $(CFLAGS) $< -o $@

$(LIB_OBJ): $(LIB_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(CMD_PATH)/%: $(CMD_SRC_PATH)/%.c $(LIB_OBJ)
	mkdir -p $(CMD_PATH)
	$(CC) $(CFLAGS) $< $(LIB_OBJ) -o $@


clean:
	rm -f $(MAIN_BIN)
	rm -f $(CMD_BINS)

.PHONY: all clean