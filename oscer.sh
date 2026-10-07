#!/usr/bin/env bash
set -euo pipefail

REMOTE_DIR=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research

git push

# make output goes to stderr so only the job id is captured
JOBID=$(ssh hpc "cd $REMOTE_DIR && git pull -q && make clean >&2 && make taco_bullshit >&2 && sbatch --parsable taco_bench.sbatch")
JOBID=${JOBID%%;*}
echo "Submitted job $JOBID"

OUT=taco_kron_results/taco_bench_sweep_${JOBID}_stdout.txt

# stream the log in the background (waits for the file to appear first)
ssh -t oscer "cd $REMOTE_DIR && while [ ! -f $OUT ]; do sleep 2; done; tail -n +1 -f $OUT" &
TAIL_PID=$!

# poll until the job leaves the queue
while [ -n "$(ssh hpc "squeue -h -j $JOBID")" ]; do
  sleep 10
done

sleep 3   # let the last output flush
kill $TAIL_PID 2>/dev/null || true

rsync -av oscer:$REMOTE_DIR/taco_kron_results/ ./taco_kron_results/
