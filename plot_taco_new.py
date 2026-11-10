# NOTE: Claude generated this.
"""Plot TACO sparse-format benchmark results, one image per CSV.

Usage:
    python3 plot_taco_new.py [--mode separate] [--linear] [-o OUT.png] FILE1.csv FILE2.csv ...

Input: one CSV per run (e.g. one per Kronecker graph) with columns
    k,function,time,faults,bytes,flops,memops,cache,branch,alloc,kron_nnz
`function` is the format combo (e.g. 'CSR_x_CSC'); `k` is the size exponent
(dim = 2^k). Extra columns are ignored.

Output (OUT = taco_bench.png by default):
    OUT_<name>.png        one image per CSV: six panels (time, cache misses, branch
                          mispredicts, page faults, allocations, cache misses/cycle)
    OUT_ranks_<name>.png  one image per CSV: all pairs ranked at every k, with slowdown vs the fastest
    OUT_best.png          fastest format pair per k, one row per CSV
    OUT_best_by_density.png, OUT_best_by_nnz_per_row.png, OUT_best_by_nnz.png
                          same, but x = density / nnz per row / input nnz
    OUT_slowdown_vs_scalar.png   all CSVs pooled: slowdown vs the best pair, against
                          density and against nnz per row
    OUT_fixed_pair_cost.png      all CSVs pooled: cost of always using one pair

--mode tiled (CSVs with k,function,alloc,kron_nnz,block_size,grid_size,time,...):
    OUT_tiled_<name>.png      per CSV: one heatmap per function, x = k, y = block size,
                              colour = time / best block at that k, star = best block
    OUT_tiled_best_block.png  best block size against k, one panel per function, one line per CSV
    OUT_tiled_relative.png    pooled: slowdown vs the best block size, against block size
    plus a printed table (best fixed block size and each block size's median slowdown)
<name> is the CSV filename with timestamps and node names (like
2026-11-4_0:6:36_c953) removed. Every figure is checked for overlapping text and
a warning is printed if any is found.
"""

import argparse
import csv
import math
import os
import re
from collections import defaultdict
from statistics import median

import matplotlib
matplotlib.use('Agg')
import matplotlib.colors
import matplotlib.lines
import matplotlib.ticker

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch, Rectangle
from matplotlib.ticker import FuncFormatter, MaxNLocator, NullFormatter

# Row layout used everywhere: (k, time, cache, branch, alloc, kron_nnz, faults).
K, TIME, CACHE, BRANCH, ALLOC, KRON, FAULTS = range(7)


# ---------------------------------------------------------------------------
# Loading and labels
# ---------------------------------------------------------------------------

def load_results(csv_path):
    """{ 'CSR_x_CSC': [(k, time, cache, branch, alloc, kron_nnz, faults), ...] }, sorted by k."""
    data = defaultdict(list)
    with open(csv_path, newline='') as f:
        for row in csv.DictReader(f):
            data[row['function']].append((
                int(row['k']),
                int(row['time']),
                int(row['cache']),
                int(row['branch']),
                int(row.get('alloc') or 0),
                int(row.get('kron_nnz') or 0),
                int(row.get('faults') or 0),
            ))
    for combo in data:
        data[combo].sort(key=lambda r: r[K])
    return data


# Timestamps and node names that end up in result filenames, e.g.
# AS-Newman_s2_2026-11-4_0:14:4_c953.csv  (unpadded date/time, then node name).
_TIMESTAMP_PATTERNS = [
    r'(?<!\d)\d{10,13}(?!\d)',                                              # unix epoch
    r'\d{4}-\d{1,2}-\d{1,2}(?:[T_ ]\d{1,2}[-:]\d{1,2}(?:[-:]\d{1,2})?)?',   # 2026-11-4 or 2026-11-4_0:6:36
    r'\d{8}(?:[_-]?\d{4,6})?',                                              # 20261108 or 20261108_143012
    r'(?<!\d)\d{1,2}[-:]\d{1,2}[-:]\d{1,2}(?!\d)',                          # 0:6:36 or 14-30-12
    r'(?<![A-Za-z0-9])c\d+(?![A-Za-z0-9])',                                 # node name like c953
]


def strip_timestamps(name):
    """Drops the extension, timestamps and node names. 'AS-Newman_s2_2026-11-4_0:14:4_c953.csv'
    becomes 'AS-Newman_s2'. If nothing is left, returns 'run'."""
    base, ext = os.path.splitext(name)
    if ext.lower() not in ('.csv', '.txt'):
        base = name
    for pat in _TIMESTAMP_PATTERNS:
        base = re.sub(pat, '', base)
    base = re.sub(r'[_\-\s]{2,}', '_', base).strip('_- .')
    return base if re.search(r'[A-Za-z0-9]', base) else 'run'


def clean_labels(names):
    """strip_timestamps on every name; numbers duplicates ('run #1', 'run #2')."""
    cleaned = [strip_timestamps(n) for n in names]
    seen = {}
    out = []
    for c in cleaned:
        if cleaned.count(c) > 1:
            seen[c] = seen.get(c, 0) + 1
            out.append(f'{c} #{seen[c]}')
        else:
            out.append(c)
    return out


def slug(label):
    return re.sub(r'[^A-Za-z0-9._-]+', '_', label).strip('_')


def build_color_map(all_datasets):
    """One color per format combo, the same in every figure."""
    combos = sorted({combo for data in all_datasets for combo in data})
    cycle = plt.rcParams['axes.prop_cycle'].by_key()['color']
    return {combo: cycle[i % len(cycle)] for i, combo in enumerate(combos)}


# ---------------------------------------------------------------------------
# Overlap check: every figure is checked before it is saved
# ---------------------------------------------------------------------------

def _text_boxes(fig):
    """(description, bbox) for every title, axis label, visible tick label and
    in-axes annotation. Legends are not included: they sit on top of data by design."""
    renderer = fig.canvas.get_renderer()
    items = []

    def add(desc, artist):
        if artist.get_visible() and artist.get_text().strip():
            items.append((desc, artist.get_window_extent(renderer)))

    for i, ax in enumerate(fig.axes):
        name = f'panel {i + 1}'
        add(f'{name} title', ax.title)
        add(f'{name} x label', ax.xaxis.label)
        add(f'{name} y label', ax.yaxis.label)
        for t in ax.texts:
            add(f'{name} annotation "{t.get_text()[:15]}"', t)
        for axis, lim, kind in ((ax.xaxis, ax.get_xlim(), 'x'), (ax.yaxis, ax.get_ylim(), 'y')):
            lo, hi = sorted(lim)
            for ticks in (axis.get_major_ticks(), axis.get_minor_ticks()):
                for tick in ticks:
                    if lo <= tick.get_loc() <= hi:
                        add(f'{name} {kind} tick "{tick.label1.get_text()}"', tick.label1)
    return items


def warn_if_text_overlaps(fig, name):
    """Prints a warning for any two pieces of text whose boxes overlap."""
    fig.canvas.draw()
    items = _text_boxes(fig)
    problems = []
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            a, b = items[i][1], items[j][1]
            if a.x0 < b.x1 - 1 and b.x0 < a.x1 - 1 and a.y0 < b.y1 - 1 and b.y0 < a.y1 - 1:
                problems.append((items[i][0], items[j][0]))
    if problems:
        print(f'WARNING: text overlaps in {name}:')
        for a, b in problems[:8]:
            print(f'    {a}  <->  {b}')
        if len(problems) > 8:
            print(f'    ... and {len(problems) - 8} more')
    return problems


def save(fig, path):
    warn_if_text_overlaps(fig, path)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f'Saved {path}')


def figure_legend(fig, color_map, combos):
    handles = [Patch(facecolor=color_map[c], label=c) for c in sorted(combos)]
    try:
        fig.legend(handles=handles, loc='outside lower center',
                   ncol=min(len(handles), 3), fontsize='small')
    except ValueError:  # older matplotlib without 'outside' placement
        fig.axes[0].legend(handles=handles, fontsize='small')


# ---------------------------------------------------------------------------
# One image per CSV
# ---------------------------------------------------------------------------

def _finish_axes(ax, title, ylabel):
    ax.set_title(title)
    ax.set_xlabel('k  (dim = 2^k)')
    ax.set_ylabel(ylabel)
    ax.xaxis.set_major_locator(MaxNLocator(integer=True, nbins=6))
    ax.yaxis.set_minor_formatter(NullFormatter())  # minor log labels crowd each other
    ax.grid(True, alpha=0.3)


def draw_metric(ax, data, color_map, idx, title, ylabel, log_scale):
    any_positive = False
    for combo, rows in sorted(data.items()):
        ks = [r[K] for r in rows]
        vals = [r[idx] if (r[idx] > 0 or not log_scale) else float('nan') for r in rows]
        any_positive = any_positive or any(v > 0 for v in vals if v == v)
        ax.plot(ks, vals, color=color_map[combo], linewidth=1.5)
        ax.scatter(ks, vals, s=10, color=color_map[combo], zorder=3)
    if log_scale and any_positive:
        ax.set_yscale('log')
    _finish_axes(ax, title, ylabel)


def draw_rate(ax, data, color_map, title, ylabel):
    """Cache misses per timestamp cycle."""
    for combo, rows in sorted(data.items()):
        pts = [(r[K], r[CACHE] / r[TIME]) for r in rows if r[TIME] > 0]
        if pts:
            ks, rates = zip(*pts)
            ax.plot(ks, rates, color=color_map[combo], linewidth=1.5)
            ax.scatter(ks, rates, s=10, color=color_map[combo], zorder=3)
    ax.set_ylim(bottom=0)
    _finish_axes(ax, title, ylabel)


def plot_one(data, color_map, log_scale):
    """Six panels for one CSV. Titles are just the metric names (the file name
    is in the image's file name and your LaTeX caption), so they stay short."""
    fig, axes = plt.subplots(3, 2, figsize=(7.0, 8.8), layout='constrained')
    flat = axes.ravel()
    panels = [(TIME, 'Time', 'timestamp cycles'),
              (CACHE, 'Cache misses', 'count'),
              (BRANCH, 'Branch mispredicts', 'count'),
              (FAULTS, 'Page faults', 'count'),
              (ALLOC, 'Allocations', 'bytes')]
    for ax, (idx, title, ylabel) in zip(flat, panels):
        draw_metric(ax, data, color_map, idx, title, ylabel, log_scale)
    draw_rate(flat[5], data, color_map, 'Cache misses / cycle', 'misses per cycle')
    figure_legend(fig, color_map, data.keys())
    return fig


# ---------------------------------------------------------------------------
# Fastest pair per k (one row per CSV)
# ---------------------------------------------------------------------------

def best_per_k(dataset):
    """{ k: [(combo, median time), ...] } fastest first, for one CSV."""
    pooled = defaultdict(lambda: defaultdict(list))
    for combo, rows in dataset.items():
        for r in rows:
            if r[TIME] > 0:
                pooled[r[K]][combo].append(r[TIME])
    return {k: sorted(((c, median(v)) for c, v in combos.items()), key=lambda x: x[1])
            for k, combos in sorted(pooled.items())}


def print_best_table(labels, datasets):
    for label, ds in zip(labels, datasets):
        print(f'\n== Fastest function per k: {label} ==')
        print(f'{"k":>3}  {"best":<16} {"time":>14}  {"runner-up":<16} {"slower by":>10}')
        for k, ranked in best_per_k(ds).items():
            best, t = ranked[0]
            if len(ranked) > 1:
                ru, ru_t = ranked[1]
                margin = f'{ru_t / t:.2f}x'
            else:
                ru, margin = '-', '-'
            print(f'{k:>3}  {best:<16} {t:>14,.0f}  {ru:<16} {margin:>10}')


def plot_best(labels, datasets, color_map):
    """One row per CSV, one cell per k, colored by the fastest pair; the number
    in each cell is how much slower the runner-up was."""
    rankings = [best_per_k(ds) for ds in datasets]
    all_ks = sorted({k for r in rankings for k in r})
    if not all_ks:
        return None
    k_idx = {k: i for i, k in enumerate(all_ks)}
    n = len(rankings)
    fig, ax = plt.subplots(figsize=(max(6.0, 0.5 * len(all_ks) + 3.5), 0.38 * n + 2.2),
                           layout='constrained')
    used = set()
    any_tie = False
    for row, ranking in enumerate(rankings):
        for k, ranked in ranking.items():
            combo, t = ranked[0]
            used.add(combo)
            tie = len(ranked) > 1 and ranked[1][1] / t < _TIE
            any_tie = any_tie or tie
            ax.add_patch(Rectangle((k_idx[k], row), 1, 1, facecolor=color_map[combo],
                                   edgecolor='white', alpha=0.3 if tie else 0.9))
            if len(ranked) > 1:
                ax.text(k_idx[k] + 0.5, row + 0.5, f'{ranked[1][1] / t:.1f}x',
                        ha='center', va='center', fontsize=6)
    ax.set_xlim(0, len(all_ks))
    ax.set_ylim(n, 0)
    ax.set_xticks([i + 0.5 for i in range(len(all_ks))])
    ax.set_xticklabels([str(k) for k in all_ks], fontsize=8)
    ax.set_yticks([i + 0.5 for i in range(n)])
    ax.set_yticklabels(labels, fontsize=8)
    ax.set_xlabel('k  (dim = 2^k)')
    ax.set_title('Fastest pair per k')
    handles = [Patch(facecolor=color_map[c], label=c) for c in sorted(used)]
    if any_tie:
        handles.append(Patch(facecolor='gray', alpha=0.3, label=f'faded: within {(_TIE - 1) * 100:.0f}% of runner-up'))
    fig.legend(handles=handles, loc='outside right upper', fontsize='small')
    return fig


def plot_ranks(label, data):
    """One graph: rows = format pairs (best on average at the top), columns = k. Colour = the
    pair's rank at that k (1 = fastest, bright); the number is how much slower it is than the
    fastest pair at that k (1.00x = the winner). Blank = no run."""
    times = defaultdict(lambda: defaultdict(list))
    for combo, rows in data.items():
        for r in rows:
            if r[TIME] > 0:
                times[r[K]][combo].append(r[TIME])
    per_k = {k: {c: median(v) for c, v in cm.items()} for k, cm in times.items()}
    ks = sorted(per_k)
    combos = sorted({c for cm in per_k.values() for c in cm})
    if not ks:
        return None
    rank = {}   # (combo, k) -> (rank, slowdown)
    for k, cm in per_k.items():
        order = sorted(cm, key=cm.get)
        for i, c in enumerate(order):
            rank[(c, k)] = (i + 1, cm[c] / cm[order[0]])
    avg_rank = {c: sum(rank[(c, k)][0] for k in ks if (c, k) in rank) /
                   max(1, sum(1 for k in ks if (c, k) in rank)) for c in combos}
    combos.sort(key=avg_rank.get)
    n = len(combos)
    cmap = plt.get_cmap('viridis')
    fig, ax = plt.subplots(figsize=(max(6.0, 0.45 * len(ks) + 3.0), 0.4 * n + 1.9),
                           layout='constrained')
    for row, c in enumerate(combos):
        for col, k in enumerate(ks):
            if (c, k) not in rank:
                continue
            rk, slow = rank[(c, k)]
            frac = 1.0 - (rk - 1) / max(1, n - 1)          # rank 1 -> 1.0 (bright)
            ax.add_patch(Rectangle((col, row), 1, 1, facecolor=cmap(frac), edgecolor='white'))
            txt = f'{slow:.2f}x' if slow < 10 else (f'{slow:.0f}x' if slow < 1000 else '>999x')
            ax.text(col + 0.5, row + 0.5, txt, ha='center', va='center', fontsize=6,
                    color='black' if frac > 0.5 else 'white')
    ax.set_xlim(0, len(ks))
    ax.set_ylim(n, 0)
    ax.set_xticks([i + 0.5 for i in range(len(ks))])
    ax.set_xticklabels([str(k) for k in ks], fontsize=8)
    ax.set_yticks([i + 0.5 for i in range(n)])
    ax.set_yticklabels(combos, fontsize=8)
    ax.set_xlabel('k  (dim = 2^k)')
    ax.set_title(f'{label}: pairs ranked at each k (bright = fastest)')
    return fig


def _row_order(datasets):
    """Row order for the heatmaps: sparsest base graph (fewest nnz per row at the smallest k) first."""
    keys = []
    for i, ds in enumerate(datasets):
        med = point_medians([ds], min_k=1)
        if med:
            _, k, nnz = min(med, key=lambda x: x[1])
            keys.append((nnz / float(2 ** k), i))
        else:
            keys.append((float('inf'), i))
    return [i for _, i in sorted(keys)]


def plot_best_by_scalar(labels, datasets, color_map, name, fn, bin_decades):
    """Like the fastest-pair-per-k figure, but the x axis is a different quantity, in log bins
    (e.g. density). Rows = graphs, sparsest base graph first; cell colour = the pair with the
    smallest average log time relative to the best in that bin; blank = no data.
    If colour changes line up in a vertical band, this quantity decides the winner."""
    order = _row_order(datasets)
    cells = {}  # (row, bin) -> {combo: [log slowdown, ...]}
    for row, i in enumerate(order):
        for (_, k, nnz), cm in point_medians([datasets[i]], min_k=1).items():
            b = math.floor(math.log10(fn(k, nnz)) / bin_decades)
            best = min(cm.values())
            d = cells.setdefault((row, b), {})
            for c, t in cm.items():
                d.setdefault(c, []).append(math.log(t / best))
    if not cells:
        return None
    b_lo = min(b for _, b in cells)
    b_hi = max(b for _, b in cells)
    n = len(order)
    fig, ax = plt.subplots(figsize=(max(7.0, 0.3 * (b_hi - b_lo + 1) + 4.5), 0.38 * n + 2.2),
                           layout='constrained')
    used = set()
    any_tie = False
    for (row, b), d in cells.items():
        # a pair missing at some point of the bin cannot win the bin
        n_pts = max(len(v) for v in d.values())
        full = {c: sum(v) / len(v) for c, v in d.items() if len(v) == n_pts}
        ranked = sorted(full, key=full.get)
        combo = ranked[0]
        used.add(combo)
        tie = len(ranked) > 1 and math.exp(full[ranked[1]] - full[combo]) < _TIE
        any_tie = any_tie or tie
        ax.add_patch(Rectangle((b - b_lo, row), 1, 1, facecolor=color_map[combo],
                               edgecolor='white', alpha=0.3 if tie else 0.9))
    ax.set_xlim(0, b_hi - b_lo + 1)
    ax.set_ylim(n, 0)
    step = max(1, int(round(1.0 / bin_decades)))  # label once per decade
    ticks = [b - b_lo for b in range(b_lo, b_hi + 1) if b % step == 0]
    ax.set_xticks(ticks)
    ax.set_xticklabels([f'1e{(t + b_lo) * bin_decades:.0f}' for t in ticks], fontsize=8)
    ax.set_yticks([i + 0.5 for i in range(n)])
    ax.set_yticklabels([labels[i] for i in order], fontsize=8)
    ax.set_xlabel(f'{name}  (log bins, {bin_decades:g} decade wide)')
    ax.set_title(f'Fastest pair by {name}')
    handles = [Patch(facecolor=color_map[c], label=c) for c in sorted(used)]
    if any_tie:
        handles.append(Patch(facecolor='gray', alpha=0.3, label=f'faded: within {(_TIE - 1) * 100:.0f}% of runner-up'))
    fig.legend(handles=handles, loc='outside right upper', fontsize='small')
    return fig


# ---------------------------------------------------------------------------
# Pooled analysis: does one scalar decide the winner, and what does it cost
# to always use one pair?  One "point" is one (CSV, k).
# ---------------------------------------------------------------------------

_ALL_PAIRS = False
_TIE = 1.03   # winner within this factor of the runner-up = near tie (drawn faded)
_MIN_K = 1   # set from --min-k: pooled analyses ignore smaller k (tiny runs are timer noise)


def point_medians(datasets, min_k=None):
    """{ (csv_index, k, kron_nnz): { combo: median time } } for time > 0 and known kron_nnz."""
    pooled = defaultdict(lambda: defaultdict(list))
    for i, data in enumerate(datasets):
        for combo, rows in data.items():
            for r in rows:
                if r[TIME] > 0 and r[KRON] > 0 and r[K] >= (_MIN_K if min_k is None else min_k):
                    pooled[(i, r[K], r[KRON])][combo].append(r[TIME])
    return {key: {c: median(v) for c, v in combos.items()} for key, combos in pooled.items()}


def _nnz(k, nnz):
    return nnz


def _density(k, nnz):
    return nnz / float(4 ** k)


def _nnz_per_row(k, nnz):
    return nnz / float(2 ** k)


SCALARS = {
    'k': lambda k, nnz: float(k),
    'nnz': lambda k, nnz: float(nnz),
    'density': _density,
    'nnz per row': _nnz_per_row,
}


def best_segmentation_accuracy(points, max_segments):
    """points: [(x, label)]. Cut the points, sorted by x, into at most max_segments
    runs, each predicting its most common label; return the best fraction correct
    (the accuracy of the best 'thresholds on one scalar' rule)."""
    pts = sorted(points, key=lambda p: p[0])
    n = len(pts)
    if n == 0:
        return float('nan')
    labels = sorted({l for _, l in pts})
    prefix = {l: [0] * (n + 1) for l in labels}
    for i, (_, l) in enumerate(pts):
        for m in labels:
            prefix[m][i + 1] = prefix[m][i] + (1 if m == l else 0)

    def seg_correct(j, i):
        return max(prefix[m][i] - prefix[m][j] for m in labels)

    dp = [[-1] * (n + 1) for _ in range(max_segments + 1)]
    dp[0][0] = 0
    for s in range(1, max_segments + 1):
        for i in range(1, n + 1):
            best = -1
            for j in range(i):
                if dp[s - 1][j] != -1:
                    best = max(best, dp[s - 1][j] + seg_correct(j, i))
            dp[s][i] = best
    return max(dp[s][n] for s in range(1, max_segments + 1)) / n


def count_transitions(points):
    pts = sorted(points, key=lambda p: p[0])
    return sum(1 for a, b in zip(pts, pts[1:]) if a[1] != b[1])


def print_separation_table(datasets):
    med = point_medians(datasets)
    winners = {key: min(c, key=c.get) for key, c in med.items() if c}
    if not winners:
        print('\nNo kron_nnz data found; skipping the separation table.')
        return
    n = len(winners)
    counts = defaultdict(int)
    for w in winners.values():
        counts[w] += 1
    print('\n== Does one scalar decide the winner? (all CSVs pooled) ==')
    print(f'{n} points (CSV, k); always guessing the most common winner is right '
          f'{max(counts.values()) / n:.0%} of the time.')
    print('Accuracy = best "thresholds on this one scalar" rule with at most N segments.')
    print(f'{"scalar":<12} {"winner changes":>15} {"3 segments":>12} {"4 segments":>12} {"6 segments":>12}')
    for name, fn in SCALARS.items():
        pts = [(fn(k, nnz), winners[(i, k, nnz)]) for (i, k, nnz) in winners]
        print(f'{name:<12} {count_transitions(pts):>15d} '
              f'{best_segmentation_accuracy(pts, 3):>12.0%} '
              f'{best_segmentation_accuracy(pts, 4):>12.0%} '
              f'{best_segmentation_accuracy(pts, 6):>12.0%}')


def best_rule_regret(points, max_segments):
    """points: [(x, {combo: time})]. Sort by x, cut into at most max_segments runs, and in
    each run always use one pair, chosen to minimise total log slowdown vs the best pair.
    Returns (mean slowdown, worst slowdown) of the best such rule."""
    pts = sorted(points, key=lambda p: p[0])
    n = len(pts)
    combos = sorted({c for _, cm in pts for c in cm})
    pre = {c: [0.0] for c in combos}
    for _, cm in pts:
        best = min(cm.values())
        for c in combos:
            pre[c].append(pre[c][-1] + (math.log(cm[c] / best) if c in cm else 1e9))
    def seg(j, i):
        return min(range(len(combos)), key=lambda m: pre[combos[m]][i] - pre[combos[m]][j]), \
               min(pre[c][i] - pre[c][j] for c in combos)
    INF = float('inf')
    dp = [[INF] * (n + 1) for _ in range(max_segments + 1)]
    back = [[None] * (n + 1) for _ in range(max_segments + 1)]
    dp[0][0] = 0.0
    for sg in range(1, max_segments + 1):
        for i in range(1, n + 1):
            for j in range(i):
                if dp[sg - 1][j] < INF:
                    v = dp[sg - 1][j] + seg(j, i)[1]
                    if v < dp[sg][i]:
                        dp[sg][i], back[sg][i] = v, j
    sg = min(range(1, max_segments + 1), key=lambda g: dp[g][n])
    i, worst = n, 1.0
    while sg > 0:
        j = back[sg][i]
        m, _ = seg(j, i)
        c = combos[m]
        for _, cm in pts[j:i]:
            worst = max(worst, cm[c] / min(cm.values()) if c in cm else INF)
        i, sg = j, sg - 1
    return math.exp(dp[min(range(1, max_segments + 1), key=lambda g: dp[g][n])][n] / n), worst


def print_rule_regret_table(datasets, max_segments=3):
    """The metric that matters: how much slower than the oracle is a threshold rule on one
    scalar (at most 3 segments), and how does that compare with one fixed pair?"""
    med = point_medians(datasets)
    med = {key: cm for key, cm in med.items() if len(cm) > 1}
    if not med:
        return
    print(f'\n== Cost of a {max_segments}-segment threshold rule (slowdown vs. always picking the best) ==')
    print(f'{"rule":<14} {"mean":>8} {"worst":>8}')
    for name, fn in SCALARS.items():
        pts = [(fn(k, nnz), cm) for (i, k, nnz), cm in med.items()]
        mean, worst = best_rule_regret(pts, max_segments)
        print(f'{name:<14} {mean:>7.3f}x {worst:>7.2f}x')
    pts = [(0.0, cm) for cm in med.values()]
    mean, worst = best_rule_regret(pts, 1)
    print(f'{"one fixed pair":<14} {mean:>7.3f}x {worst:>7.2f}x')


def _binned_median_line(xs, ys, n_bins=8):
    pts = sorted(zip(xs, ys))
    if len(pts) < 2:
        return [p[0] for p in pts], [p[1] for p in pts]
    n_bins = max(1, min(n_bins, len(pts) // 3))
    size = len(pts) / float(n_bins)
    bx, by = [], []
    for b in range(n_bins):
        chunk = pts[int(round(b * size)):int(round((b + 1) * size))]
        if chunk:
            bx.append(median([p[0] for p in chunk]))
            by.append(median([p[1] for p in chunk]))
    return bx, by


def plot_slowdown_vs_scalar(datasets, color_map):
    """Each dot = one input (a CSV at one k): how much slower a format pair is than the best
    pair there (1x = it wins). Left: against density. Right: against nnz per row. The panel
    where each pair's dots form a tidy curve is the better explanation. Lines = binned medians."""
    med = point_medians(datasets)
    if not med:
        return None
    combos = sorted({c for v in med.values() for c in v})
    if not _ALL_PAIRS:  # drop pairs that are hopeless (avg > 2x slower) so the good ones are readable
        def _gm(c):
            v = [cm[c] / min(cm.values()) for cm in med.values() if c in cm]
            return math.exp(sum(math.log(x) for x in v) / len(v))
        good = [c for c in combos if _gm(c) < 2.0]
        combos = good or combos
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.6), sharey=True, layout='constrained')
    ymax = 1.1
    for ax, (xname, xfn) in zip(axes, (('density', _density), ('nnz per row', _nnz_per_row))):
        for c in combos:
            xs, ys = [], []
            for (i, k, nnz), cm in med.items():
                if c in cm:
                    xs.append(xfn(k, nnz))
                    ys.append(cm[c] / min(cm.values()))
            if not xs:
                continue
            ymax = max(ymax, max(ys))
            ax.scatter(xs, ys, s=12, color=color_map[c], alpha=0.35, linewidths=0)
            bx, by = _binned_median_line(xs, ys)
            ax.plot(bx, by, color=color_map[c], linewidth=2.2)
        ax.set_xscale('log')
        ax.set_xlabel(xname)
        ax.set_title(f'Slowdown vs. {xname}')
        ax.grid(True, alpha=0.3)
    axes[0].set_ylabel('time / time of best pair\n(1x = this pair wins)')
    axes[0].set_yscale('log')
    span = math.log(ymax * 1.05)
    ticks = []
    for t in (1, 1.05, 1.1, 1.2, 1.3, 1.5, 2, 3, 5, 10, 20, 50, 100):
        if t <= ymax * 1.05 and (not ticks or math.log(t / ticks[-1]) >= span / 7):
            ticks.append(t)  # only keep ticks far enough apart that labels never touch
    axes[0].set_yticks(ticks)
    axes[0].yaxis.set_major_formatter(FuncFormatter(lambda v, _: f'{v:g}x'))
    axes[0].yaxis.set_minor_formatter(NullFormatter())
    axes[0].set_ylim(0.98, ymax * 1.05)
    fig.legend(handles=[Patch(facecolor=color_map[c], label=c) for c in combos],
               loc='outside right upper', fontsize='small')
    return fig


def plot_fixed_pair_cost(datasets, color_map):
    """If you always used one pair: bar = average slowdown vs the best pair, tick = worst case."""
    med = point_medians(datasets)
    if not med:
        return None
    combos = sorted({c for v in med.values() for c in v})
    gm, worst = [], []
    for c in combos:
        v = [cm[c] / min(cm.values()) for cm in med.values() if c in cm]
        gm.append(math.exp(sum(math.log(x) for x in v) / len(v)))
        worst.append(max(v))
    fig, ax = plt.subplots(figsize=(max(6.0, 1.3 * len(combos) + 1.5), 4.6),
                           layout='constrained')
    xs = list(range(len(combos)))
    ax.bar(xs, [g - 1 for g in gm], bottom=1, color=[color_map[c] for c in combos], alpha=0.8)
    ax.set_yscale('log')
    ax.scatter(xs, worst, marker='_', s=600, color='black', zorder=3)
    for x, g, w in zip(xs, gm, worst):
        ax.text(x, w * 1.1, f'avg {g:.2f}x\nworst {w:.1f}x', ha='center', va='bottom', fontsize=8)
    ax.axhline(1.0, color='gray', linewidth=0.8)
    ax.set_xticks(xs)
    ax.set_xticklabels(combos, rotation=30, ha='right', fontsize=8)
    ax.set_ylim(0.9, max(worst) * 2.5)
    ax.set_ylabel('slower than the best pair (log scale)')
    ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f'{v:g}x'))
    ax.yaxis.set_minor_formatter(NullFormatter())
    ax.set_title('Cost of always using one pair')
    ax.grid(True, alpha=0.3, axis='y')
    return fig


# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Tiled mode: block-size sweeps (CSV has block_size and grid_size columns)
# ---------------------------------------------------------------------------

def load_tiled(csv_path):
    """{function: {k: {block_size: (time, cache, branch, faults)}}}, medians over repeats.
    block_size 0 or empty marks an untiled baseline row."""
    raw = defaultdict(list)
    with open(csv_path, newline='') as f:
        reader = csv.DictReader(f)
        if 'block_size' not in (reader.fieldnames or []):
            raise SystemExit(f'{csv_path} has no block_size column; it is not a tiled CSV')
        for row in reader:
            key = (row['function'], int(row['k']), int(float(row.get('block_size') or 0)))
            raw[key].append((int(row['time']), int(row.get('cache') or 0),
                             int(row.get('branch') or 0), int(row.get('faults') or 0)))
    out = defaultdict(lambda: defaultdict(dict))
    for (fn, k, b), rows in raw.items():
        out[fn][k][b] = tuple(median(col) for col in zip(*rows))
    return out


def _fmt_block(b):
    """65536 -> '64K', 1048576 -> '1M' (labels stay short; exact value if not a power of 1024)."""
    b = int(b)
    if b >= 1 << 20 and b % (1 << 20) == 0:
        return f'{b >> 20}M'
    if b >= 1 << 10 and b % (1 << 10) == 0:
        return f'{b >> 10}K'
    return str(b)


def _block_ticks(ax, blocks, max_labels=5):
    blocks = sorted(blocks)
    ax.set_xscale('log', base=2)
    ax.set_xticks(blocks)
    step = max(1, math.ceil(len(blocks) / max_labels))
    ax.set_xticklabels([_fmt_block(b) if i % step == 0 else '' for i, b in enumerate(blocks)])
    ax.xaxis.set_minor_formatter(NullFormatter())
    ax.xaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    ax.yaxis.set_minor_formatter(matplotlib.ticker.LogFormatterSciNotation(minor_thresholds=(2, 0.5)))
    ax.grid(True, alpha=0.3)


_TILED_METRICS = {'time': 0, 'cache': 1, 'branch': 2}


def _tiled_fns(T):
    return sorted(fn for fn, by_k in T.items() if any(b > 0 for pts in by_k.values() for b in pts))


def plot_tiled_one(T, metric, min_k):
    """One heatmap per tiled function: x = k, y = block size, colour = metric / best over
    block sizes at that k (1 = best). A star marks the best block size at each k."""
    idx = _TILED_METRICS[metric]
    fns = _tiled_fns(T)
    fig, axes = plt.subplots(len(fns), 1, figsize=(7.5, 3.3 * len(fns) + 0.6),
                             layout='constrained', squeeze=False)
    mappable = None
    for ax, fn in zip(axes[:, 0], fns):
        ks = sorted(k for k in T[fn] if k >= min_k and any(b > 0 for b in T[fn][k]))
        blocks = sorted({b for k in ks for b in T[fn][k] if b > 0})
        if not ks or not blocks:
            ax.set_visible(False)
            continue
        grid = np.full((len(blocks), len(ks)), np.nan)
        for j, k in enumerate(ks):
            vals = {b: v[idx] for b, v in T[fn][k].items() if b > 0 and v[idx] > 0}
            if not vals:
                continue
            best = min(vals.values())
            for b, v in vals.items():
                grid[blocks.index(b), j] = v / best
            bi = blocks.index(min(vals, key=vals.get))
            ax.scatter([j], [bi], marker='*', s=70, color='white', edgecolor='black', zorder=3)
        mappable = ax.pcolormesh(np.arange(len(ks) + 1) - 0.5, np.arange(len(blocks) + 1) - 0.5,
                                 np.ma.masked_invalid(grid), cmap='viridis_r',
                                 norm=matplotlib.colors.LogNorm(vmin=1.0, vmax=3.0))
        ax.set_xticks(range(len(ks)))
        step = max(1, math.ceil(len(ks) / 12))
        ax.set_xticklabels([str(k) if i % step == 0 else '' for i, k in enumerate(ks)])
        ax.set_yticks(range(len(blocks)))
        ystep = max(1, math.ceil(len(blocks) / 8))
        ax.set_yticklabels([_fmt_block(b) if i % ystep == 0 else '' for i, b in enumerate(blocks)])
        ax.set_xlabel('k  (dim = 2^k)')
        ax.set_ylabel('block size')
        ax.set_title(fn)
    if mappable is not None:
        cb = fig.colorbar(mappable, ax=axes[:, 0].tolist(), shrink=0.8)
        cb.set_label(f'{metric} / best block at this k  (log scale; star = best)')
        cb.set_ticks([1, 1.5, 2, 3])
        cb.set_ticklabels(['1', '1.5', '2', '3+'])
    return fig


def _rel_to_best(T, min_k):
    """{function: {block: [time / best time over blocks, ...]}} over every (k >= min_k)."""
    rel = defaultdict(lambda: defaultdict(list))
    for fn, by_k in T.items():
        for k, pts in by_k.items():
            tiled = {b: v[0] for b, v in pts.items() if b > 0 and v[0] > 0}
            if k < min_k or len(tiled) < 2:
                continue
            best = min(tiled.values())
            for b, t in tiled.items():
                rel[fn][b].append(t / best)
    return rel


def plot_tiled_relative(Ts, color_map, min_k):
    """All CSVs pooled: slowdown vs the best block size, against block size."""
    pooled = defaultdict(lambda: defaultdict(list))
    for T in Ts:
        for fn, by_b in _rel_to_best(T, min_k).items():
            for b, xs in by_b.items():
                pooled[fn][b].extend(xs)
    if not pooled:
        return None
    fig, ax = plt.subplots(figsize=(7.0, 4.6), layout='constrained')
    blocks = set()
    for fn, by_b in sorted(pooled.items()):
        bs = sorted(by_b)
        blocks.update(bs)
        med = [median(by_b[b]) for b in bs]
        lo = [sorted(by_b[b])[len(by_b[b]) // 4] for b in bs]
        hi = [sorted(by_b[b])[(3 * len(by_b[b])) // 4] for b in bs]
        ax.plot(bs, med, color=color_map[fn], linewidth=1.8, marker='o', markersize=3)
        ax.fill_between(bs, lo, hi, color=color_map[fn], alpha=0.15, linewidth=0)
    ax.axhline(1.0, color='black', linewidth=0.8)
    ax.set_yscale('log')
    _block_ticks(ax, blocks)
    ax.set_xlabel('block size')
    ax.set_ylabel('time / best block size (same graph, k)')
    ax.set_title('Cost of a fixed block size (median, IQR band)')
    figure_legend(fig, color_map, pooled.keys())
    return fig


def plot_tiled_best_block(labels, Ts, metric, min_k):
    """Best block size against k, one panel per tiled function, one line per CSV."""
    idx = _TILED_METRICS[metric]
    fns = sorted({fn for T in Ts for fn in _tiled_fns(T)})
    if not fns:
        return None
    fig, axes = plt.subplots(len(fns), 1, figsize=(7.5, 3.6 * len(fns) + 0.6),
                             layout='constrained', squeeze=False)
    cycle = plt.get_cmap('tab20').colors
    any_pts = False
    for ax, fn in zip(axes[:, 0], fns):
        for i, (label, T) in enumerate(zip(labels, Ts)):
            pts = []
            for k, by_b in sorted(T.get(fn, {}).items()):
                vals = {b: v[idx] for b, v in by_b.items() if b > 0 and v[idx] > 0}
                if k >= min_k and vals:
                    pts.append((k, min(vals, key=vals.get)))
            if pts:
                any_pts = True
                ks, bs = zip(*pts)
                ax.plot(ks, bs, color=cycle[i % 20], marker='o', markersize=3, linewidth=1.2)
        ax.set_yscale('log', base=2)
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: _fmt_block(v)))
        ax.yaxis.set_minor_formatter(NullFormatter())
        ax.yaxis.set_minor_locator(matplotlib.ticker.NullLocator())
        ax.set_title(fn)
        ax.set_xlabel('k  (dim = 2^k)')
        ax.set_ylabel(f'best block size ({metric})')
        ax.xaxis.set_major_locator(MaxNLocator(integer=True, nbins=8))
        ax.grid(True, alpha=0.3)
    if not any_pts:
        plt.close(fig)
        return None
    handles = [matplotlib.lines.Line2D([], [], color=cycle[i % 20], label=l) for i, l in enumerate(labels)]
    try:
        fig.legend(handles=handles, loc='outside right', fontsize='small')
    except ValueError:
        axes[0][0].legend(handles=handles, fontsize='small')
    return fig


def print_tiled_table(labels, Ts, min_k):
    """Per function: best block size by median relative time, and the regret of each block size."""
    print('\nTiled: median slowdown vs the best block size at the same (graph, k)')
    pooled = defaultdict(lambda: defaultdict(list))
    for T in Ts:
        for fn, by_b in _rel_to_best(T, min_k).items():
            for b, xs in by_b.items():
                pooled[fn][b].extend(xs)
    for fn, by_b in sorted(pooled.items()):
        row = '  '.join(f'{b}:{median(xs):.3f}' for b, xs in sorted(by_b.items()))
        best_b = min(by_b, key=lambda b: median(by_b[b]))
        print(f'  {fn}: best fixed block = {best_b}   ({row})')


def run_tiled(args, labels, stem, ext):
    Ts = [load_tiled(p) for p in args.csv_files]
    for p, T in zip(args.csv_files, Ts):
        if not T:
            raise SystemExit(f'No usable rows found in {p}')
    for label, T in zip(labels, Ts):
        if not _tiled_fns(T):
            print(f'{label}: no rows with block_size > 0, skipped')
            continue
        save(plot_tiled_one(T, args.tiled_metric, args.min_k), f'{stem}_tiled_{slug(label)}{ext}')
    print_tiled_table(labels, Ts, args.min_k)
    color_map = build_color_map(Ts)
    for suffix, fig in (('_tiled_best_block', plot_tiled_best_block(labels, Ts, args.tiled_metric, args.min_k)),
                        ('_tiled_relative', plot_tiled_relative(Ts, color_map, args.min_k))):
        if fig is not None:
            save(fig, f'{stem}{suffix}{ext}')


def main():
    # allow_abbrev=False: an unknown option must be an error, never silently read
    # as an abbreviation of another option.
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('csv_files', nargs='+', help='Benchmark CSVs, one per run.')
    parser.add_argument('--mode', choices=['separate', 'tiled'], default='separate',
                        help="'separate' (default): one image per CSV. 'tiled': CSVs with block_size/grid_size columns, block-size sweeps.")
    parser.add_argument('--tiled-metric', choices=['time', 'cache', 'branch'], default='time',
                        help='tiled mode: metric used to pick the best block size and colour the heatmaps (default time).')
    parser.add_argument('--labels', nargs='+', default=None,
                        help='Names for the CSVs, in order (default: file names with timestamps removed).')
    parser.add_argument('-o', '--out', default='taco_bench.png',
                        help='Output path; images are written as OUT_<name>.png, OUT_best.png, etc.')
    parser.add_argument('--min-k', type=int, default=10,
                        help='Pooled analyses (tables and the last three images) ignore k below this (default 10: tiny runs are timer noise). Per-CSV images and the best-per-k figure show every k.')
    parser.add_argument('--tie', type=float, default=1.03,
                        help='Heatmap cells where the winner beats the runner-up by less than this factor are drawn faded (default 1.03).')
    parser.add_argument('--all-pairs', action='store_true', help='Show every pair in the slowdown plot, not just the good ones.')
    parser.add_argument('--linear', action='store_true', help='Linear y-axis instead of log.')
    args = parser.parse_args()

    if args.labels and len(args.labels) != len(args.csv_files):
        raise SystemExit('--labels must match the number of csv_files given')
    labels = clean_labels(args.labels or [os.path.basename(p) for p in args.csv_files])

    if args.mode == 'tiled':
        stem, ext = os.path.splitext(args.out)
        run_tiled(args, labels, stem, ext or '.png')
        return

    datasets = [load_results(p) for p in args.csv_files]
    for p, data in zip(args.csv_files, datasets):
        if not data:
            raise SystemExit(f'No usable rows found in {p}')
    color_map = build_color_map(datasets)
    log_scale = not args.linear
    global _MIN_K, _ALL_PAIRS, _TIE
    _MIN_K, _ALL_PAIRS, _TIE = args.min_k, args.all_pairs, args.tie

    stem, ext = os.path.splitext(args.out)
    ext = ext or '.png'

    # One image per CSV.
    for label, data in zip(labels, datasets):
        save(plot_one(data, color_map, log_scale), f'{stem}_{slug(label)}{ext}')

    # One rank figure per CSV.
    for label, data in zip(labels, datasets):
        fig = plot_ranks(label, data)
        if fig is not None:
            save(fig, f'{stem}_ranks_{slug(label)}{ext}')

    # Summaries across all CSVs.
    print_best_table(labels, datasets)
    print_separation_table(datasets)
    print_rule_regret_table(datasets)
    for suffix, fig in (('_best', plot_best(labels, datasets, color_map)),
                        ('_best_by_density', plot_best_by_scalar(
                            labels, datasets, color_map, 'density = nnz / 4^k', _density, 0.5)),
                        ('_best_by_nnz_per_row', plot_best_by_scalar(
                            labels, datasets, color_map, 'nnz per row', _nnz_per_row, 0.25)),
                        ('_best_by_nnz', plot_best_by_scalar(
                            labels, datasets, color_map, 'input nnz', _nnz, 0.25)),
                        ('_slowdown_vs_scalar', plot_slowdown_vs_scalar(datasets, color_map)),
                        ('_fixed_pair_cost', plot_fixed_pair_cost(datasets, color_map))):
        if fig is not None:
            save(fig, f'{stem}{suffix}{ext}')


if __name__ == '__main__':
    main()
