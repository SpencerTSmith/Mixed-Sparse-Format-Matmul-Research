#!/usr/bin/env bash

set -euo pipefail

REMOTE_DIR=/ourdisk/hpc/soonerhpclab/dont_archive/spencer03/Mixed-Sparse-Format-Matmul-Research

if [ -n "$(git status --porcelain)" ]; then
  echo "Uncommitted changes:" >&2
  git status --short >&2
  echo >&2
  read -r -p "Proceed? [y/N] " reply
  case "$reply" in
    [yY]|[yY][eE][sS]) ;;
    *) echo "Aborted." >&2; exit 1 ;;
  esac
fi

git push

ssh -O check oscer 2>/dev/null || ssh -fN oscer

JOBID=$(ssh oscer "cd $REMOTE_DIR && git pull -q && make clean >&2 && make taco_bullshit >&2 && sbatch --parsable taco_bench.sbatch")
JOBID=${JOBID%%;*}
echo "Submitted job $JOBID"

OUT=oscer/taco_bench_sweep_${JOBID}_stdout.txt

ssh -t oscer "cd $REMOTE_DIR && while [ ! -f $OUT ]; do sleep 2; done; tail -n +1 -f $OUT" &
TAIL_PID=$!

while [ -n "$(ssh oscer "squeue -h -j $JOBID")" ]; do
  sleep 10
done

sleep 3
kill $TAIL_PID 2>/dev/null || true

rsync -az "oscer:$REMOTE_DIR/taco_kron_results/" taco_kron_results/
