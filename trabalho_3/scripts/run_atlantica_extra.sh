#!/usr/bin/env bash
set -euo pipefail

# Executa os casos extras pedidos no enunciado, exceto 1023.
# O caso 1023 fica em script separado para nao afetar os demais se travar.
#
# Uso:
#   bash scripts/run_atlantica_extra.sh 1000000 reverse 3
#
# Observacao:
#   Para esses processos extras, usamos --oversubscribe porque ha mais
#   processos MPI do que slots fisicos nos 2 nos.

cd "$(dirname "$0")/.."
make

N="${1:-1000000}"
INPUT="${2:-reverse}"
REPS="${3:-3}"
OUT_DIR="resultados"
OUT_FILE="$OUT_DIR/resultados_extra_N${N}_${INPUT}.log"

mkdir -p "$OUT_DIR"
: > "$OUT_FILE"

echo "inicio=$(date -Is) tamanho=$N entrada=$INPUT repeticoes=$REPS processos_extra=63,123,255,511" | tee -a "$OUT_FILE"

for np in 63 123 255 511; do
  for rep in $(seq 1 "$REPS"); do
    echo "rep=$rep tipo=mpi_arvore np=$np" | tee -a "$OUT_FILE"
    mpirun --oversubscribe -np "$np" ./bin/mpi_tree_sort "$N" "$INPUT" | tee -a "$OUT_FILE"
  done
done

for np in 63 123 255 511; do
  for rep in $(seq 1 "$REPS"); do
    echo "rep=$rep tipo=mpi_balanceado np=$np" | tee -a "$OUT_FILE"
    mpirun --oversubscribe -np "$np" ./bin/mpi_balanced_sort "$N" "$INPUT" | tee -a "$OUT_FILE"
  done
done

echo "fim=$(date -Is)" | tee -a "$OUT_FILE"
echo "Arquivo gerado: $OUT_FILE"
