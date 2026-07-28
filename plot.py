import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os
import argparse

def formula_dense_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LRC * LCC * RCC
    memops = (2 * LRC * LCC * RCC) + (LRC * RCC)
    return flops, memops

def formula_dense_csr(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LRC * RNZ
    memops = (3 * LRC * LCC) + (4 * LRC * RNZ)
    return flops, memops

def formula_dense_csc(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LRC * RNZ
    memops = (3 * LRC * RCC) + (3 * LRC * RNZ)
    return flops, memops

def formula_dense_coo(LRC, LCC, RCC, LNZ, RNZ):
    flops  = 2 * LRC * RNZ
    memops = (2 * RNZ) + (LRC * RNZ) + (4 * LRC * RNZ)
    return flops, memops

def formula_csr_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LNZ * RCC
    memops = (2 * LRC) + (2 * LNZ) + (3 * LNZ * RCC)
    return flops, memops

def formula_csr_csr(LRC, LCC, RCC, LNZ, RNZ):
    # THE SAME!
    left_density = LNZ / (LRC * LCC)
    right_density = RNZ / (LCC * RCC)
    expected_work = left_density * right_density * LRC * RCC * LCC

    # THE SAME!
    # RNZ / RRC works for estimating the average non-zeroes in rows
    flops = (2 * LNZ * RNZ / LCC)
    memops = (2 * LRC) + (4 * LNZ) + (4 * LNZ * RNZ / LCC)
    return flops, memops

def formula_csr_csc(LRC, LCC, RCC, LNZ, RNZ):
    matches = LNZ * RNZ / LCC
    flops   = 2 * matches
    memops  = (2 * LRC) + (3 * LRC * RCC) + (LNZ * RCC) + (RNZ * LRC) + (2 * matches) + (LRC * RCC)
    return flops, memops

def formula_csr_coo(LRC, LCC, RCC, LNZ, RNZ):
    matches = LNZ * RNZ / LCC
    flops  = 2 * matches
    memops = (2 * LRC) + (LNZ) + (LRC * RNZ) + (4 * matches)
    return flops, memops

def formula_csc_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LNZ * RCC
    memops = (2 * LCC) + (2 * LNZ) + (3 * LNZ * RCC)
    return flops, memops

def formula_csc_csr(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 * LNZ * RNZ / LCC
    memops = (4 * LCC) + (2 * LNZ) + (4 * LNZ * RNZ / LCC)
    return flops, memops

def formula_csc_csc(LRC, LCC, RCC, LNZ, RNZ):
    flops = (2 * RNZ * LNZ / LCC)
    memops = (2 * RCC) + (4 * RNZ) + (4 * RNZ * LNZ / LCC)
    return flops, memops

def formula_csc_coo(LRC, LCC, RCC, LNZ, RNZ):
    RS      = np.minimum(LCC, RNZ)
    matches = LNZ * RNZ / LCC
    flops   = 2 * matches
    memops  = (2 * RNZ) + (2 * RS) + (4 * matches)
    return flops, memops

def formula_coo_dense(LRC, LCC, RCC, LNZ, RNZ):
    flops  = 2 * LNZ * RCC
    memops = (3 * LNZ) + (3 * LNZ * RCC)
    return flops, memops

def formula_coo_csr(LRC, LCC, RCC, LNZ, RNZ):
    matches = LNZ * RNZ / LCC
    flops   = 2 * matches
    memops  = (5 * LNZ) + (4 * matches)
    return flops, memops

def formula_coo_csc(LRC, LCC, RCC, LNZ, RNZ):
    LS      = np.minimum(LRC, LNZ)
    matches = LNZ * RNZ / LCC
    flops   = 2 * matches
    memops  = (LNZ) + (2 * RCC * LS) + (LNZ * RCC) + (RNZ * LS) + (2 * matches) + (LS * RCC)
    return flops, memops

def formula_coo_coo(LRC, LCC, RCC, LNZ, RNZ):
    LS      = np.minimum(LRC, LNZ)
    matches = LNZ * RNZ / LCC
    flops   = 2 * matches
    memops  = (LNZ) + (LNZ) + (RNZ * LS) + (LNZ) + (4 * matches)
    return flops, memops

FORMULA_MAP = {
    'dense_X_dense': formula_dense_dense,
    'dense_X_csr':   formula_dense_csr,
    'dense_X_csc':   formula_dense_csc,
    'dense_X_coo':   formula_dense_coo,
    'csr_X_dense':   formula_csr_dense,
    'csr_X_csr':     formula_csr_csr,
    'csr_X_csc':     formula_csr_csc,
    'csr_X_coo':     formula_csr_coo,
    'csc_X_dense':   formula_csc_dense,
    'csc_X_csr':     formula_csc_csr,
    'csc_X_csc':     formula_csc_csc,
    'csc_X_coo':     formula_csc_coo,
    'coo_X_dense':   formula_coo_dense,
    'coo_X_csr':     formula_coo_csr,
    'coo_X_csc':     formula_coo_csc,
    'coo_X_coo':     formula_coo_coo,
}

PLOT_TYPES = [
    'flops',
    'memops',
    'time',
    'flop_error',
    'memop_error',
    'cache_miss',
    'roofline',
]

def load_data(csv_file, formula_map):
    data = pd.read_csv(csv_file)
    basename = os.path.basename(csv_file).removesuffix('.csv')
    formula_func = formula_map.get(basename, None)

    lrc = data['row_count'].iloc[0]
    lcc = data['inner_count'].iloc[0]
    rcc = data['col_count'].iloc[0]
    lnz = data['left_non_zero_count'].values
    rnz = data['right_non_zero_count'].values

    formula_flops, formula_memops = (None, None)
    if formula_func:
        formula_flops, formula_memops = formula_func(lrc, lcc, rcc, lnz, rnz)
        densities = data['density'].values
        if np.isscalar(formula_flops):
            formula_flops = np.full_like(densities, formula_flops)
        if np.isscalar(formula_memops):
            formula_memops = np.full_like(densities, formula_memops)

    return dict(
        basename        = basename,
        densities       = data['density'].values,
        observed_flops  = data['flops'].values,
        observed_memops = data['memops'].values,
        time            = data['time'].values,
        byte            = data['bytes'].values,
        cache           = data['cache'].values,
        formula_flops   = formula_flops,
        formula_memops  = formula_memops,
        has_formula     = formula_func is not None,
    )

def plot_flops(ax, d, color):
    ax.plot(d['densities'], d['observed_flops'], 'o', label=f"{d['basename']}: Observed", color=color, markersize=4)
    if d['has_formula']:
        ax.plot(d['densities'], d['formula_flops'], '--', label=f"{d['basename']}: Formula", color=color)
    ax.set_xlabel('Density')
    ax.set_ylabel('Flops')
    ax.set_title('Flops')
    ax.grid(True)

def plot_memops(ax, d, color):
    ax.plot(d['densities'], d['observed_memops'], 'o', label=f"{d['basename']}: Observed", color=color, markersize=4)
    if d['has_formula']:
        ax.plot(d['densities'], d['formula_memops'], '--', label=f"{d['basename']}: Formula", color=color)
    ax.set_xlabel('Density')
    ax.set_ylabel('Memops')
    ax.set_title('Memops')
    ax.grid(True)

def plot_time(ax, d, color):
    ax.plot(d['densities'], d['time'], '-', label=d['basename'], color=color, markersize=4)
    ax.set_xlabel('Density')
    ax.set_ylabel('Timestamp Cycles')
    ax.set_title('Runtime Performance')
    ax.grid(True)

def plot_flop_error(ax, d, color):
    if not d['has_formula']:
        return
    err = (d['formula_flops'] - d['observed_flops']) / d['observed_flops']
    ax.plot(d['densities'], err, '--', label=d['basename'], color=color)
    ax.set_xlabel('Density')
    ax.set_ylabel('Relative Error')
    ax.set_title('Formula Flops Relative Error')
    ax.grid(True)

def plot_memop_error(ax, d, color):
    if not d['has_formula']:
        return
    err = (d['formula_memops'] - d['observed_memops']) / d['observed_memops']
    ax.plot(d['densities'], err, '--', label=d['basename'], color=color)
    ax.set_xlabel('Density')
    ax.set_ylabel('Relative Error')
    ax.set_title('Formula Memops Relative Error')
    ax.grid(True)

def plot_roofline(ax, d, color, peak_flops_per_cycle, peak_bytes_per_cycle):
    flops_per_byte  = d['observed_flops'] / d['byte']
    flops_per_cycle = d['observed_flops'] / d['time']
    ax.plot(flops_per_byte, flops_per_cycle, 'o-', color=color, label=d['basename'], markersize=4)
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlabel('Arithmetic Intensity (FLOP/byte)')
    ax.set_ylabel('FLOP/cycle')
    ax.set_title('Roofline')
    ax.grid(True)

def plot_cache_miss(ax, d, color):
    ax.plot(d['densities'], d['cache'], '-', label=d['basename'], color=color, markersize=4)
    ax.set_xlabel('Density')
    ax.set_ylabel('Cache Misses')
    ax.set_title('Cache Misses')
    ax.grid(True)

PLOT_FUNCTION_MAP = {
    'flops':       plot_flops,
    'memops':      plot_memops,
    'time':        plot_time,
    'flop_error':  plot_flop_error,
    'memop_error': plot_memop_error,
    'cache_miss':  plot_cache_miss,
}

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('csv_files', nargs='+')
    parser.add_argument('--plots', nargs='+', default=PLOT_TYPES)
    args = parser.parse_args()

    csv_files  = [f for f in args.csv_files if 'roofline' not in f]
    plot_types = args.plots

    peak_flops_per_cycle = 27.311
    peak_bytes_per_cycle = 30.041

    n_plots = len(plot_types)
    n_cols  = min(n_plots, 3)
    n_rows  = (n_plots + n_cols - 1) // n_cols

    fig, axes = plt.subplots(n_rows, n_cols, figsize=(7 * n_cols, 5 * n_rows))
    axes_flat = np.array(axes).flatten()

    for ax in axes_flat[n_plots:]:
        ax.set_visible(False)

    colors = [plt.cm.tab20(i / 20) for i in range(len(csv_files))]
    datasets = []

    for i, csv_file in enumerate(csv_files):
        formula_func = FORMULA_MAP.get(os.path.basename(csv_file).removesuffix('.csv'), None)
        if not formula_func:
            print(f"No formula defined for file: {csv_file}")

        d = load_data(csv_file, FORMULA_MAP)
        color = colors[i]
        datasets.append((d,color))

        for ax, plot_type in zip(axes_flat, plot_types):
            if plot_type == 'roofline':
                plot_roofline(ax, d, color, peak_flops_per_cycle, peak_bytes_per_cycle)
            else:
                PLOT_FUNCTION_MAP[plot_type](ax, d, color)

    if 'roofline' in plot_types:
        roofline_ax = axes_flat[plot_types.index('roofline')]
        roofline_ax.axhline(y=peak_flops_per_cycle, color='black', linestyle='--', label='Peak FLOP/cycle')
        x = np.logspace(-3, 3, 100)
        roofline_ax.plot(x, np.minimum(peak_bytes_per_cycle * x, peak_flops_per_cycle),
                         color='black', linestyle='-', label='Memory bound')

    # one legend entry per dataset using a colored line
    # plus a single shared observed/formula indicator pair
    legend_handles = [plt.Line2D([0], [0], color=c, marker='o', linestyle='-', markersize=4, label=d['basename'])
                      for d, c in datasets]
    legend_handles += [
        plt.Line2D([0], [0], color='gray', marker='o', linestyle='none', markersize=4, label='Observed'),
        plt.Line2D([0], [0], color='gray', linestyle='--',                              label='Formula'),
    ]

    fig.legend(handles=legend_handles,
               bbox_to_anchor=(0.5, -0.15),
               loc='lower center',
               ncol=min(6, len(legend_handles)),
               fontsize=8,
               framealpha=0.9)

    plt.tight_layout()
    plt.savefig("plot.png", bbox_inches="tight")
    plt.show()
