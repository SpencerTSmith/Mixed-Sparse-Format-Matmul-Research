import csv
import json
import os
import glob
import re

def density_to_bin(d):
    d = round(d, 2)
    if d <= 0.1:
        return 0
    elif d <= 0.7:
        return 1
    else:
        return 2

FORMAT_NAMES = {
    "dense": 0,
    "csr":   1,
    "csc":   2,
    "coo":   3,
}

# costs[fa][fb][bin_a][bin_b] = min time observed
costs = {}

def parse_folder_name(folder):
    match = re.search(r'sweep_(left|right)_fixed_(\d+\.\d+)', folder)
    if not match:
        return None, None
    sweep_side  = match.group(1)   # "left" or "right"
    fixed_density = float(match.group(2))
    return sweep_side, fixed_density

for csv_path in glob.glob("runtime_sweep/**/*.csv", recursive=True):
    filename = os.path.basename(csv_path).replace(".csv", "")
    if filename == "roofline":
        continue

    parts = filename.lower().split("_x_")

    folder = os.path.basename(os.path.dirname(csv_path))
    sweep_side, fixed_density = parse_folder_name(folder)

    fa = FORMAT_NAMES.get(parts[0])
    fb = FORMAT_NAMES.get(parts[1])

    with open(csv_path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            swept_density = float(row["density"])

            if sweep_side == "left":
                left_density  = swept_density
                right_density = fixed_density
            else:
                left_density  = fixed_density
                right_density = swept_density

            bin_a = density_to_bin(left_density)
            bin_b = density_to_bin(right_density)
            time  = int(row["time"])

            key = (fa, fb, bin_a, bin_b)
            if key not in costs or time < costs[key]:
                costs[key] = time

costs_serializable = {str(k): v for k, v in costs.items()}
with open("runtime_sweep/costs_cache.json", "w") as f:
    json.dump(costs_serializable, f, indent=2)
