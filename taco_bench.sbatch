#!/bin/bash
#
#SBATCH --partition=normal,sooner_test
#SBATCH --container=el9hw
#SBATCH --ntasks=1
#SBATCH --mem=128G
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

ulimit -c unlimited

COREDIR=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/core_dumps
mkdir -p "$COREDIR"

echo "core_pattern: $(cat /proc/sys/kernel/core_pattern)"
echo "ulimit -c: $(ulimit -c)"

cat /proc/sys/kernel/perf_event_paranoid

cd "$COREDIR"

/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_bench.x \
    --out_dir=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/taco_kron_results \
    --kron_folder=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research/krons/AS-Newman

EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "taco_bench.x exited with code $EXIT_CODE"

    # Case 1: plain core_pattern wrote directly into $COREDIR
    ls -la "$COREDIR"

    # Case 2: this node uses systemd-coredump (pattern starts with a pipe to
    # systemd-coredump). If so, the file above won't exist -- pull it out
    # of the systemd store instead.
    if command -v coredumpctl >/dev/null 2>&1; then
        coredumpctl dump taco_bench.x -o "$COREDIR/core.taco_bench.$SLURM_JOB_ID" 2>/dev/null \
            && echo "Extracted core file via coredumpctl"
    fi
fi
