CC = gcc
CFLAGS = -O3 -Wall -Wextra -Iinclude
OMP_FLAGS = -fopenmp
MATH_FLAGS = -lm

# MS-MPI para Windows (paquete mingw-w64-x86_64-msmpi)
# Headers y libs se instalan en las rutas estandar de MinGW64
MPI_FLAGS = -lmsmpi

BIN_DIR = bin
SRC_DIR = src

TARGETS = $(BIN_DIR)/cartoon_seq $(BIN_DIR)/cartoon_omp $(BIN_DIR)/cartoon_mpi $(BIN_DIR)/cartoon_hybrid

all: $(BIN_DIR) $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/cartoon_seq: $(SRC_DIR)/cartoon_seq.c include/ppm_utils.h include/cartoon_core.h
	$(CC) $(CFLAGS) $< -o $@ $(MATH_FLAGS)

$(BIN_DIR)/cartoon_omp: $(SRC_DIR)/cartoon_omp.c include/ppm_utils.h include/cartoon_core.h
	$(CC) $(CFLAGS) $(OMP_FLAGS) $< -o $@ $(MATH_FLAGS)

$(BIN_DIR)/cartoon_mpi: $(SRC_DIR)/cartoon_mpi.c include/ppm_utils.h include/cartoon_core.h
	$(CC) $(CFLAGS) $< -o $@ $(MPI_FLAGS) $(MATH_FLAGS)

$(BIN_DIR)/cartoon_hybrid: $(SRC_DIR)/cartoon_hybrid.c include/ppm_utils.h include/cartoon_core.h
	$(CC) $(CFLAGS) $(OMP_FLAGS) $< -o $@ $(MPI_FLAGS) $(MATH_FLAGS)

clean:
	rm -rf $(BIN_DIR) *.ppm

.PHONY: all clean
