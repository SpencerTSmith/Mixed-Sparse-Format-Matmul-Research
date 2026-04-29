#!/bin/bash

mkdir -p runtime_sweep/

# make no_observe

for right_density in 0.1 0.7 0.9; do
    ./reptest.x \
        --out_dir=runtime_sweep\
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
