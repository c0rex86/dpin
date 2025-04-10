CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lncurses -lpthread -lssl -lcrypto -lcurl

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))
BIN = $(BIN_DIR)/dpin

.PHONY: all clean

all: dirs $(BIN)

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: all
	install -m 755 $(BIN) /usr/local/bin/dpin

uninstall:
	rm -f /usr/local/bin/dpin 