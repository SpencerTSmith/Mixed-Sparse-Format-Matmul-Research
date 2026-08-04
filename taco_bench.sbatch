#!/bin/bash
#
#SBATCH --partition=normal,sooner_test
#SBATCH --container=el9hw
#SBATCH --ntasks=1
#SBATCH --mem=32G
#SBATCH --time=01:00:00
#SBATCH --job-name=taco_bench_sweep
#SBATCH --output=taco_bench_sweep_stdout.txt
#SBATCH --error=taco_bench_sweep_stderr.txt
#SBATCH --mail-user=spencer.smith-1@ou.edu
#SBATCH --mail-type=ALL
#SBATCH --chdir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research
#
##################################################
mkdir -p taco_kron_results/

cat /proc/sys/kernel/perf_event_paranoid

./taco_bench.x --out_dir=taco_kron_results --kron_folder=krons/AS-Newman
