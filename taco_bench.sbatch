#!/bin/bash
#
#SBATCH --partition=el9_test
#SBATCH --container=el9hw
#SBATCH --ntasks=1
#SBATCH --mem=4G
#SBATCH --time=01:00:00
#SBATCH --job-name=taco_bench_sweep
#SBATCH --output=taco_bench_sweep_%J_stdout.txt
#SBATCH --error=taco_bench_sweep_%J_stderr.txt
#SBATCH --mail-user=spencer.smith-1@ou.edu
#SBATCH --mail-type=ALL
#SBATCH --chdir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research
#
##################################################
mkdir -p taco_kron_results/

./taco_bench.x \
    --out_dir=taco_kron_results \
    --kron_folder= /ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/AS-Newman \
    --seconds_to_try_for_min=1
