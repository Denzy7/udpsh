CC = gcc
CFLAGS = -Wall
LDFLAGS =

SRC_DIR = .
OBJ_DIR = obj

SOCK_SRCS = udpsh_sock.c
SOCK_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SOCK_SRCS))

CLIENT_SRC = udpsh_client.c
CLIENT_OBJ = $(OBJ_DIR)/udpsh_client.o

SERVER_SRC = udpsh_server.c
SERVER_OBJ = $(OBJ_DIR)/udpsh_server.o

.PHONY: all clean

all: udpsh-client udpsh-server

udpsh-client: $(CLIENT_OBJ) $(SOCK_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

udpsh-server: $(SERVER_OBJ) $(SOCK_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) udpsh-client udpsh-server
