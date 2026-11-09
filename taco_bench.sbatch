#!/bin/bash
#
#SBATCH --partition=normal,sooner_test
#SBATCH --container=el9hw
#SBATCH --ntasks=1
#SBATCH --mem=128G
#SBATCH --time=8:00:00
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

SAMPLE=$1

BLOCKING="--blocking"

# Answers
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
  --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
  --k_min=0 --k_max=23 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
  --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Answers

# AS-Newman
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=22 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/AS-Newman

# AS-RouteViews
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=23 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/AS-RouteViews

# ATP-GR-QC
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
  --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
  --k_min=0 --k_max=23 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
  --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/ATP-GR-QC

# Bio-Proteins
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=22 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Bio-Proteins

# Blog-Nat05-6m
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=21 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Blog-Nat05-6m

# Blog-Nat06all
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=21 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Blog-Nat06all

# CA-DBLP
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=22 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/CA-DBLP

# CA-GR-QC
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=22 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/CA-GR-QC

# CA-Hep-Ph
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=21 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/CA-Hep-Ph

# CA-Hep-Th
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=23 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/CA-Hep-Th

# Cit-Hep-Ph
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=21 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Cit-Hep-Ph

# Cit-Hep-Th
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=21 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Cit-Hep-Th

# Delicious
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=23 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Delicious

# Email-Inside
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=18 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Email-Inside

# Epinions
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=23 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Epinions

# Flickr
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=23 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Flickr

# Gnutella-25
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Gnutella-25

# Gnutella-30
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=24 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Gnutella-30

# Web-Notredame
/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --k_min=0 --k_max=23 --oscer --s=$SAMPLE $BLOCKING --node_name=$NODE_NAME \
    --kron_dir=/ourdisk/hpc/soonerhpclab/dont_archive/moezdurrani/data/Web-Notredame

EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "taco_bench.x exited with code $EXIT_CODE"

    if command -v coredumpctl >/dev/null 2>&1; then
        coredumpctl dump taco_bench.x -o "$COREDIR/core.taco_bench.$SLURM_JOB_ID" 2>/dev/null \
            && echo "Extracted core file via coredumpctl"
    fi
fi
