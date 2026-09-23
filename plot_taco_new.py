# NOTE: Claude generated this.
"""Plot TACO sparse-format benchmark results.

Expected input: one CSV file per run, with columns
    k,function,time,faults,bytes,flops,memops,cache,branch
where `function` is already the format combo (e.g. 'CSR_x_CSC') and `k` is
the matrix-size exponent (dim = 2^k). Only time/cache/branch are plotted;
faults/bytes/flops/memops are read but currently unused -- ignored rather
than errored on, so files can carry extra columns freely.
"""

import argparse
import csv
import os
from collections import defaultdict

import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
from matplotlib.lines import Line2D
from matplotlib.patches import Patch

# Cycle through these for each input file, in order given on the CLI.
# Only used in 'overlay' mode.
LINESTYLES = ['-', '--', ':', '-.']

# Tuple layout used everywhere below: (k, time, cache, branch).
K, TIME, CACHE, BRANCH = range(4)


def load_results(csv_path):
    """Returns { 'CSR_x_CSC': [(k, time, cache, branch), ...], ... }, sorted by k.

    Reads csv_path and groups its rows by the `function` column.
    """
    data = defaultdict(list)

    with open(csv_path, newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data[row['function']].append((
                int(row['k']),
                int(row['time']),
                int(row['cache']),
                int(row['branch']),
            ))

    for combo in data:
        data[combo].sort(key=lambda r: r[K])

    return data


def build_color_map(all_datasets):
    """Assign a consistent color per combo across all runs, so e.g.
    CSR_x_CSC is the same color in every panel/row regardless of which
    file it came from."""
    combos = sorted({combo for data in all_datasets for combo in data})
    cycle = plt.rcParams['axes.prop_cycle'].by_key()['color']
    return {combo: cycle[i % len(cycle)] for i, combo in enumerate(combos)}


def plot_metric(datasets, labels, color_map, metric_idx, title, ylabel, ax,
                 log_scale=True, linestyles=None, show_legend=True):
    """Plots one metric for one or more (dataset, label) pairs onto a single
    axes. linestyles=None means all datasets use a solid line (grid mode,
    where each dataset already has its own axes); pass a list to
    differentiate datasets sharing one axes (overlay mode)."""
    if linestyles is None:
        linestyles = ['-'] * len(datasets)

    all_vals = []
    all_ks = set()

    for data, label, ls in zip(datasets, labels, linestyles):
        for combo, rows in sorted(data.items()):
            ks = [r[K] for r in rows]
            vals = [r[metric_idx] for r in rows]
            all_vals.extend(vals)
            all_ks.update(ks)

            line_vals = [v if v > 0 else float('nan') for v in vals]
            color = color_map[combo]
            legend_label = combo if len(datasets) == 1 else f'{combo} ({label})'
            ax.plot(ks, line_vals, label=legend_label, color=color, linestyle=ls)

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
    if show_legend:
        ax.legend(fontsize='small')


def plot_rate_metric(datasets, labels, color_map, numerator_idx, denominator_idx,
                      title, ylabel, ax, linestyles=None, show_legend=True):
    """Plots a derived per-point ratio (e.g. cache misses / cycle) vs k.

    Floor is fixed at 0 (0 misses/cycle is a real, meaningful minimum);
    ceiling is left to auto-scale based on whatever the data actually
    shows, since that's effectively bounded by DRAM bandwidth rather than
    anything we'd want to clip or log-scale.
    """
    if linestyles is None:
        linestyles = ['-'] * len(datasets)

    all_ks = set()

    for data, label, ls in zip(datasets, labels, linestyles):
        for combo, rows in sorted(data.items()):
            ks, rates = [], []
            for r in rows:
                k, denom = r[K], r[denominator_idx]
                if denom > 0:  # skip points with 0 cycles measured -- undefined rate
                    ks.append(k)
                    rates.append(r[numerator_idx] / denom)

            all_ks.update(r[K] for r in rows)

            color = color_map[combo]
            legend_label = combo if len(datasets) == 1 else f'{combo} ({label})'
            ax.plot(ks, rates, label=legend_label, color=color, linestyle=ls)
            ax.scatter(ks, rates, s=10, color=color, zorder=3)

    ax.set_xlabel('k  (matrix dim = 2^k)')
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.set_ylim(bottom=0)

    if all_ks:
        ax.set_xticks(sorted(all_ks))
    ax.xaxis.set_major_locator(MaxNLocator(integer=True))

    ax.grid(True, alpha=0.3)
    if show_legend:
        ax.legend(fontsize='small')


def build_shared_legend(fig, color_map, labels):
    """Two-part legend for overlay mode: one entry per combo (color) and one
    per file (linestyle), instead of a combinatorial 'combo (label)'
    entry per line repeated in every panel."""
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


def plot_overlay(all_datasets, labels, color_map, log_scale):
    """One row of 4 panels; each dataset overlaid with a distinct linestyle,
    color shared per combo across datasets. Best for a small number of
    files where you want direct point-by-point comparison."""
    fig, axes = plt.subplots(1, 4, figsize=(24, 5))
    linestyles = LINESTYLES[:len(all_datasets)]

    plot_metric(all_datasets, labels, color_map, TIME, 'Time', 'timestamp cycles',
                axes[0], log_scale=log_scale, linestyles=linestyles, show_legend=False)
    plot_metric(all_datasets, labels, color_map, CACHE, 'Cache misses', 'count',
                axes[1], log_scale=log_scale, linestyles=linestyles, show_legend=False)
    plot_metric(all_datasets, labels, color_map, BRANCH, 'Branch mispredicts', 'count',
                axes[2], log_scale=log_scale, linestyles=linestyles, show_legend=False)
    plot_rate_metric(all_datasets, labels, color_map, CACHE, TIME, 'LLC misses / cycle',
                      'misses per cycle', axes[3], linestyles=linestyles, show_legend=False)

    build_shared_legend(fig, color_map, labels)
    fig.tight_layout(rect=[0, 0.06, 1, 1])
    return fig


def plot_grid(all_datasets, labels, color_map, log_scale):
    """One row per dataset, 4 metric columns. Each row is exactly the
    original single-run plot; easier to read when comparing more than 2-3
    files or when overlaid lines get too cluttered."""
    n = len(all_datasets)
    fig, axes = plt.subplots(n, 4, figsize=(24, 5 * n), squeeze=False)

    for row, (data, label) in enumerate(zip(all_datasets, labels)):
        d, l = [data], [label]
        plot_metric(d, l, color_map, TIME, f'Time — {label}', 'timestamp cycles',
                    axes[row][0], log_scale=log_scale)
        plot_metric(d, l, color_map, CACHE, f'Cache misses — {label}', 'count',
                    axes[row][1], log_scale=log_scale)
        plot_metric(d, l, color_map, BRANCH, f'Branch mispredicts — {label}', 'count',
                    axes[row][2], log_scale=log_scale)
        plot_rate_metric(d, l, color_map, CACHE, TIME, f'LLC misses / cycle — {label}',
                          'misses per cycle', axes[row][3])

    fig.tight_layout()
    return fig


# ---------------------------------------------------------------------------
# Violin mode: unlike overlay/grid, this treats every input file as a repeated
# *sample* of the same underlying dataset (e.g. the same benchmark re-run
# N times), not as a distinct dataset to compare side by side. Points that
# share a (combo, k) are pooled across all csv_files into one distribution,
# and each (combo, k) gets a single violin showing that spread. There's no
# per-file row/column and no --labels — the files are just
# replicates.
# ---------------------------------------------------------------------------

def aggregate_for_violin(all_datasets):
    """Pools raw metric values across all csv_files, keyed by combo then
    metric index then k. { combo: { metric_idx: { k: [values, ...] } } }"""
    agg = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    for data in all_datasets:
        for combo, rows in data.items():
            for row in rows:
                k = row[K]
                for metric_idx in (TIME, CACHE, BRANCH):
                    agg[combo][metric_idx][k].append(row[metric_idx])
    return agg


def aggregate_rate_for_violin(all_datasets, numerator_idx, denominator_idx):
    """Pools a derived per-point ratio across all csv_files, keyed by combo
    then k. { combo: { k: [rates, ...] } }"""
    agg = defaultdict(lambda: defaultdict(list))
    for data in all_datasets:
        for combo, rows in data.items():
            for row in rows:
                k, denom = row[K], row[denominator_idx]
                if denom > 0:  # skip points with 0 cycles measured -- undefined rate
                    agg[combo][k].append(row[numerator_idx] / denom)
    return agg


# How far apart adjacent k groups are placed on the x-axis, in "k units".
# 1.0 reproduces the old behavior (groups exactly 1 apart); raise it to give
# wide violins more breathing room between k values.
K_SPACING = 2.5


def _draw_violins(ax, combo_to_k_vals, color_map, all_vals_out):
    """Shared drawing logic: one violin per (combo, k), combos side-by-side
    at each k position so they don't overlap."""
    combos = sorted(combo_to_k_vals.keys())
    n_combos = len(combos)
    all_ks = sorted({k for combo in combos for k in combo_to_k_vals[combo]})
    if not all_ks:
        return combos

    # Violin widths scale with K_SPACING too, so they still comfortably fill
    # the (now-wider) gap between k groups instead of staying pinned to the
    # old 1-unit-wide spacing.
    width = 0.8 * K_SPACING / max(n_combos, 1)

    for i, combo in enumerate(combos):
        k_to_vals = combo_to_k_vals[combo]
        ks = sorted(k for k, vals in k_to_vals.items() if vals)
        if not ks:
            continue
        offset = (i - (n_combos - 1) / 2) * width
        positions = [k * K_SPACING + offset for k in ks]
        datasets = [k_to_vals[k] for k in ks]
        all_vals_out.extend(v for vals in datasets for v in vals)

        # violinplot chokes on a group with a single point (needs variance),
        # so those fall back to a plain scatter marker instead.
        multi_pos = [p for p, vals in zip(positions, datasets) if len(vals) > 1]
        multi_data = [vals for vals in datasets if len(vals) > 1]
        single_pos = [p for p, vals in zip(positions, datasets) if len(vals) == 1]
        single_vals = [vals[0] for vals in datasets if len(vals) == 1]

        color = color_map[combo]

        if multi_data:
            parts = ax.violinplot(multi_data, positions=multi_pos, widths=width * 3.0,
                                   showmeans=True, showextrema=True)
            for pc in parts['bodies']:
                pc.set_facecolor(color)
                pc.set_alpha(0.6)
                pc.set_edgecolor(color)
            for key in ('cmeans', 'cmins', 'cmaxes', 'cbars'):
                if key in parts:
                    parts[key].set_color(color)

        if single_vals:
            ax.scatter(single_pos, single_vals, s=14, color=color, zorder=3)

    if all_ks:
        # Ticks sit at the scaled positions, but labels still show the real
        # k values -- so the axis reads normally despite the wider spacing.
        ax.set_xticks([k * K_SPACING for k in all_ks])
        ax.set_xticklabels([str(k) for k in all_ks])
    return combos


def plot_violin_metric(agg, metric_idx, title, ylabel, ax, color_map, log_scale=True):
    combo_to_k_vals = {combo: agg[combo][metric_idx] for combo in agg}
    all_vals = []
    combos = _draw_violins(ax, combo_to_k_vals, color_map, all_vals)

    ax.set_xlabel('k  (matrix dim = 2^k)')
    ax.set_ylabel(ylabel)
    ax.set_title(title)

    if log_scale:
        positive_vals = [v for v in all_vals if v > 0]
        linthresh = min(positive_vals) if positive_vals else 1
        ax.set_yscale('symlog', linthresh=max(linthresh, 1e-9), linscale=0.3)
        ax.set_ylim(bottom=0)

    ax.grid(True, alpha=0.3)
    handles = [Patch(facecolor=color_map[c], edgecolor=color_map[c], alpha=0.6, label=c)
               for c in combos]
    if handles:
        ax.legend(handles=handles, fontsize='small')


def plot_violin_rate(agg, title, ylabel, ax, color_map):
    all_vals = []
    combos = _draw_violins(ax, agg, color_map, all_vals)

    ax.set_xlabel('k  (matrix dim = 2^k)')
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.set_ylim(bottom=0)

    ax.grid(True, alpha=0.3)
    handles = [Patch(facecolor=color_map[c], edgecolor=color_map[c], alpha=0.6, label=c)
               for c in combos]
    if handles:
        ax.legend(handles=handles, fontsize='small')


def plot_violin(all_datasets, color_map, log_scale):
    """One row of 4 panels, same layout as overlay mode, but every input file
    is pooled as a replicate of the same dataset instead of being kept
    distinct -- so each (combo, k) becomes a distribution rather than a
    single line."""
    fig, axes = plt.subplots(1, 4, figsize=(24, 5))

    agg = aggregate_for_violin(all_datasets)
    plot_violin_metric(agg, TIME, 'Time', 'timestamp cycles', axes[0], color_map, log_scale)
    plot_violin_metric(agg, CACHE, 'Cache misses', 'count', axes[1], color_map, log_scale)
    plot_violin_metric(agg, BRANCH, 'Branch mispredicts', 'count', axes[2], color_map, log_scale)

    rate_agg = aggregate_rate_for_violin(all_datasets, CACHE, TIME)
    plot_violin_rate(rate_agg, 'LLC misses / cycle', 'misses per cycle', axes[3], color_map)

    fig.tight_layout()
    return fig


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv_files', nargs='+',
                         help='One or more benchmark CSV files (columns: '
                              'k,function,time,...,cache,branch). Pass 2+ to compare '
                              '(or, in violin mode, to pool as repeated samples of the '
                              'same dataset).')
    parser.add_argument('--labels', nargs='+', default=None,
                         help='Labels for each csv_file, in order (default: file basenames). '
                              'Not used in violin mode, since csv_files are treated as replicates '
                              'of one dataset rather than distinct, individually-labeled datasets.')
    parser.add_argument('--mode', choices=['overlay', 'grid', 'violin'], default='overlay',
                         help="'overlay' (default): all datasets on shared axes, distinguished "
                              "by linestyle, treating each csv_file as a distinct dataset. "
                              "'grid': one row of panels per dataset, easier to read with many "
                              "files/combos. 'violin': treats ALL csv_files as repeated "
                              "samples of the SAME dataset -- pools points sharing a (combo, k) "
                              "across every file into one distribution per combo/k, instead "
                              "of a per-file row or line.")
    parser.add_argument('-o', '--out', default='taco_bench.png', help='Output image path')
    parser.add_argument('--linear', action='store_true', help='Use linear y-axis instead of log')
    args = parser.parse_args()

    if args.mode == 'violin':
        if args.labels:
            raise SystemExit('--labels is not used in violin mode (csv_files are pooled as '
                              'replicates, not labeled separately)')
    elif args.labels and len(args.labels) != len(args.csv_files):
        raise SystemExit('--labels must match the number of csv_files given')

    labels = args.labels or [os.path.basename(p) for p in args.csv_files]

    if args.mode == 'overlay' and len(args.csv_files) > len(LINESTYLES):
        raise SystemExit(
            f'overlay mode supports at most {len(LINESTYLES)} files '
            f'(got {len(args.csv_files)}) -- use --mode grid instead'
        )

    all_datasets = [load_results(p) for p in args.csv_files]
    for p, data in zip(args.csv_files, all_datasets):
        if not data:
            raise SystemExit(f'No usable rows found in {p}')

    color_map = build_color_map(all_datasets)
    log_scale = not args.linear

    if args.mode == 'overlay':
        fig = plot_overlay(all_datasets, labels, color_map, log_scale)
        title_dirs = ' vs '.join(labels)
        fig.suptitle(f'TACO format benchmark: {title_dirs}')
    elif args.mode == 'grid':
        fig = plot_grid(all_datasets, labels, color_map, log_scale)
        title_dirs = ' vs '.join(labels)
        fig.suptitle(f'TACO format benchmark: {title_dirs}')
    else:
        fig = plot_violin(all_datasets, color_map, log_scale)
        fig.suptitle(f'TACO format benchmark: {len(args.csv_files)} samples')

    fig.savefig(args.out, dpi=150, bbox_inches='tight')
    print(f'Saved plot to {args.out}')


if __name__ == '__main__':
    main()
