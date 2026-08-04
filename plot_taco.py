# NOTE: Claude generated this.
import argparse
import csv
import glob
import os
import re
from collections import defaultdict

import matplotlib
matplotlib.use('Agg')  # non-interactive backend -- avoids loading a GUI
                        # toolkit (GTK/Qt/etc) entirely, which is what was
                        # segfaulting on exit. We only ever call savefig().
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

FILENAME_RE = re.compile(r'k(\d+)_([A-Za-z]+)_([A-Za-z]+)\.csv$')


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


def plot_metric(data, metric_idx, title, ylabel, ax, log_scale=True):
    all_vals = []
    all_ks = set()

    for combo, rows in sorted(data.items()):
        ks = [r[0] for r in rows]
        vals = [r[metric_idx] for r in rows]
        all_vals.extend(vals)
        all_ks.update(ks)

        # Treat 0 as "no measurement" rather than a real data point: replace
        # with NaN so matplotlib breaks the line (leaves a gap) instead of
        # drawing a segment through it, and skip it entirely for markers.
        line_vals = [v if v > 0 else float('nan') for v in vals]
        line, = ax.plot(ks, line_vals, label=combo)

        nz_ks = [k for k, v in zip(ks, vals) if v > 0]
        nz_vals = [v for v in vals if v > 0]
        ax.scatter(nz_ks, nz_vals, s=10, color=line.get_color(), zorder=3)

    ax.set_xlabel('k  (matrix dim = 2^k)')
    ax.set_ylabel(ylabel)
    ax.set_title(title)

    if log_scale:
        # symlog instead of log: handles 0 (and negatives) gracefully by
        # using a linear region near zero, then switching to log spacing
        # beyond it. Zero sits flat in that region instead of shooting to
        # -inf like it does with a pure log axis.
        positive_vals = [v for v in all_vals if v > 0]
        linthresh = min(positive_vals) if positive_vals else 1
        ax.set_yscale('symlog', linthresh=max(linthresh, 1e-9), linscale=0.3)
        ax.set_ylim(bottom=0)

    # k is discrete (2, 4, 8, ... via 2^k) -- only tick the actual measured
    # values so the axis never shows fractional k's.
    if all_ks:
        ax.set_xticks(sorted(all_ks))
    ax.xaxis.set_major_locator(MaxNLocator(integer=True))

    ax.grid(True, alpha=0.3)
    ax.legend(fontsize='small')

    ax.grid(True, alpha=0.3)
    ax.legend(fontsize='small')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_dir', help='Directory containing the k*_*.csv result files')
    parser.add_argument('-o', '--out', default='taco_bench.png', help='Output image path')
    parser.add_argument('--linear', action='store_true', help='Use linear y-axis instead of log')
    args = parser.parse_args()

    data = load_results(args.run_dir)
    if not data:
        raise SystemExit(f'No matching CSVs found in {args.run_dir}')

    fig, axes = plt.subplots(1, 3, figsize=(18, 5))

    plot_metric(data, 1, 'Time', 'cycles (check your timer units)', axes[0], log_scale=not args.linear)
    plot_metric(data, 2, 'Cache misses', 'count', axes[1], log_scale=not args.linear)
    plot_metric(data, 3, 'Branch mispredicts', 'count', axes[2], log_scale=not args.linear)

    fig.suptitle(f'TACO format benchmark: {os.path.basename(os.path.normpath(args.run_dir))}')
    fig.tight_layout()
    fig.savefig(args.out, dpi=150)
    print(f'Saved plot to {args.out}')


if __name__ == '__main__':
    main()
