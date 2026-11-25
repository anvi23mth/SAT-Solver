# SAT Solver: Pair Sudoku + DPLL (MOMS)

Two small parts:

1. **`sudoku_pair.py`**: encodes *Pair Sudoku* as CNF and solves it with PySAT (Glucose3).
2. **`dpll.cpp`**: my own SAT solver in C++: DPLL + unit propagation + MOMS branching.

The Python script can also export the CNF (DIMACS), so the C++ solver can solve the same puzzle.

## Run it

```bash
pip install python-sat
g++ -O2 -std=c++17 dpll.cpp -o dpll

python3 sudoku_pair.py examples/k3.csv 3              # solve 9x9 pair with PySAT
python3 sudoku_pair.py examples/k3.csv 3 --compare    # minimal vs redundant encoding

python3 sudoku_pair.py examples/k2.csv 2 --dimacs k2.cnf   # export CNF
./dpll k2.cnf > k2.out                                     # solve with my solver
python3 sudoku_pair.py examples/k2.csv 2 --decode k2.out   # show as grids

./dpll examples/tiny_unsat.cnf                        # prints UNSAT
```

Input CSV: `2N` rows × `N` columns (`N = k²`). First N rows = Sudoku 1, next N = Sudoku 2, `0` = empty.

---

## Part 1: The encoding (explain this in 1 minute)

**Pair Sudoku:** two k²×k² Sudokus, both valid, and **cell (r,c) of S1 ≠ cell (r,c) of S2**.

**Variable:** `x(s,r,c,v)` = "sudoku s has value v at cell (r,c)". Total `2·N³` variables (1458 for 9×9).

| Constraint | Clauses |
|---|---|
| Each cell has ≥1 value | `x(s,r,c,1) ∨ … ∨ x(s,r,c,N)` |
| Each cell has ≤1 value | `¬x(s,r,c,a) ∨ ¬x(s,r,c,b)` for every pair a<b |
| Each value once per row / column / box | at-least-one clause (+ optional at-most-one pairs) |
| Pair constraint | `¬x(0,r,c,v) ∨ ¬x(1,r,c,v)` |
| Given clues | unit clause `x(s,r,c,v)` |

**"Exactly one" = at-least-one + at-most-one.** At-most-one uses the *pairwise* encoding: for every pair, "not both".

### Encoding redundancy (the "assess" part of the résumé line)

If every row has N cells and each cell holds exactly one value, then "each value appears **at least once** in the row" already forces "**at most once**" (pigeonhole). So the row/column/box at-most-one clauses are **logically redundant**.

Measured with `--compare` (9×9 example):

| Encoding | Clauses | Glucose time |
|---|---|---|
| Minimal | 7,239 | ~4,300 ms |
| Redundant | 24,735 | ~1 ms |

**Takeaway:** about 3.4× more clauses but thousands of times faster. Redundant clauses don't change the answer; they give the solver more **unit propagation**, so it detects conflicts immediately instead of finding them through deep search. (The cell's at-most-one is *not* redundant; without it a cell could hold two values.)

---

## Part 2: DPLL solver (explain this in 2 minutes)

```
DPLL(F):
    unit-propagate F          # forced assignments
    if some clause is all-false: return UNSAT (backtrack)
    if every clause satisfied:   return SAT
    x = pick variable (MOMS)
    return DPLL(F with x=true) or DPLL(F with x=false)
```

**Data:** `val[v] ∈ {1,-1,0}`, and a **trail** (stack of assigned variables). Backtracking = pop the trail back to a saved size and unassign those variables.

**Unit propagation:** a clause where every literal is false except one unassigned literal → that literal must be true. Repeat until nothing changes. If a clause has all literals false, that's a **conflict**.

**MOMS heuristic** (Maximum Occurrences in clauses of Minimum Size):
1. Among unsatisfied clauses, find the smallest size (count of unassigned literals).
2. Count how often each variable appears in those smallest clauses.
3. Score = `(f(x) + f(¬x))·2 + f(x)·f(¬x)` and branch on the highest.
4. Try the polarity that appears more often first.

*Why:* small clauses are closest to becoming unit or false. Branching on their variables triggers propagation and conflicts early, which prunes the search tree.

**Results:** 4×4 pair: 23 decisions, <1 ms. 9×9 pair: about 93k decisions, around 1 minute, versus ~1 ms for Glucose.

---

## Likely interview questions

**Q: Why is your solver so much slower than Glucose/MiniSat?**
They use **CDCL** (Conflict-Driven Clause Learning): on a conflict they analyse *why* it happened, learn a new clause so it never repeats, and **backjump** several levels instead of one. They also use **two watched literals** (only look at a clause when one of its two watched literals becomes false). My solver rescans every clause on each propagation step, so it is O(clauses) per step. Other extras: VSIDS heuristic and restarts.

**Q: What are watched literals?**
Each clause watches 2 unassigned/true literals. A clause can only become unit or false when a watched literal becomes false, so you only visit those clauses. Backtracking needs no updates.

**Q: Time complexity?** SAT is NP-complete, so the worst case is exponential, O(2ⁿ). Heuristics and propagation prune the tree in practice.

**Q: Why is Sudoku a good SAT problem?** All constraints are "exactly one" over finite choices, which maps directly to clauses.

**Q: Pairwise at-most-one has O(n²) clauses. Alternatives?** Sequential counter / ladder encoding: O(n) clauses with extra auxiliary variables. Fine to use pairwise here since n = 9.

**Q: What's pure literal elimination?** If a variable appears with only one polarity, set it that way. It is part of the original DPLL; I skipped it because it's expensive to check and gives little benefit with good propagation.

**Q: How do you verify the answer?** Decode the model back into grids and check rows, columns, boxes and the pair condition. A SAT model can also be checked against every clause in linear time.

**Q: DIMACS format?** `p cnf <vars> <clauses>`, then each clause is a line of integers ending in `0`; negative = negated variable.

**Q: How is the variable numbered?** `s·N³ + r·N² + c·N + (v−1) + 1` (a unique integer starting at 1, as DIMACS requires).
