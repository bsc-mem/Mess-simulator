import os
import glob
import math
import shutil
import re

onChipLatency = 28.0

def round_to_even(val):
    # Get the nearest integer from normal rounding
    nearest_int = int(round(val))
    # If it's already even, return it
    if nearest_int % 2 == 0:
        return nearest_int
    # If it's odd, find the closest even number
    down = nearest_int - 1
    up = nearest_int + 1
    dist_down = abs(down - val)
    dist_up = abs(up - val)
    if dist_down <= dist_up:
        return down
    else:
        return up

files = glob.glob("bwlat_*.txt")

existing_indices = []
for f in files:
    name = os.path.basename(f)
    match = re.match(r"bwlat_(\d+)\.txt", name)
    if match:
        idx = int(match.group(1))
        existing_indices.append(idx)

existing_indices.sort()

def closest_index(target, sorted_list):
    closest = sorted_list[0]
    min_diff = abs(closest - target)
    for i in sorted_list[1:]:
        diff = abs(i - target)
        if diff < min_diff:
            min_diff = diff
            closest = i
    return closest

os.makedirs("out", exist_ok=True)

for f in files:
    name = os.path.basename(f)
    match = re.match(r"bwlat_(\d+)\.txt", name)
    if not match:
        continue
    X = int(match.group(1))
    # print(f"Processing {name} (X = {X})")
    if X <= 50:
        src = "bwlat_0.txt"
    else:
        # print(f"X = {X}")
        # val = 100.0 / (200 - X)
        val = 2.0 - (100/X)
        # print(val)
        # Y = int(round(100*val))
        Y = round_to_even(100*val)
        # print(Y)
        candidate = f"bwlat_{Y}.txt"
        if os.path.isfile(candidate):
            src = candidate
        else:
            # If exact Y not found, try using X itself if it exists
            self_candidate = f"bwlat_{X}.txt"
            if os.path.isfile(self_candidate):
                src = self_candidate
            else:
                c = closest_index(Y, existing_indices)
                src = f"bwlat_{c}.txt"

    dst = os.path.join("out", f"bwlat_{X}.txt")
    shutil.copy(src, dst)
    print(f"{dst} <- {src}")


    with open(dst, 'r') as file:
        lines = file.readlines()

    with open(dst, 'w') as file:
        for line in lines:
            parts = line.split()
            if len(parts) == 2:
                first_col = float(parts[0])
                second_col = float(parts[1]) - onChipLatency  # Subtracting a constant value (e.g., 10.0)
                file.write(f"{first_col} {second_col}\n")
            else:
                file.write(line)

