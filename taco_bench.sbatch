#!/bin/bash
#
#SBATCH --partition=normal,sooner_test
#SBATCH --container=el9hw
#SBATCH --ntasks=1
#SBATCH --mem=128G
#SBATCH --time=3:00:00
#SBATCH --job-name=taco_bench_sweep
#SBATCH --output=oscer/taco_bench_sweep_%J_stdout.txt
#SBATCH --error=oscer/taco_bench_sweep_%J_stderr.txt
#SBATCH --mail-user=spencer.smith-1@ou.edu
#SBATCH --mail-type=ALL
#SBATCH --chdir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research
#
##################################################
mkdir -p taco_kron_results/

ulimit -c unlimited

echo "ulimit -c: $(ulimit -c)"

COREDIR=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/core_dumps
mkdir -p "$COREDIR"

cd "$COREDIR"

NODE_NAME=$(hostname -s)

# AS-Newman
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/AS-Newman

# Answers
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Answers

# AS-RouteViews
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/AS-RouteViews

# Delicious
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Delicious

# Flickr
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Flickr

# Bio-Proteins
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Bio-Proteins

# Web-Notredame
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Web-Notredame

EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "taco_bench.x exited with code $EXIT_CODE"

    if command -v coredumpctl >/dev/null 2>&1; then
        coredumpctl dump taco_bench.x -o "$COREDIR/core.taco_bench.$SLURM_JOB_ID" 2>/dev/null \
            && echo "Extracted core file via coredumpctl"
    fi
fi
