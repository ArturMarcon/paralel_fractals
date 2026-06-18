#!/usr/bin/env bash
set -euo pipefail

# Executa todos os casos, exceto 1023:
# 1, 3, 7, 15, 31, 63, 123, 255 e 511 processos.
#
# Uso:
#   bash scripts/run_atlantica_all_no1023.sh 1000000 reverse 3
#
# Observacao:
#   A conquista e determinada pela disponibilidade de filhos/helpers na arvore,
#   nao por um delta fixo de tamanho.

cd "$(dirname "$0")/.."
make

N="${1:-1000000}"
INPUT="${2:-reverse}"
REPS="${3:-3}"
OUT_DIR="resultados"
OUT_FILE="$OUT_DIR/resultados_all_no1023_N${N}_${INPUT}.log"

mkdir -p "$OUT_DIR"
: > "$OUT_FILE"

echo "inicio=$(date -Is) tamanho=$N entrada=$INPUT repeticoes=$REPS processos=1,3,7,15,31,63,123,255,511" | tee -a "$OUT_FILE"

for rep in $(seq 1 "$REPS"); do
  echo "rep=$rep tipo=sequencial" | tee -a "$OUT_FILE"
  ./bin/sequential_bs "$N" "$INPUT" | tee -a "$OUT_FILE"
done

for np in 1 3 7 15 31 63 123 255 511; do
  for rep in $(seq 1 "$REPS"); do
    echo "rep=$rep tipo=mpi_arvore np=$np" | tee -a "$OUT_FILE"
    mpirun --oversubscribe -np "$np" ./bin/mpi_tree_sort "$N" "$INPUT" | tee -a "$OUT_FILE"
  done
done

for np in 1 3 7 15 31 63 123 255 511; do
  for rep in $(seq 1 "$REPS"); do
    echo "rep=$rep tipo=mpi_balanceado np=$np" | tee -a "$OUT_FILE"
    mpirun --oversubscribe -np "$np" ./bin/mpi_balanced_sort "$N" "$INPUT" | tee -a "$OUT_FILE"
  done
done

echo "fim=$(date -Is)" | tee -a "$OUT_FILE"
echo "Arquivo gerado: $OUT_FILE"
