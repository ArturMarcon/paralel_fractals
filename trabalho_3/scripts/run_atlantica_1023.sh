#!/usr/bin/env bash
set -euo pipefail

# Executa somente o caso extremo com 1023 processos.
# Fica separado porque o oversubscribe de 1023 processos em 2 nos pode demorar
# muito ou ficar inviavel no ambiente.
#
# Uso:
#   bash scripts/run_atlantica_1023.sh 1000000 reverse 1

cd "$(dirname "$0")/.."
make

N="${1:-1000000}"
INPUT="${2:-reverse}"
REPS="${3:-1}"
OUT_DIR="resultados"
OUT_FILE="$OUT_DIR/resultados_1023_N${N}_${INPUT}.log"

mkdir -p "$OUT_DIR"
: > "$OUT_FILE"

echo "inicio=$(date -Is) tamanho=$N entrada=$INPUT repeticoes=$REPS processos=1023" | tee -a "$OUT_FILE"

for rep in $(seq 1 "$REPS"); do
  echo "rep=$rep tipo=mpi_arvore np=1023" | tee -a "$OUT_FILE"
  mpirun --oversubscribe -np 1023 ./bin/mpi_tree_sort "$N" "$INPUT" | tee -a "$OUT_FILE"
done

for rep in $(seq 1 "$REPS"); do
  echo "rep=$rep tipo=mpi_balanceado np=1023" | tee -a "$OUT_FILE"
  mpirun --oversubscribe -np 1023 ./bin/mpi_balanced_sort "$N" "$INPUT" | tee -a "$OUT_FILE"
done

echo "fim=$(date -Is)" | tee -a "$OUT_FILE"
echo "Arquivo gerado: $OUT_FILE"
