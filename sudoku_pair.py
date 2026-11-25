"""
Pair Sudoku as a SAT problem (using PySAT).

Problem: two k^2 x k^2 Sudokus S1 and S2 must both be valid, AND for every
cell (r, c), S1[r][c] != S2[r][c].

Variable:  x(s, r, c, v)  is TRUE  <=>  in sudoku s, cell (r, c) holds value v
           s in {0,1}, r,c in [0, N), v in [1, N], with N = k*k

Usage:
    python sudoku_pair.py input.csv k            # solve
    python sudoku_pair.py input.csv k --compare  # minimal vs redundant encoding
    python sudoku_pair.py input.csv k --dimacs out.cnf   # export for the C++ solver

Input CSV: 2N rows, N columns. First N rows = S1, next N rows = S2. 0 = empty.
"""
import sys
import csv
import time
from itertools import combinations
from pysat.solvers import Glucose3


def var(s, r, c, v, N):
    """Map (s, r, c, v) to a unique positive integer (SAT variables start at 1)."""
    return s * N * N * N + r * N * N + c * N + (v - 1) + 1


def exactly_one(lits, clauses, redundant):
    """At-least-one is always added. At-most-one (pairwise) only if redundant=True."""
    clauses.append(list(lits))
    if redundant:
        for a, b in combinations(lits, 2):
            clauses.append([-a, -b])


def encode(grids, k, redundant=True):
    N = k * k
    clauses = []

    for s in range(2):
        for r in range(N):
            for c in range(N):
                # 1. Each cell has exactly one value  (this AMO is REQUIRED)
                cell = [var(s, r, c, v, N) for v in range(1, N + 1)]
                exactly_one(cell, clauses, redundant=True)

        for v in range(1, N + 1):
            # 2. Each row has value v exactly once
            for r in range(N):
                exactly_one([var(s, r, c, v, N) for c in range(N)], clauses, redundant)
            # 3. Each column has value v exactly once
            for c in range(N):
                exactly_one([var(s, r, c, v, N) for r in range(N)], clauses, redundant)
            # 4. Each k x k box has value v exactly once
            for br in range(0, N, k):
                for bc in range(0, N, k):
                    box = [var(s, br + i, bc + j, v, N) for i in range(k) for j in range(k)]
                    exactly_one(box, clauses, redundant)

    # 5. Pair constraint: same cell cannot have same value in both sudokus
    for r in range(N):
        for c in range(N):
            for v in range(1, N + 1):
                clauses.append([-var(0, r, c, v, N), -var(1, r, c, v, N)])

    # 6. Pre-filled cells become unit clauses
    for s in range(2):
        for r in range(N):
            for c in range(N):
                v = grids[s][r][c]
                if v != 0:
                    clauses.append([var(s, r, c, v, N)])
    return clauses


def decode(model, k):
    N = k * k
    true_vars = set(l for l in model if l > 0)
    out = [[[0] * N for _ in range(N)] for _ in range(2)]
    for s in range(2):
        for r in range(N):
            for c in range(N):
                for v in range(1, N + 1):
                    if var(s, r, c, v, N) in true_vars:
                        out[s][r][c] = v
    return out


def read_input(path, k):
    N = k * k
    with open(path) as f:
        rows = [list(map(int, row)) for row in csv.reader(f) if row]
    return [rows[:N], rows[N:2 * N]]


def solve(clauses):
    with Glucose3(bootstrap_with=clauses) as solver:
        t = time.time()
        ok = solver.solve()
        return ok, solver.get_model(), time.time() - t


def print_grid(g):
    for row in g:
        print(" ".join(f"{x:2d}" for x in row))


def main():
    path, k = sys.argv[1], int(sys.argv[2])
    grids = read_input(path, k)

    if "--compare" in sys.argv:
        for name, red in [("minimal  ", False), ("redundant", True)]:
            cl = encode(grids, k, redundant=red)
            ok, _, t = solve(cl)
            print(f"{name}: {len(cl):6d} clauses, SAT={ok}, time={t*1000:.2f} ms")
        return

    if "--decode" in sys.argv:
        # Read the C++ solver's output and print it as two grids
        out = open(sys.argv[sys.argv.index("--decode") + 1]).read().split()
        if out[0] != "SAT":
            print("None"); return
        s1, s2 = decode([int(x) for x in out[1:]], k)
        print("Sudoku 1:"); print_grid(s1)
        print("Sudoku 2:"); print_grid(s2)
        return

    clauses = encode(grids, k)

    if "--dimacs" in sys.argv:
        out = sys.argv[sys.argv.index("--dimacs") + 1]
        nvars = 2 * (k * k) ** 3
        with open(out, "w") as f:
            f.write(f"p cnf {nvars} {len(clauses)}\n")
            for cl in clauses:
                f.write(" ".join(map(str, cl)) + " 0\n")
        print(f"Wrote {out}: {nvars} vars, {len(clauses)} clauses")
        return

    ok, model, t = solve(clauses)
    if not ok:
        print("None  (no valid pair exists)")
        return
    s1, s2 = decode(model, k)
    print("Sudoku 1:"); print_grid(s1)
    print("Sudoku 2:"); print_grid(s2)
    print(f"Solved in {t*1000:.2f} ms with {len(clauses)} clauses")


if __name__ == "__main__":
    main()
