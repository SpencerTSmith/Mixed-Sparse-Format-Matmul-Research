import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

def formula_dense_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LRC * LCC * RCC
    memops = (2 * LRC * LCC * RCC) + (LRC * RCC)
    return flops, memops

def formula_dense_csr(LRC, LCC, RCC, LNZ, RNZ):
    return 0, 0

def formula_dense_csc(LRC, LCC, RCC, LNZ, RNZ):
    return 0, 0

def formula_csr_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LNZ * RCC
    memops = (2 * LRC) + (2 * LNZ) + (3 * LNZ * RCC)
    return flops, memops

def formula_csr_csr(LRC, LCC, RCC, LNZ, RNZ):
    # THE SAME!
    left_density = LNZ / (LRC * LCC)
    right_density = RNZ / (LCC * RCC)
    expected_work = left_density * right_density * LRC * RCC * RRC

    # THE SAME!
    # RNZ / RRC works for estimating the average non-zeroes in rows
    flops = (2 * LNZ * RNZ / RRC)
    memops = (2 * LRC) + (4 * LNZ) + (4 * LNZ * RNZ / RRC)
    return flops, memops

def formula_csr_csc(LRC, LCC, RCC, LNZ, RNZ):
    return 0, 0

def formula_csc_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LNZ * RCC
    memops = (2 * LCC) + (2 * LNZ) + (3 * LNZ * RCC)
    return flops, memops

def formula_csc_csr(LRC, LCC, RCC, LNZ, RNZ):
    return 0, 0

def formula_csc_csc(LRC, LCC, RCC, LNZ, RNZ):
    flops = (2 * RNZ * LNZ / LCC)
    memops = (2 * RCC) + (4 * RNZ) + (4 * RNZ * LNZ / LCC)
    return flops, memops

formula_map = {
    'dense_X_dense': formula_dense_dense,
    'dense_X_csr':   formula_dense_csr,
    'dense_X_csc':   formula_dense_csc,
    'csr_X_dense':   formula_csr_dense,
    'csr_X_csr':     formula_csr_csr,
    'csr_X_csc':     formula_csr_csc,
    'csc_X_dense':   formula_csc_dense,
    'csc_X_csr':     formula_csc_csr,
    'csc_X_csc':     formula_csc_csc,
}

csv_files = sys.argv[1:]

plt.figure(figsize=(24,5))

colors = plt.cm.tab10(np.linspace(0, 1, len(csv_files)))

for i, csv_file in enumerate(csv_files):
    data = pd.read_csv(csv_file)

    row_count      = data['row_count'].iloc[0]
    col_count      = data['col_count'].iloc[0]
    inner_count    = data['inner_count'].iloc[0]
    densities      = data['density'].values

    observed_flops  = data['flops'].values
    observed_memops = data['memops'].values

    time = data['time'].values
    byte = data['bytes'].values

    formula_func = formula_map.get(csv_file.removesuffix('.csv'), None)

    if not formula_func:
        print(f"No formula defined for file: {csv_file}")
        continue

    LRC = row_count
    LCC = inner_count
    RRC = inner_count
    RCC = col_count

    LNZ = data['left_non_zero_count'].values
    RNZ = data['right_non_zero_count'].values

    formula_flops, formula_memops = formula_func(LRC, LCC, RCC, LNZ, RNZ)

    # If we produce a single value and not a series
    if np.isscalar(formula_flops):
        formula_flops = np.full_like(densities, formula_flops)
    if np.isscalar(formula_memops):
        formula_memops = np.full_like(densities, formula_memops)

    plt.subplot(1,4,1)
    plt.plot(densities, observed_flops, 'o', label=f'{csv_file}: Observed',
             color=colors[i], markersize=4)
    plt.plot(densities, formula_flops, '--', label=f'{csv_file}: Formula',
             color=colors[i],)
    plt.xlabel('Density')
    plt.ylabel('Flops')
    plt.title('Flops')
    plt.grid(True)

    plt.subplot(1,4,2)
    plt.plot(densities, observed_memops, 'o', label=f'{csv_file}: Observed',
             color=colors[i], markersize=4)
    plt.plot(densities, formula_memops, '--', label=f'{csv_file}: Formula',
             color=colors[i])
    plt.xlabel('Density')
    plt.ylabel('Memops')
    plt.title('Memops')
    plt.grid(True)

    plt.subplot(1,4,3)
    plt.plot(densities, time, '-', label=f'{csv_file}: Time',
             color=colors[i], markersize=4)
    plt.xlabel('Density')
    plt.ylabel('Timestamp Cycles')
    plt.title('Runtime Performance')
    plt.grid(True)

    flops_per_byte = observed_flops / byte
    flops_per_cycle = observed_flops / time

    plt.subplot(1,4,4)
    plt.plot(flops_per_byte, flops_per_cycle, 'o-', color=colors[i], label=csv_file, markersize=4)
    # plt.xscale('log')
    # plt.yscale('log')
    plt.xlabel('Arithmetic Intensity (FLOP/byte)')
    plt.ylabel('FLOP/cycle')
    plt.title('Roofline')
    plt.grid(True)

handles, labels = plt.gca().get_legend_handles_labels()
plt.figlegend(handles, labels, loc='lower center', ncol=4, fontsize=7)

plt.tight_layout()
plt.savefig("plot.png")
plt.show()
