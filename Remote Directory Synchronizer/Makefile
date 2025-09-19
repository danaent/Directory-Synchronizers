# Compiler
CC = gcc
CFLAGS = -g

SRC_DIR = src
OBJ_DIR = obj

# Manager files
SRC_M = ./src/nfs_manager_main.c ./src/nfs_manager.c ./src/util.c ./src/socket_util.c ./src/sync_buffer.c ./src/logfile.c
OBJ_M = nfs_manager_main.o nfs_manager.o util.o socket_util.o sync_buffer.o logfile.o
EXEC_M = nfs_manager

# Client files
SRC_C = ./src/nfs_client.c ./src/util.c ./src/socket_util.c
OBJ_C = nfs_client.o util.o socket_util.o
EXEC_C = nfs_client

# Console files
SRC_CN = ./src/nfs_console.c ./src/util.c ./src/socket_util.c
OBJ_CN = nfs_console.o util.o socket_util.o
EXEC_CN = nfs_console

# All
all: $(EXEC_M) $(EXEC_C) $(EXEC_CN) clean

# Manager executable
$(EXEC_M): $(OBJ_M)
	$(CC) $^ -o $@ -lpthread
	rm -rf $(OBJ_M)

# Client executable
$(EXEC_C): $(OBJ_C)
	$(CC) $^ -o $@ -lpthread
	rm -rf $(OBJ_C)

# Console executable
$(EXEC_CN): $(OBJ_CN)
	$(CC) $^ -o $@ -lpthread
	rm -rf $(OBJ_CN)

# Compile files separately
%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $^ -o $@

# Remove object files
clean:
	rm -rf $(OBJ_M) $(OBJ_C) $(OBJ_CN)