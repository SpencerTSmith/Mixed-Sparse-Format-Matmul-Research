import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

def formula_dense_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LRC * LCC * RCC
    memops = (2 * LRC * LCC * RCC) + (LRC * RCC)
    return flops, memops

def formula_csr_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LNZ * RCC
    memops = (2 * LRC) + (2 * LNZ) + (3 * LNZ * RCC)
    return flops, memops

def formula_csc_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LNZ * RCC
    memops = (2 * LCC) + (2 * LNZ) + (3 * LNZ * RCC)
    return flops, memops

def formula_csr_csr(LRC, LCC, RCC, LNZ, RNZ):
    # RNZ / RCC works for estimating the average non-zeroes in rows
    flops = (2 * LNZ * RNZ / RCC)
    memops = (2 * LRC) + (4 * LNZ) + (4 * LNZ * RNZ/RCC)
    return flops, memops

formula_map = {
    'dense_X_dense': formula_dense_dense,
    'csr_X_dense':   formula_csr_dense,
    'csc_X_dense':   formula_csc_dense,
    'csr_X_csr':     formula_csr_csr,
}

csv_files = sys.argv[1:]

plt.figure(figsize=(12,5))

for csv_file in csv_files:
    data = pd.read_csv(csv_file)

    row_count = data['row_count'].iloc[0]
    col_count = data['col_count'].iloc[0]
    densities = data['density'].values

    observed_flops  = data['flops'].values
    observed_memops = data['memops'].values

    formula_func = None
    for key, func in formula_map.items():
        if key in csv_file:
            formula_func = func
            break

    if not formula_func:
        print(f"No formula defined for file {csv_file}")
        exit()

    # NOTE: We are working with only completely square matrices
    LRC = row_count
    LCC = col_count
    RRC = row_count
    RCC = col_count

    # TODO: Probably some error, should probably just include nnz in csv
    LNZ = LRC * LCC * densities
    RNZ = RRC * RCC * densities

    formula_flops, formula_memops = formula_func(LRC, LCC, RCC, LNZ, RNZ)

    # If we produce a single value nad not a series
    if np.isscalar(formula_flops):
        formula_flops = np.full_like(densities, formula_flops)
    if np.isscalar(formula_memops):
        formula_memops = np.full_like(densities, formula_memops)


    plt.subplot(1,2,1)
    plt.plot(densities, observed_flops, 'o', label=f'{csv_file}: Observed', markersize=4)
    plt.plot(densities, formula_flops, '-', label=f'{csv_file}: Formula', linewidth=2)
    plt.xlabel('Density')
    plt.ylabel('Flops')
    plt.title('Flops')
    plt.legend()
    plt.grid(True)

    plt.subplot(1,2,2)
    plt.plot(densities, observed_memops, 'o', label=f'{csv_file}: Observed', markersize=4)
    plt.plot(densities, formula_memops, '-', label=f'{csv_file}: Formula', linewidth=2)
    plt.xlabel('Density')
    plt.ylabel('Memops')
    plt.title('Memops')
    plt.legend()
    plt.grid(True)

plt.tight_layout()
plt.show()
plt.savefig("plot.png")
