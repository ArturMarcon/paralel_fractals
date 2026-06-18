#!/usr/bin/env bash
set -euo pipefail

# Uso sugerido no Atlantica:
#   salloc --exclusive -N 2
#   bash scripts/run_atlantica.sh 10000 reverse 3
#
# Parametros:
#   $1 tamanho do vetor
#   $2 entrada: reverse, random ou almost
#   $3 repeticoes por configuracao

cd "$(dirname "$0")/.."
make

N="${1:-10000}"
INPUT="${2:-reverse}"
REPS="${3:-3}"
OUT_DIR="resultados"
OUT_FILE="$OUT_DIR/resultados_N${N}_${INPUT}.log"

mkdir -p "$OUT_DIR"
: > "$OUT_FILE"

echo "inicio=$(date -Is) tamanho=$N entrada=$INPUT repeticoes=$REPS" | tee -a "$OUT_FILE"

for rep in $(seq 1 "$REPS"); do
  echo "rep=$rep tipo=sequencial" | tee -a "$OUT_FILE"
  ./bin/sequential_bs "$N" "$INPUT" | tee -a "$OUT_FILE"
done

for np in 1 3 7 15 31; do
  for rep in $(seq 1 "$REPS"); do
    echo "rep=$rep tipo=mpi_arvore np=$np" | tee -a "$OUT_FILE"
    mpirun -np "$np" ./bin/mpi_tree_sort "$N" "$INPUT" | tee -a "$OUT_FILE"
  done
done

for np in 1 3 7 15 31; do
  for rep in $(seq 1 "$REPS"); do
    echo "rep=$rep tipo=mpi_balanceado np=$np" | tee -a "$OUT_FILE"
    mpirun -np "$np" ./bin/mpi_balanced_sort "$N" "$INPUT" | tee -a "$OUT_FILE"
  done
done

echo "fim=$(date -Is)" | tee -a "$OUT_FILE"
echo "Arquivo gerado: $OUT_FILE"
