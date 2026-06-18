MPICC = mpicc
CC = gcc
CFLAGS = -Wall -Wextra -std=c99

BIN_DIR = bin
SRC_DIR = src

all: $(BIN_DIR)/sequential_bs $(BIN_DIR)/mpi_tree_sort $(BIN_DIR)/mpi_balanced_sort

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/sequential_bs: $(SRC_DIR)/sequential_bs.c $(SRC_DIR)/common_sort.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/mpi_tree_sort: $(SRC_DIR)/mpi_tree_sort.c $(SRC_DIR)/common_sort.h | $(BIN_DIR)
	$(MPICC) $(CFLAGS) $< -o $@

$(BIN_DIR)/mpi_balanced_sort: $(SRC_DIR)/mpi_balanced_sort.c $(SRC_DIR)/common_sort.h | $(BIN_DIR)
	$(MPICC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
