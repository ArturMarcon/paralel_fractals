#!/bin/bash
#SBATCH --job-name=julia_fractal
#SBATCH --nodes=4
#SBATCH --time=02:00:00
#SBATCH --exclusive
#SBATCH --output=sbatch_output.log
#SBATCH --error=sbatch_error.log

# Ensure we are in the right directory (SLURM normally uses the submission dir, but we enforce it)
cd /home/cp04/trabalho_4_new

# Run the experiments
./run_experiments.sh lad
