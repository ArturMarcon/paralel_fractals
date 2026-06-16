#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
make

N="${1:-2000}"
DELTA="${2:-500}"
INPUT="${3:-reverse}"

./bin/sequential_bs "$N" "$INPUT"
mpirun -np 1 ./bin/mpi_tree_sort "$N" "$DELTA" "$INPUT"
mpirun -np 3 ./bin/mpi_tree_sort "$N" "$DELTA" "$INPUT"
mpirun -np 3 ./bin/mpi_balanced_sort "$N" "$DELTA" "$INPUT"
