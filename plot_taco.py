# NOTE: Claude generated this.
import argparse
import csv
import glob
import os
import re
from collections import defaultdict

import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
from matplotlib.lines import Line2D

FILENAME_RE = re.compile(r'k(\d+)_([A-Za-z]+)_([A-Za-z]+)\.csv$')

# Cycle through these for each run directory, in order given on the CLI.
LINESTYLES = ['-', '--', ':', '-.']


def load_results(run_dir):
    """Returns { 'CSRxCSC': [(k, time, cache, branch), ...], ... }, sorted by k."""
    data = defaultdict(list)

    for path in glob.glob(os.path.join(run_dir, '*.csv')):
        match = FILENAME_RE.search(os.path.basename(path))
        if not match:
            continue

        k = int(match.group(1))
        a_fmt, b_fmt = match.group(2), match.group(3)
        combo = f'{a_fmt}x{b_fmt}'

        with open(path, newline='') as f:
            reader = csv.DictReader(f)
            row = next(reader, None)
            if row is None:
                continue
            data[combo].append((
                k,
                int(row['time']),
                int(row['cache']),
                int(row['branch']),
            ))

    for combo in data:
        data[combo].sort(key=lambda r: r[0])

    return data


def build_color_map(all_datasets):
    """Assign a consistent color per combo across all run directories, so
    e.g. CSRxCSC is the same color in every panel regardless of which
    directory it came from."""
    combos = sorted({combo for data in all_datasets for combo in data})
    cycle = plt.rcParams['axes.prop_cycle'].by_key()['color']
    return {combo: cycle[i % len(cycle)] for i, combo in enumerate(combos)}


def plot_metric(all_datasets, labels, color_map, metric_idx, title, ylabel, ax, log_scale=True):
    all_vals = []
    all_ks = set()

    for data, label, ls in zip(all_datasets, labels, LINESTYLES):
        for combo, rows in sorted(data.items()):
            ks = [r[0] for r in rows]
            vals = [r[metric_idx] for r in rows]
            all_vals.extend(vals)
            all_ks.update(ks)

            line_vals = [v if v > 0 else float('nan') for v in vals]
            color = color_map[combo]
            ax.plot(ks, line_vals, label=f'{combo} ({label})', color=color, linestyle=ls)

            nz_ks = [k for k, v in zip(ks, vals) if v > 0]
            nz_vals = [v for v in vals if v > 0]
            ax.scatter(nz_ks, nz_vals, s=10, color=color, zorder=3)

    ax.set_xlabel('k  (matrix dim = 2^k)')
    ax.set_ylabel(ylabel)
    ax.set_title(title)

    if log_scale:
        positive_vals = [v for v in all_vals if v > 0]
        linthresh = min(positive_vals) if positive_vals else 1
        ax.set_yscale('symlog', linthresh=max(linthresh, 1e-9), linscale=0.3)
        ax.set_ylim(bottom=0)

    if all_ks:
        ax.set_xticks(sorted(all_ks))
    ax.xaxis.set_major_locator(MaxNLocator(integer=True))

    ax.grid(True, alpha=0.3)


def plot_rate_metric(all_datasets, labels, color_map, numerator_idx, denominator_idx, title, ylabel, ax):
    """Plots a derived per-point ratio (e.g. cache misses / cycle) vs k.

    Floor is fixed at 0 (0 misses/cycle is a real, meaningful minimum);
    ceiling is left to auto-scale based on whatever the data actually
    shows, since that's effectively bounded by DRAM bandwidth rather than
    anything we'd want to clip or log-scale.
    """
    all_ks = set()

    for data, label, ls in zip(all_datasets, labels, LINESTYLES):
        for combo, rows in sorted(data.items()):
            ks, rates = [], []
            for r in rows:
                k, denom = r[0], r[denominator_idx]
                if denom > 0:  # skip points with 0 cycles measured -- undefined rate
                    ks.append(k)
                    rates.append(r[numerator_idx] / denom)

            all_ks.update(r[0] for r in rows)

            color = color_map[combo]
            ax.plot(ks, rates, label=f'{combo} ({label})', color=color, linestyle=ls)
            ax.scatter(ks, rates, s=10, color=color, zorder=3)

    ax.set_xlabel('k  (matrix dim = 2^k)')
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.set_ylim(bottom=0)

    if all_ks:
        ax.set_xticks(sorted(all_ks))
    ax.xaxis.set_major_locator(MaxNLocator(integer=True))

    ax.grid(True, alpha=0.3)


def build_legend(fig, color_map, labels):
    """Two-part legend: one entry per combo (color) and one per run dir
    (linestyle), instead of a combinatorial 'combo (label)' entry per line
    repeated in every panel."""
    combo_handles = [
        Line2D([0], [0], color=color, lw=2, label=combo)
        for combo, color in sorted(color_map.items())
    ]
    style_handles = [
        Line2D([0], [0], color='black', lw=2, linestyle=ls, label=label)
        for label, ls in zip(labels, LINESTYLES)
    ]
    fig.legend(
        handles=combo_handles + style_handles,
        loc='lower center',
        ncol=min(len(combo_handles) + len(style_handles), 8),
        fontsize='small',
        bbox_to_anchor=(0.5, -0.02),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_dirs', nargs='+',
                         help='One or more directories containing k*_*.csv result files. '
                              'Pass 2+ to overlay a comparison.')
    parser.add_argument('--labels', nargs='+', default=None,
                         help='Labels for each run_dir, in order (default: directory basenames).')
    parser.add_argument('-o', '--out', default='taco_bench.png', help='Output image path')
    parser.add_argument('--linear', action='store_true', help='Use linear y-axis instead of log')
    args = parser.parse_args()

    if args.labels and len(args.labels) != len(args.run_dirs):
        raise SystemExit('--labels must match the number of run_dirs given')
    labels = args.labels or [os.path.basename(os.path.normpath(d)) for d in args.run_dirs]

    all_datasets = [load_results(d) for d in args.run_dirs]
    for d, data in zip(args.run_dirs, all_datasets):
        if not data:
            raise SystemExit(f'No matching CSVs found in {d}')

    color_map = build_color_map(all_datasets)

    fig, axes = plt.subplots(1, 4, figsize=(24, 5))

    plot_metric(all_datasets, labels, color_map, 1, 'Time', 'timestamp cycles', axes[0], log_scale=not args.linear)
    plot_metric(all_datasets, labels, color_map, 2, 'Cache misses', 'count', axes[1], log_scale=not args.linear)
    plot_metric(all_datasets, labels, color_map, 3, 'Branch mispredicts', 'count', axes[2], log_scale=not args.linear)
    plot_rate_metric(all_datasets, labels, color_map, 2, 1, 'LLC misses / cycle', 'misses per cycle', axes[3])

    build_legend(fig, color_map, labels)

    title_dirs = ' vs '.join(labels)
    fig.suptitle(f'TACO format benchmark: {title_dirs}')
    fig.tight_layout(rect=[0, 0.06, 1, 1])
    fig.savefig(args.out, dpi=150, bbox_inches='tight')
    print(f'Saved plot to {args.out}')


if __name__ == '__main__':
    main()
