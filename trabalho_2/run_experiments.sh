#!/usr/bin/env bash
# Run strong and weak scaling experiments and emit a CSV ready for analysis.
#
# Compile first:
#   local: mpicc -O3 -Wall -o julia_dive julia_dive.c -lm
#          cc    -O3 -Wall -o julia_dive_seq julia_dive_seq.c -lm
#   lad:   ladcomp -env mpicc -O3 -Wall julia_dive.c -o julia_dive -lm
#          gcc -O3 -Wall julia_dive_seq.c -o julia_dive_seq -lm
#   (-O3 is CRITICAL: without it, ladcomp/mpicc defaults to -O0 and the
#    parallel binary runs ~2.5x slower than the sequential, falsifying speedup.)
#
# Usage:
#   ./run_experiments.sh local   # Mac/dev box validation (mpirun, smaller workload)
#   ./run_experiments.sh lad     # PUCRS LAD atlantica (srun, full workload)
#
# IMPORTANT (lad mode): run inside a salloc allocation so all srun calls
# share the same exclusive nodes (consistent hardware across reps):
#   salloc --exclusive -N 2 -t 00:30:00
#   ./run_experiments.sh lad
#   exit
#
# Tunables (env vars): REPS, FRAMES_STRONG, FRAMES_PER_WORKER
# Output: results_<mode>.csv

set -euo pipefail

MODE="${1:-local}"
REPS="${REPS:-3}"

case "$MODE" in
  local)
    NP_LIST=(2 4 8)
    FRAMES_STRONG="${FRAMES_STRONG:-60}"
    FRAMES_PER_WORKER="${FRAMES_PER_WORKER:-10}"
    LAUNCHER=(mpirun -np)
    SEQ_PREFIX=()
    ;;
  lad)
    # PUCRS LAD atlantica. Assumes you are running INSIDE a salloc allocation
    # (see header comment) — node count, exclusivity and walltime come from
    # the surrounding salloc, so each srun just specifies -n.
    # 16 and 32 are the mandatory points; intermediates fill the speedup curve.
    # SEQ_PREFIX forces the sequential baseline to run on a COMPUTE node too,
    # otherwise it executes on the (faster) login node and falsifies T_seq.
    NP_LIST=(2 4 8 16 32)
    FRAMES_STRONG="${FRAMES_STRONG:-300}"
    FRAMES_PER_WORKER="${FRAMES_PER_WORKER:-20}"
    LAUNCHER=(srun -N 2 -n)      # force tasks spread across both nodes;
                                 # without -N 2, srun packs onto one node and
                                 # master+worker collide on hyperthreads.
    SEQ_PREFIX=(srun -n 1 -N 1)  # -N 1 silences "1 proc on 2 nodes" warning
    ;;
  *)
    echo "usage: $0 [local|lad]" >&2
    exit 1
    ;;
esac

OUT="results_${MODE}.csv"
echo "experiment,rep,kind,np,workers,total_frames,total_s,io_s,compute_max_s,compute_min_s,imbalance_pct" > "$OUT"

run_seq() {
  local exp="$1" rep="$2" frames="$3"
  echo "[run] $exp rep=$rep seq frames=$frames"
  local line
  line=$("${SEQ_PREFIX[@]}" ./julia_dive_seq -0.1 -0.1 "$frames" 0 | grep '^CSV,' | sed 's/^CSV,//')
  echo "$exp,$rep,$line" >> "$OUT"
}

run_par() {
  local exp="$1" rep="$2" np="$3" frames="$4"
  echo "[run] $exp rep=$rep np=$np frames=$frames"
  local line
  line=$("${LAUNCHER[@]}" "$np" ./julia_dive -0.1 -0.1 "$frames" 0 | grep '^CSV,' | sed 's/^CSV,//')
  echo "$exp,$rep,$line" >> "$OUT"
}

echo "=== Strong scaling (frames=$FRAMES_STRONG) ==="
for rep in $(seq 1 "$REPS"); do
  run_seq strong "$rep" "$FRAMES_STRONG"
  for np in "${NP_LIST[@]}"; do
    run_par strong "$rep" "$np" "$FRAMES_STRONG"
  done
done

echo "=== Weak scaling (~$FRAMES_PER_WORKER frames/worker) ==="
for rep in $(seq 1 "$REPS"); do
  for np in "${NP_LIST[@]}"; do
    workers=$((np - 1))
    frames=$((workers * FRAMES_PER_WORKER))
    run_par weak "$rep" "$np" "$frames"
  done
done

echo
echo "Done. Results in: $OUT"
