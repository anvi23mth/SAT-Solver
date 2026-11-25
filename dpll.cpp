// Simple DPLL SAT solver with MOMS branching heuristic.
//
// Build:  g++ -O2 -std=c++17 dpll.cpp -o dpll
// Run:    ./dpll file.cnf
//
// Input: DIMACS CNF   ("p cnf <vars> <clauses>", then clauses ending in 0)
// Output: SAT + assignment, or UNSAT
//
// DPLL = backtracking search + two simplifications:
//   1. Unit propagation : a clause with one unassigned literal (and the rest false)
//                         forces that literal to be true.
//   2. Branching        : pick an unassigned variable, try a value, recurse,
//                         and if it fails, try the opposite value.
// MOMS (Maximum Occurrences in clauses of Minimum Size) decides WHICH variable to
// branch on: look only at the shortest unsatisfied clauses, and pick the variable
// that appears most often there. Short clauses are "almost failing", so deciding
// their variables early causes conflicts (or propagation) sooner.

#include <bits/stdc++.h>
using namespace std;

int nVars, nClauses;
vector<vector<int>> clauses;
vector<int> val;          // val[v] = 1 (true), -1 (false), 0 (unassigned)
vector<int> trail;        // order in which variables were assigned (for undo)
long long decisions = 0, propagations = 0;

// value of a literal: 1 true, -1 false, 0 unassigned
int litValue(int lit) {
    int v = val[abs(lit)];
    return lit > 0 ? v : -v;
}

void assign(int lit) {
    val[abs(lit)] = lit > 0 ? 1 : -1;
    trail.push_back(abs(lit));
}

// Undo assignments until the trail has `size` entries
void backtrack(size_t size) {
    while (trail.size() > size) {
        val[trail.back()] = 0;
        trail.pop_back();
    }
}

// Repeatedly apply unit propagation. Returns false if some clause becomes all-false.
bool unitPropagate() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto &cl : clauses) {
            int unassigned = 0, lastLit = 0;
            bool satisfied = false;
            for (int lit : cl) {
                int lv = litValue(lit);
                if (lv == 1) { satisfied = true; break; }
                if (lv == 0) { unassigned++; lastLit = lit; }
            }
            if (satisfied) continue;
            if (unassigned == 0) return false;          // conflict
            if (unassigned == 1) {                      // unit clause
                assign(lastLit);
                propagations++;
                changed = true;
            }
        }
    }
    return true;
}

// MOMS: returns a literal to branch on, or 0 if every clause is satisfied.
int pickMOMS() {
    int minSize = INT_MAX;
    // Pass 1: find the size of the smallest unsatisfied clause (counting unassigned lits)
    vector<int> sizes(clauses.size(), -1);
    for (size_t i = 0; i < clauses.size(); i++) {
        int cnt = 0; bool sat = false;
        for (int lit : clauses[i]) {
            int lv = litValue(lit);
            if (lv == 1) { sat = true; break; }
            if (lv == 0) cnt++;
        }
        if (!sat) { sizes[i] = cnt; minSize = min(minSize, cnt); }
    }
    if (minSize == INT_MAX) return 0;   // all satisfied

    // Pass 2: count occurrences of each literal in those minimum-size clauses
    vector<int> pos(nVars + 1, 0), neg(nVars + 1, 0);
    for (size_t i = 0; i < clauses.size(); i++) {
        if (sizes[i] != minSize) continue;
        for (int lit : clauses[i])
            if (litValue(lit) == 0) (lit > 0 ? pos[lit] : neg[-lit])++;
    }

    // Score from the classic MOMS formula: (f(x)+f(-x))*2^k + f(x)*f(-x), k = 1 here
    long long best = -1; int bestLit = 0;
    for (int v = 1; v <= nVars; v++) {
        if (val[v] != 0) continue;
        long long score = (long long)(pos[v] + neg[v]) * 2 + (long long)pos[v] * neg[v];
        if (score > best) { best = score; bestLit = pos[v] >= neg[v] ? v : -v; }
    }
    return bestLit;
}

bool dpll() {
    size_t mark = trail.size();
    if (!unitPropagate()) { backtrack(mark); return false; }

    int lit = pickMOMS();
    if (lit == 0) return true;            // every clause satisfied -> SAT

    decisions++;
    size_t before = trail.size();
    for (int choice : {lit, -lit}) {      // try lit, then its negation
        assign(choice);
        if (dpll()) return true;
        backtrack(before);
    }
    backtrack(mark);
    return false;
}

int main(int argc, char **argv) {
    if (argc < 2) { cerr << "Usage: ./dpll file.cnf\n"; return 1; }
    ifstream in(argv[1]);
    string tok;
    while (in >> tok) {
        if (tok == "c") { getline(in, tok); continue; }   // comment
        if (tok == "p") { in >> tok >> nVars >> nClauses; break; }
    }
    int lit; vector<int> cur;
    while (in >> lit) {
        if (lit == 0) { clauses.push_back(cur); cur.clear(); }
        else cur.push_back(lit);
    }
    val.assign(nVars + 1, 0);

    auto t0 = chrono::steady_clock::now();
    bool sat = dpll();
    double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();

    cout << (sat ? "SAT" : "UNSAT") << "\n";
    if (sat) {
        for (int v = 1; v <= nVars; v++) cout << (val[v] == -1 ? -v : v) << " ";
        cout << "0\n";
    }
    cerr << "decisions=" << decisions << " propagations=" << propagations
         << " time=" << ms << " ms\n";
}
