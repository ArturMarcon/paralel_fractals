#!/usr/bin/env bash
# Run scaling experiments for double buffered hybrid

set -euo pipefail

MODE="${1:-local}"
REPS=3

OUT_LOG="resultados_db.log"
echo "--- Início dos Experimentos Double Buffered ---" > "$OUT_LOG"

if [ "$MODE" = "lad" ]; then
    LAUNCH_MPI="srun --mpi=pmix"
else
    LAUNCH_MPI="mpirun --oversubscribe"
fi

WORKERS_LIST=(1 2 4 8 16 32 64)
FRAMES_STRONG=128

for rep in $(seq 1 $REPS); do
    echo ">> Repetição $rep" | tee -a "$OUT_LOG"
    
    # ------------------ HÍBRIDO DOUBLE BUFFERED ------------------
    for W in "${WORKERS_LIST[@]}"; do
        if [ "$W" -le 8 ]; then
            N=1
        elif [ "$W" -eq 16 ]; then
            N=2
        else
            N=4
        fi
        T=$(( W / N ))
        NP_HYBRID=$(( N + 1 ))
        
        export OMP_NUM_THREADS=$T
        if [ "$MODE" = "lad" ]; then
            out=$(srun --mpi=pmix -n $NP_HYBRID -N $N ./julia_dive_hybrid_db -0.1 -0.1 $FRAMES_STRONG 0 | grep "^CSV") || true
        else
            out=$($LAUNCH_MPI -np $NP_HYBRID ./julia_dive_hybrid_db -0.1 -0.1 $FRAMES_STRONG 0 | grep "^CSV") || true
        fi
        
        if [ -n "$out" ]; then
            total_s=$(echo "$out" | cut -d',' -f7)
            echo "Hibrido DB, Nos: $N, Threads: $T, Tempo: ${total_s}s" | tee -a "$OUT_LOG"
        fi
    done
done

echo "--- Fim dos Experimentos Double Buffered ---" >> "$OUT_LOG"
