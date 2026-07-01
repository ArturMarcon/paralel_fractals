#!/bin/bash
#SBATCH --job-name=julia_db
#SBATCH --nodes=4
#SBATCH --time=01:00:00
#SBATCH --exclusive
#SBATCH --output=sbatch_output_db.log
#SBATCH --error=sbatch_error_db.log

# Ensure we are in the right directory
cd /home/cp04/trabalho_4_new

# Compile
make -f Makefile.lad clean
make -f Makefile.lad julia_dive_hybrid_db

# Make script executable
chmod +x run_experiments_db.sh

# Run the experiments
./run_experiments_db.sh lad
