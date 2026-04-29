#!/bin/bash
#
#SBATCH --partition=el9_test
#SBATCH --container=el9hw
#SBATCH --ntasks=1
#SBATCH --mem=4G
#SBATCH --time=01:00:00
#SBATCH --job-name=spmm_runtime_sweep
#SBATCH --output=spmm_sweep_%J_stdout.txt
#SBATCH --error=spmm_sweep_%J_stderr.txt
#SBATCH --mail-user=spencer.smith-1@ou.edu
#SBATCH --mail-type=ALL
#SBATCH --chdir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research
#
##################################################

mkdir -p runtime_sweep/

# make no_observe

for right_density in 0.1 0.7 0.9; do
    ./reptest.x \
        --out_dir=runtime_sweep \
        --sweep=left \
        --fixed_density=$right_density \
        --row_count=32 --col_count=32 --inner_count=32
done

for left_density in 0.1 0.7 0.9; do
    ./reptest.x \
        --out_dir=runtime_sweep \
        --sweep=right \
        --fixed_density=$left_density \
        --row_count=32 --col_count=32 --inner_count=32
done
