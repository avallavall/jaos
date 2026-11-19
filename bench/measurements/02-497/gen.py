# SPDX-License-Identifier: Apache-2.0
# Writes the degenerate LPs of 02-497: costs c = T * A'y* with y* zero on
# about 40% of the rows, so those rows' duals are zero at the optimum, and a
# right-hand side every row meets with equality at a planted point. T is
# 1e6 to 1e9 over 3, 7, 11 or 13. Each seed solved by JAOS from the command
# line; a status other than optimal, infeasible or unbounded keeps the file.
#
#   python3 gen.py OUT FIRST COUNT JAOS [ARGS...]
import os
import random
import subprocess
import sys


def write(path, seed):
    r = random.Random(seed)
    m = r.randint(3, 25)
    n = r.randint(m + 2, 3 * m + 10)
    a = [[r.choice([0, 0, 0, 1, 2, 3, 5, 7]) for _ in range(n)]
         for _ in range(m)]
    for j in range(n):
        if all(a[i][j] == 0 for i in range(m)):
            a[r.randrange(m)][j] = 1
    t = 10 ** r.uniform(6, 9) / r.choice([3, 7, 11, 13])
    y = [0.0 if r.random() < 0.4 else r.uniform(0.1, 1.0) for _ in range(m)]
    basic = [j for j in range(n) if r.random() < 0.6]
    c = []
    for j in range(n):
        v = sum(a[i][j] * y[i] for i in range(m)) * t
        if j not in basic:
            v += r.uniform(0.0, 1.0) * t * r.choice([0, 1e-3, 1])
        c.append(v)
    x = [r.choice([0, 0.5, 1, 1.5]) if j in basic else 0.0 for j in range(n)]
    b = [sum(a[i][j] * x[j] for j in range(n)) for i in range(m)]
    sense = [r.choice(["G", "G", "L", "E"]) if y[i] == 0 else "G"
             for i in range(m)]
    with open(path, "w") as f:
        f.write("NAME G\nROWS\n N obj\n")
        for i in range(m):
            f.write(f" {sense[i]} r{i}\n")
        f.write("COLUMNS\n")
        for j in range(n):
            f.write(f" x{j} obj {c[j]!r}\n")
            for i in range(m):
                if a[i][j]:
                    f.write(f" x{j} r{i} {a[i][j]}\n")
        f.write("RHS\n")
        for i in range(m):
            f.write(f" rhs r{i} {b[i]!r}\n")
        f.write("BOUNDS\n")
        for j in range(n):
            u = r.choice([2, 4, None])
            if u is not None:
                f.write(f" UP bnd x{j} {u!r}\n")
        f.write("ENDATA\n")


def main():
    out, first, count, jaos = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    os.makedirs(out, exist_ok=True)
    kept = 0
    for seed in range(first, first + count):
        path = f"{out}/h{seed}.mps"
        write(path, seed)
        res = subprocess.run([jaos, "solve", path, "--threads", "1"] + sys.argv[5:],
                             capture_output=True, text=True)
        st = [l.split()[1] for l in res.stdout.splitlines()
              if l.startswith("status ")]
        status = st[0] if st else "none"
        if status in ("optimal", "infeasible", "unbounded"):
            os.remove(path)
        else:
            kept += 1
            print(seed, status, flush=True)
    print("kept", kept, "of", count)


main()
