#!/usr/bin/env bash
set -euo pipefail

git push
ssh hpc 'cd /ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research && git pull && make clean && make && sbatch --wait job.sh'
rsync -av hpc:~/myproject/results/ ./results/
