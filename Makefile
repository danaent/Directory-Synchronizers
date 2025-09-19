# Compiler
CC = gcc
CFLAGS = -w -g

SRC_DIR = src
OBJ_DIR = obj

# Manager files
SRC_M = ./src/fss_manager_main ./src/fss_manager.c ./src/job_queue.c ./src/worker_management.c ./src/int_queue.c ./src/util.c ./src/file_monitor.c
OBJ_M = fss_manager_main.o fss_manager.o job_queue.o worker_management.o int_queue.o util.o file_monitor.o
EXEC_M = fss_manager

# Worker files
SCR_W = ./src/worker.c ./src/util.c
OBJ_W = worker.o  util.o
EXEC_W = worker

# Console files
SRC_C = ./src/fss_console.c ./src/util.c
OBJ_C = fss_console.o util.o
EXEC_C = fss_console

# All
all: $(EXEC_M) $(EXEC_W) $(EXEC_C) clean

# Manager executable
$(EXEC_M): $(OBJ_M)
	$(CC) $^ -o $@

# Worker executable
$(EXEC_W): $(OBJ_W)
	$(CC) $^ -o $@

# Console executable
$(EXEC_C): $(OBJ_C)
	$(CC) $^ -o $@

# Compile files separately
%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $^ -o $@

# Remove object files
clean:
	rm -rf $(OBJ_M) $(OBJ_W) $(OBJ_C)

# Run executable with valgrind
# help: $(EXEC)
# 	valgrind --leak-check=full -v --show-leak-kinds=all --track-origins=yes ./$(EXEC)